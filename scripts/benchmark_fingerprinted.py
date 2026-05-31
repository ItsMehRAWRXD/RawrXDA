#!/usr/bin/env python3
"""
Fingerprinted Throughput + Loader Activation Benchmark
Synthetic benchmark that models:
1) Model size + quantization throughput
2) Loader/streaming method behavior
3) Concurrent activation limits when multiple methods run at once
"""

import argparse
import json
import random
import sys
from datetime import UTC, datetime
from itertools import combinations, islice
from typing import Dict, List

# Hardware baseline TPS for 7B model at each quantization family.
HARDWARE_PROFILE = {
    "fp32": 9.8,
    "fp16": 15.2,
    "q8": 28.5,
    "q6": 42.3,
    "q5": 58.7,
    "q4": 89.2,
    "q3": 124.5,
    "q2": 185.3,
    "q1": 225.0,
}

# Quant aliases/modes mapped to family + relative multiplier.
QUANT_PROFILES = {
    "fp32": {"family": "fp32", "mult": 1.00},
    "fp16": {"family": "fp16", "mult": 1.00},
    "q8": {"family": "q8", "mult": 1.00},
    "q8_0": {"family": "q8", "mult": 1.03},
    "q8_k": {"family": "q8", "mult": 1.06},
    "q6": {"family": "q6", "mult": 1.00},
    "q6_k": {"family": "q6", "mult": 1.07},
    "q5": {"family": "q5", "mult": 1.00},
    "q5_k_m": {"family": "q5", "mult": 1.10},
    "q5_k_s": {"family": "q5", "mult": 1.06},
    "q4": {"family": "q4", "mult": 1.00},
    "q4_k_m": {"family": "q4", "mult": 1.12},
    "q4_k_s": {"family": "q4", "mult": 1.08},
    "q3": {"family": "q3", "mult": 1.00},
    "q3_k_m": {"family": "q3", "mult": 1.10},
    "q3_k_s": {"family": "q3", "mult": 1.05},
    "q2": {"family": "q2", "mult": 1.00},
    "q2_k": {"family": "q2", "mult": 1.08},
    "q1": {"family": "q1", "mult": 1.00},
    "q1_xs": {"family": "q1", "mult": 1.05},
}

ALL_Q_TYPES = ",".join(QUANT_PROFILES.keys())

# Method fingerprints approximate behavior of loader/streaming paths.
# tps_mult: steady-state decode throughput multiplier
# load_ms_per_gb: approximate load/open latency cost
# mem_working_set_gb: additional per-active-method working set
METHOD_PROFILES = {
    "gguf_loader_whole_zone": {
        "label": "GGUFLoader whole-zone",
        "tps_mult": 1.00,
        "load_ms_per_gb": 110.0,
        "mem_working_set_gb": 1.20,
    },
    "streaming_zone_disk": {
        "label": "StreamingGGUF zone from disk",
        "tps_mult": 0.93,
        "load_ms_per_gb": 80.0,
        "mem_working_set_gb": 0.90,
    },
    "mapped_window_streamer": {
        "label": "MappedWindowStreamer (MapViewOfFile)",
        "tps_mult": 0.90,
        "load_ms_per_gb": 52.0,
        "mem_working_set_gb": 0.30,
    },
    "streaming_zone_mmap_fallback": {
        "label": "StreamingGGUF LoadZoneMapped fallback",
        "tps_mult": 0.88,
        "load_ms_per_gb": 56.0,
        "mem_working_set_gb": 0.35,
    },
    "chunked_file_loader": {
        "label": "ChunkedFileLoader/GGUFChunkedLoader",
        "tps_mult": 0.95,
        "load_ms_per_gb": 68.0,
        "mem_working_set_gb": 0.60,
    },
    "iouring_zone_loader": {
        "label": "IORingZoneLoader",
        "tps_mult": 1.07,
        "load_ms_per_gb": 44.0,
        "mem_working_set_gb": 0.45,
    },
    "titan_dual800b_kernel": {
        "label": "Titan Dual-800B Kernel Loader",
        "tps_mult": 1.42,
        "load_ms_per_gb": 38.0,
        "mem_working_set_gb": 0.70,
    },
    "aperture_predictive_prefetch": {
        "label": "Aperture Predictive Prefetch Loader",
        "tps_mult": 1.24,
        "load_ms_per_gb": 34.0,
        "mem_working_set_gb": 0.65,
    },
    "self_correcting_loader": {
        "label": "Self-Correcting Loader (adaptive)",
        "tps_mult": 1.31,
        "load_ms_per_gb": 42.0,
        "mem_working_set_gb": 0.85,
    },
}

# Compression profiles model CPU cost and memory pressure for each codec path.
COMPRESSION_PROFILES = {
    "none": {
        "label": "No compression",
        "tps_mult": 1.00,
        "load_ms_mult": 1.00,
        "extra_working_set_gb": 0.00,
    },
    "masm_brutal": {
        "label": "MASM brutal deflate",
        "tps_mult": 0.96,
        "load_ms_mult": 1.18,
        "extra_working_set_gb": 0.22,
    },
    "custom_zlib_masm": {
        "label": "CustomZlibCompress (MASM)",
        "tps_mult": 0.94,
        "load_ms_mult": 1.22,
        "extra_working_set_gb": 0.26,
    },
    "deflate_cpu": {
        "label": "CPU deflate",
        "tps_mult": 0.91,
        "load_ms_mult": 1.27,
        "extra_working_set_gb": 0.28,
    },
    "gdeflate_stream": {
        "label": "GDEFLATE stream path",
        "tps_mult": 1.02,
        "load_ms_mult": 1.10,
        "extra_working_set_gb": 0.18,
    },
    "masm_brutal_hyper": {
        "label": "MASM brutal hyper-pack",
        "tps_mult": 1.08,
        "load_ms_mult": 1.08,
        "extra_working_set_gb": 0.24,
    },
    "adaptive_hybrid_codec": {
        "label": "Adaptive Hybrid Codec",
        "tps_mult": 1.11,
        "load_ms_mult": 1.05,
        "extra_working_set_gb": 0.20,
    },
    "delta_tensor_pack": {
        "label": "Delta Tensor Pack",
        "tps_mult": 1.06,
        "load_ms_mult": 1.09,
        "extra_working_set_gb": 0.16,
    },
}

# Model scaling degradation by size (extended to 2T class synthetic models).
SCALING_PROFILE = [
    (7, 1.0),       # baseline
    (13, 0.89),     # -11%
    (34, 0.68),     # -32%
    (70, 0.35),     # -65%
    (120, 0.18),    # -82%
    (400, 0.11),
    (800, 0.08),
    (1200, 0.06),
    (1600, 0.045),
    (2000, 0.035),
]

def get_scaling_factor(model_size_b: int) -> float:
    """Linear interpolation for scaling factor at given model size"""
    
    # Exact match
    for size, factor in SCALING_PROFILE:
        if size == model_size_b:
            return factor
    
    # Find bounding points
    below = [(s, f) for s, f in SCALING_PROFILE if s <= model_size_b]
    above = [(s, f) for s, f in SCALING_PROFILE if s > model_size_b]
    
    if not below:
        return 1.0
    if not above:
        return 0.18
    
    x1, y1 = below[-1]
    x2, y2 = above[0]
    
    ratio = (model_size_b - x1) / (x2 - x1)
    return y1 + (y2 - y1) * ratio


def titan_kernel_boost(model_size_b: int) -> float:
    """Synthetic boost reflecting Titan dual-800B class kernel scaling."""
    if model_size_b < 400:
        return 1.0
    if model_size_b >= 1600:
        return 3.5
    # Linearly ramp 1.0 -> 3.5 from 400B to 1600B
    return 1.0 + ((model_size_b - 400) / 1200.0) * 2.5

def run_fingerprint(model_size_b: int, quantization: str, method: str, compression: str, target_tps_floor: float) -> Dict:
    """Run fingerprinted prediction for size + quant + loading method + compression."""
    
    if quantization not in QUANT_PROFILES:
        raise ValueError(f"Unknown quantization: {quantization}")
    if method not in METHOD_PROFILES:
        raise ValueError(f"Unknown method: {method}")
    if compression not in COMPRESSION_PROFILES:
        raise ValueError(f"Unknown compression: {compression}")
    
    quant_profile = QUANT_PROFILES[quantization]
    baseline_tps = HARDWARE_PROFILE[quant_profile["family"]] * quant_profile["mult"]
    scaling_factor = get_scaling_factor(model_size_b)
    method_profile = METHOD_PROFILES[method]
    compression_profile = COMPRESSION_PROFILES[compression]
    predicted_tps = (
        baseline_tps
        * scaling_factor
        * method_profile["tps_mult"]
        * compression_profile["tps_mult"]
    )

    # Titan kernel uplift on large models.
    if method in ("titan_dual800b_kernel", "self_correcting_loader"):
        predicted_tps *= titan_kernel_boost(model_size_b)
    
    # Add minor noise ±2% for realism
    noise = random.uniform(-2, 2) / 100
    measured_tps = predicted_tps * (1 + noise)

    corrected = False
    correction_boost = 1.0
    if method == "self_correcting_loader" and measured_tps < target_tps_floor:
        corrected = True
        correction_boost = min(4.2, target_tps_floor / max(measured_tps, 0.001))
        measured_tps = measured_tps * correction_boost
    efficiency = measured_tps / model_size_b
    approx_load_ms = (
        model_size_b
        * method_profile["load_ms_per_gb"]
        * compression_profile["load_ms_mult"]
        * 0.75
    )
    
    return {
        "model_size_b": model_size_b,
        "quantization": quantization,
        "quant_family": quant_profile["family"],
        "method": method,
        "method_label": method_profile["label"],
        "compression": compression,
        "compression_label": compression_profile["label"],
        "predicted_tps": round(predicted_tps, 2),
        "measured_tps": round(measured_tps, 2),
        "scaling_factor": round(scaling_factor, 3),
        "efficiency": round(efficiency, 3),  # TPS per billion params
        "approx_load_ms": round(approx_load_ms, 2),
        "method_working_set_gb": round(
            method_profile["mem_working_set_gb"] + compression_profile["extra_working_set_gb"], 3
        ),
        "self_corrected": corrected,
        "correction_boost": round(correction_boost, 3),
    }


def activation_factor(active_count: int) -> float:
    """Concurrency contention factor as more methods are active."""
    if active_count <= 1:
        return 1.0
    return max(0.58, 1.0 - (0.085 * (active_count - 1)))


def run_activation_sweep(
    model_size_b: int,
    quantization: str,
    methods: List[str],
    compressions: List[str],
    max_combinations_per_width: int,
    target_tps_floor: float,
) -> List[Dict]:
    """Estimate throughput and memory when activating multiple methods simultaneously."""
    lane_ids = [f"{m}+{c}" for m in methods for c in compressions]
    single_method = {}
    for lane in lane_ids:
        method, compression = lane.split("+", 1)
        base = run_fingerprint(model_size_b, quantization, method, compression, target_tps_floor)
        single_method[lane] = base

    activation_rows: List[Dict] = []
    for width in range(1, len(lane_ids) + 1):
        combo_iter = combinations(lane_ids, width)
        for combo in islice(combo_iter, max_combinations_per_width):
            total_base_tps = sum(single_method[m]["measured_tps"] for m in combo)
            total_mem_gb = sum(single_method[m]["method_working_set_gb"] for m in combo)
            cont = activation_factor(width)
            # Extra pressure penalty if aggregate working set gets large.
            mem_penalty = 1.0
            if total_mem_gb > 3.0:
                mem_penalty = max(0.72, 1.0 - ((total_mem_gb - 3.0) * 0.05))

            effective_tps = total_base_tps * cont * mem_penalty
            activation_rows.append(
                {
                    "model_size_b": model_size_b,
                    "quantization": quantization,
                    "active_count": width,
                    "lanes": list(combo),
                    "aggregate_base_tps": round(total_base_tps, 2),
                    "contention_factor": round(cont, 3),
                    "memory_penalty_factor": round(mem_penalty, 3),
                    "effective_tps": round(effective_tps, 2),
                    "aggregate_working_set_gb": round(total_mem_gb, 2),
                }
            )

    return activation_rows

def main():
    parser = argparse.ArgumentParser(
        description="Fingerprinted Throughput Sweep Benchmark"
    )
    parser.add_argument("--max-size", type=int, default=2000,
                        help="Maximum model size in billions (default: 2000)")
    parser.add_argument("--step", type=int, default=100,
                        help="Step size in billions (default: 100)")
    parser.add_argument("--quants", default=ALL_Q_TYPES,
                        help="Comma-separated quantizations (default: all known Q types + fp modes)")
    parser.add_argument(
        "--methods",
        default=",".join(METHOD_PROFILES.keys()),
        help="Comma-separated loading/streaming methods",
    )
    parser.add_argument(
        "--compressions",
        default=",".join(COMPRESSION_PROFILES.keys()),
        help="Comma-separated compression methods",
    )
    parser.add_argument(
        "--activation-size",
        type=int,
        default=70,
        help="Model size used for simultaneous activation sweep (default: 70)",
    )
    parser.add_argument(
        "--activation-quant",
        default="q4_k_m",
        help="Quantization used for simultaneous activation sweep (default: q4)",
    )
    parser.add_argument(
        "--max-combos-per-width",
        type=int,
        default=20,
        help="Limit combinations per activation width to keep output bounded",
    )
    parser.add_argument(
        "--target-min-tps",
        type=float,
        default=20.0,
        help="Target minimum TPS floor for self-correcting loader",
    )
    parser.add_argument(
        "--compact-output",
        action="store_true",
        help="Print compact summaries instead of full table dumps",
    )
    parser.add_argument("--output", default="bench_sweep_fingerprinted_results.json",
                        help="Output JSON file")
    
    args = parser.parse_args()
    
    # Print header
    print("╔═══════════════════════════════════════════════════════════════╗")
    print("║  FINGERPRINTED THROUGHPUT + LOADER ACTIVATION BENCHMARK      ║")
    print("║  Synthetic: size + quant + methods + concurrent activation    ║")
    print("╚═══════════════════════════════════════════════════════════════╝\n")
    
    # Parse quantizations
    quants = [q.strip() for q in args.quants.split(",")]
    methods = [m.strip() for m in args.methods.split(",") if m.strip()]
    compressions = [c.strip() for c in args.compressions.split(",") if c.strip()]
    for method in methods:
        if method not in METHOD_PROFILES:
            raise ValueError(f"Unknown method '{method}'. Known: {', '.join(METHOD_PROFILES.keys())}")
    for compression in compressions:
        if compression not in COMPRESSION_PROFILES:
            raise ValueError(
                f"Unknown compression '{compression}'. Known: {', '.join(COMPRESSION_PROFILES.keys())}"
            )

    compact_output = args.compact_output or (len(quants) * len(methods) * len(compressions) * len(sizes) > 1500)
    
    # Build size list
    sizes = list(range(7, args.max_size + 1, args.step))
    
    total_tests = len(sizes) * len(quants) * len(methods) * len(compressions)
    print(f"Configuration:")
    print(f"  Max Size:        {args.max_size}B")
    print(f"  Step Size:       {args.step}B")
    print(f"  Quantizations:   {', '.join(quants)}")
    print(f"  Methods:         {', '.join(methods)}")
    print(f"  Compressions:    {', '.join(compressions)}")
    print(f"  Target Min TPS:  {args.target_min_tps}")
    print(f"  Output Mode:     {'compact' if compact_output else 'full'}")
    print(f"  Total Tests:     {total_tests}\n")
    
    # Run sweep
    results = []
    for quant in quants:
        if not compact_output:
            print(f"\n📊 {quant} quantization:")
        for method in methods:
            if not compact_output:
                print(f"\n  Method: {METHOD_PROFILES[method]['label']} [{method}]")
            for compression in compressions:
                if not compact_output:
                    print(
                        f"    Compression: {COMPRESSION_PROFILES[compression]['label']} [{compression}]"
                    )
                    print("    Size  │  Predicted  │  Measured  │  Load ms  │  Efficiency")
                    print("    ──────┼─────────────┼────────────┼───────────┼────────────")
                for size in sizes:
                    result = run_fingerprint(size, quant, method, compression, args.target_min_tps)
                    results.append(result)
                    if not compact_output:
                        print(
                            f"    {size:>4}B │ {result['predicted_tps']:>10.2f} │ {result['measured_tps']:>9.2f} "
                            f"│ {result['approx_load_ms']:>8.1f} │ {result['efficiency']:>10.3f}"
                        )

    activation_rows = run_activation_sweep(
        model_size_b=args.activation_size,
        quantization=args.activation_quant,
        methods=methods,
        compressions=compressions,
        max_combinations_per_width=args.max_combos_per_width,
        target_tps_floor=args.target_min_tps,
    )
    
    # Calculate statistics
    all_tps = [r["measured_tps"] for r in results]
    all_eff = [r["efficiency"] for r in results]
    
    avg_tps = sum(all_tps) / len(all_tps)
    max_tps = max(all_tps)
    min_tps = min(all_tps)
    max_eff = max(all_eff)
    
    best_result = max(results, key=lambda r: r["measured_tps"])
    best_eff_result = max(results, key=lambda r: r["efficiency"])
    
    # Print summary
    print("\n" + "═" * 63)
    print("📈 STATISTICS:")
    print(f"  Avg TPS (all):              {avg_tps:.2f}")
    print(f"  Max TPS:                    {max_tps:.2f}")
    print(f"  Min TPS:                    {min_tps:.2f}")
    print(f"  Best Efficiency (TPS/B):    {max_eff:.3f}")
    
    print("\n🏆 BEST PERFORMANCE:")
    print(
        f"  {best_result['model_size_b']}B {best_result['quantization']} {best_result['method']} "
        f"{best_result['compression']} → {best_result['measured_tps']} TPS"
    )
    
    print("\n⚡ BEST EFFICIENCY:")
    print(
        f"  {best_eff_result['model_size_b']}B {best_eff_result['quantization']} {best_eff_result['method']} "
        f"{best_eff_result['compression']} → {best_eff_result['efficiency']} TPS/B"
    )
    
    # Throughput trend chart (Q4 only)
    print("\n📊 Throughput Trend (first method/compression on q4_k_m):")
    q4_results = sorted(
        [
            r
            for r in results
            if r["quantization"] == "q4_k_m"
            and r["method"] == methods[0]
            and r["compression"] == compressions[0]
        ],
                        key=lambda x: x["model_size_b"])
    for r in q4_results:
        bars = "▰" * max(1, int(r["measured_tps"] / 5))
        print(f"  {r['model_size_b']:>3}B: {bars} {r['measured_tps']:.1f} TPS")
    
    # Show activation summary (how much can be active at once)
    print("\n🚀 Concurrent Activation Summary")
    print(f"  Reference: {args.activation_size}B {args.activation_quant}")
    print("  Active │ Best Effective TPS │ Working Set GB │ Lanes")
    print("  ───────┼────────────────────┼────────────────┼────────────────────────────────")
    by_width: Dict[int, Dict] = {}
    for row in activation_rows:
        width = row["active_count"]
        if width not in by_width or row["effective_tps"] > by_width[width]["effective_tps"]:
            by_width[width] = row
    for width in sorted(by_width):
        row = by_width[width]
        lanes_txt = ",".join(row["lanes"])[:32]
        print(
            f"  {width:>5} │ {row['effective_tps']:>18.2f} │ {row['aggregate_working_set_gb']:>14.2f} │ {lanes_txt}"
        )

    corrected_count = sum(1 for r in results if r["self_corrected"])

    # Export JSON
    output = {
        "timestamp": datetime.now(UTC).isoformat(),
        "config": {
            "max_size_b": args.max_size,
            "step_b": args.step,
            "quantizations": quants,
            "methods": methods,
            "compressions": compressions,
            "activation_size_b": args.activation_size,
            "activation_quant": args.activation_quant,
            "total_measurements": len(results),
        },
        "results": results,
        "activation_results": activation_rows,
        "statistics": {
            "avg_tps": round(avg_tps, 2),
            "max_tps": round(max_tps, 2),
            "min_tps": round(min_tps, 2),
            "best_efficiency": round(max_eff, 3),
            "self_corrected_count": corrected_count,
        },
        "best_performance": {
            "size_b": best_result["model_size_b"],
            "quantization": best_result["quantization"],
            "method": best_result["method"],
            "compression": best_result["compression"],
            "tps": best_result["measured_tps"]
        },
        "best_efficiency": {
            "size_b": best_eff_result["model_size_b"],
            "quantization": best_eff_result["quantization"],
            "method": best_eff_result["method"],
            "compression": best_eff_result["compression"],
            "tps_per_b": best_eff_result["efficiency"]
        },
        "best_activation_by_width": {str(k): by_width[k] for k in sorted(by_width)},
    }
    
    with open(args.output, 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"\n✨ Results saved to: {args.output}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
