from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from itertools import count
from typing import Any, Callable, Dict

from .models import ExecutionContext


@dataclass
class ActionResponse:
    success: bool
    details: Dict[str, Any]


_SESSION_COUNTER = count(1)
_LOCAL_MODEL_SESSIONS: Dict[str, Dict[str, Any]] = {}


def _enrich_ioc(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    indicator = params.get("indicator", "")
    intel = {
        "indicator": indicator,
        "threat_score": 92 if indicator else 10,
        "families": ["ransomware", "loader"] if indicator else [],
        "sources": ["internal-ti", "partner-feed"],
    }
    ctx.findings.append({"type": "ioc_enrichment", **intel})
    return ActionResponse(True, intel)


def _query_edr(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    host = params.get("host", ctx.incident.get("host", "unknown-host"))
    result = {
        "host": host,
        "detections": ["suspicious_powershell", "credential_dump_pattern"],
        "active_sessions": 3,
    }
    ctx.findings.append({"type": "edr_snapshot", **result})
    return ActionResponse(True, result)


def _isolate_host(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    host = params.get("host", ctx.incident.get("host", "unknown-host"))
    if ctx.dry_run:
        return ActionResponse(True, {"host": host, "mode": "dry-run", "isolated": False})
    return ActionResponse(True, {"host": host, "mode": "enforced", "isolated": True})


def _disable_account(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    account = params.get("account", ctx.incident.get("suspect_account", "unknown-account"))
    if ctx.dry_run:
        return ActionResponse(True, {"account": account, "mode": "dry-run", "disabled": False})
    return ActionResponse(True, {"account": account, "mode": "enforced", "disabled": True})


def _block_ip(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    ip = params.get("ip", ctx.incident.get("source_ip", "0.0.0.0"))
    if ctx.dry_run:
        return ActionResponse(True, {"ip": ip, "mode": "dry-run", "blocked": False})
    return ActionResponse(True, {"ip": ip, "mode": "enforced", "blocked": True})


def _snapshot_host(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    host = params.get("host", ctx.incident.get("host", "unknown-host"))
    artifact = {
        "type": "forensic_snapshot",
        "host": host,
        "artifact_id": f"snap-{host}-001",
    }
    ctx.artifacts.append(artifact)
    return ActionResponse(True, artifact)


def _create_ticket(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    queue = params.get("queue", "IR-P1")
    title = params.get("title", "Security Incident Response")
    ticket = {
        "ticket_id": "INC-SEC-10042",
        "queue": queue,
        "title": title,
        "operator": ctx.operator,
    }
    return ActionResponse(True, ticket)


def _notify_channel(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    channel = params.get("channel", "#soc-war-room")
    message = params.get("message", "workflow update")
    return ActionResponse(True, {"channel": channel, "message": message, "delivered": True})


def _discover_re_subjects(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    samples = params.get(
        "samples",
        [
            {"name": "legacy_firmware_blob", "entropy": 7.9, "interfaces": 4, "market_signal": 0.7},
            {"name": "industrial_gateway_updater", "entropy": 7.1, "interfaces": 6, "market_signal": 0.9},
            {"name": "embedded_payment_module", "entropy": 6.8, "interfaces": 8, "market_signal": 0.95},
        ],
    )

    ranked = []
    for sample in samples:
        entropy = float(sample.get("entropy", 0.0))
        interfaces = float(sample.get("interfaces", 0.0))
        market_signal = float(sample.get("market_signal", 0.0))
        re_score = round((entropy * 0.45) + (interfaces * 0.25) + (market_signal * 10.0 * 0.30), 3)
        ranked.append(
            {
                "subject": sample.get("name", "unknown"),
                "re_score": re_score,
                "entropy": entropy,
                "interfaces": interfaces,
                "market_signal": market_signal,
            }
        )

    ranked.sort(key=lambda x: x["re_score"], reverse=True)

    clusters = {
        "commercial_priority": [r for r in ranked if r["market_signal"] >= 0.8],
        "normal_grouping": [r for r in ranked if r["market_signal"] < 0.8],
    }

    result = {
        "ranked_subjects": ranked,
        "clusters": clusters,
        "top_subject": ranked[0]["subject"] if ranked else None,
    }
    ctx.findings.append({"type": "re_subject_discovery", **result})
    return ActionResponse(True, result)


def _benchmark_inference_backends(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    gpu_vendor = str(params.get("gpu_vendor", ctx.incident.get("gpu_vendor", "amd"))).lower()
    candidates = params.get("candidates", ["amd_rocm", "cuda", "cpu_avx2"])
    workload_factor = float(params.get("workload_factor", 1.0))

    baseline_tps = {
        "amd_rocm": 12800.0,
        "cuda": 12200.0,
        "cpu_avx2": 1900.0,
    }

    if gpu_vendor == "amd":
        baseline_tps["amd_rocm"] *= 1.15
        baseline_tps["cuda"] *= 0.92
    elif gpu_vendor == "nvidia":
        baseline_tps["cuda"] *= 1.18
        baseline_tps["amd_rocm"] *= 0.87

    tps_by_backend: Dict[str, float] = {}
    for backend in candidates:
        base = baseline_tps.get(backend, 1000.0)
        tps_by_backend[backend] = round(base * workload_factor, 2)

    best_backend = max(tps_by_backend, key=tps_by_backend.get)
    result = {
        "gpu_vendor": gpu_vendor,
        "tps_by_backend": tps_by_backend,
        "best_backend": best_backend,
        "best_tps": tps_by_backend[best_backend],
    }
    ctx.findings.append({"type": "backend_benchmark", **result})
    return ActionResponse(True, result)


def _four_d_strategy(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    subjects = []
    bench = {}

    if "steps" in ctx.vars:
        subjects = ctx.vars["steps"].get("subject_discovery", {}).get("ranked_subjects", [])
        bench = ctx.vars["steps"].get("hardware_benchmark", {})

    dimensions = {
        "technical_depth": float(params.get("technical_depth", 0.85)),
        "market_expansion": float(params.get("market_expansion", 0.90)),
        "throughput_scaling": float(params.get("throughput_scaling", 0.88)),
        "operational_complexity": float(params.get("operational_complexity", 0.55)),
    }

    top_subject = subjects[0]["subject"] if subjects else "unknown"
    recommended_backend = bench.get("best_backend", "amd_rocm")

    plan = {
        "top_subject": top_subject,
        "recommended_backend": recommended_backend,
        "dimensions": dimensions,
        "expansion_score": round(
            (dimensions["technical_depth"] * 0.30)
            + (dimensions["market_expansion"] * 0.30)
            + (dimensions["throughput_scaling"] * 0.30)
            + ((1.0 - dimensions["operational_complexity"]) * 0.10),
            4,
        ),
    }

    ctx.findings.append({"type": "four_d_strategy", **plan})
    return ActionResponse(True, plan)


def _plan_local_model_loading(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    model_path = params.get("model_path", ctx.incident.get("model_path", "models/local.gguf"))
    gpu_vendor = str(params.get("gpu_vendor", ctx.incident.get("gpu_vendor", "amd"))).lower()

    lanes = [
        {
            "lane": "persistent_mmap",
            "strengths": ["lowest_reload_cost", "stable_tokens_per_second"],
            "tradeoffs": ["higher_baseline_memory_reservation"],
            "score": 0.91,
        },
        {
            "lane": "ephemeral_on_demand",
            "strengths": ["low_idle_memory", "simple_lifecycle"],
            "tradeoffs": ["higher_reload_latency", "session_staleness_risk"],
            "score": 0.73,
        },
        {
            "lane": "sharded_hot_swap",
            "strengths": ["fast_unstale_session_swap", "predictable_local_recovery"],
            "tradeoffs": ["orchestration_complexity"],
            "score": 0.88,
        },
    ]

    if gpu_vendor == "amd":
        lanes[0]["score"] = round(float(lanes[0]["score"]) + 0.03, 3)
        lanes[2]["score"] = round(float(lanes[2]["score"]) + 0.02, 3)

    lanes.sort(key=lambda x: float(x["score"]), reverse=True)
    recommendation = lanes[0]["lane"]

    result = {
        "model_path": model_path,
        "gpu_vendor": gpu_vendor,
        "lanes": lanes,
        "recommended_lane": recommendation,
        "notes": "Use local persistent or hot-swap lane to avoid stale session drift.",
    }
    ctx.findings.append({"type": "local_model_loading_plan", **result})
    return ActionResponse(True, result)


def _open_local_model_session(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    model_path = params.get("model_path", ctx.incident.get("model_path", "models/local.gguf"))
    lane = params.get("lane", "persistent_mmap")
    max_uses_before_stale = int(params.get("max_uses_before_stale", 2))
    allow_fresh_swap_no_retry = bool(params.get("allow_fresh_swap_no_retry", True))

    sid = f"local-session-{next(_SESSION_COUNTER)}"
    _LOCAL_MODEL_SESSIONS[sid] = {
        "id": sid,
        "model_path": model_path,
        "lane": lane,
        "uses_remaining": max_uses_before_stale,
        "allow_fresh_swap_no_retry": allow_fresh_swap_no_retry,
        "created_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "state": "ready",
    }
    ctx.vars["active_local_session_id"] = sid

    return ActionResponse(
        True,
        {
            "session_id": sid,
            "model_path": model_path,
            "lane": lane,
            "allow_fresh_swap_no_retry": allow_fresh_swap_no_retry,
            "state": "ready",
        },
    )


def _run_local_model_inference(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    session_id = params.get("session_id")
    if not session_id:
        cached = ctx.vars.get("steps", {}).get("open_local_session", {})
        session_id = cached.get("session_id")

    prompt = params.get("prompt", "local inference test")
    min_tps = float(params.get("min_tps", 2000.0))

    if session_id not in _LOCAL_MODEL_SESSIONS:
        return ActionResponse(False, {"error": f"unknown session_id: {session_id}"})

    session = _LOCAL_MODEL_SESSIONS[session_id]
    fresh_swap = False

    if int(session.get("uses_remaining", 0)) <= 0:
        if bool(session.get("allow_fresh_swap_no_retry", False)):
            new_sid = f"local-session-{next(_SESSION_COUNTER)}"
            _LOCAL_MODEL_SESSIONS[new_sid] = {
                **session,
                "id": new_sid,
                "uses_remaining": 2,
                "created_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
                "state": "ready",
            }
            session["state"] = "stale_replaced"
            session_id = new_sid
            session = _LOCAL_MODEL_SESSIONS[new_sid]
            fresh_swap = True
        else:
            session["state"] = "stale"
            return ActionResponse(False, {"error": "session stale and no fresh swap policy enabled"})

    lane = session.get("lane", "persistent_mmap")
    base_tps = {
        "persistent_mmap": 13200.0,
        "ephemeral_on_demand": 8900.0,
        "sharded_hot_swap": 12400.0,
    }.get(lane, 7000.0)

    prompt_size_factor = max(0.70, min(1.10, 1.0 - (len(str(prompt)) / 10000.0)))
    measured_tps = round(base_tps * prompt_size_factor, 2)

    session["uses_remaining"] = int(session.get("uses_remaining", 1)) - 1
    session["state"] = "ready" if session["uses_remaining"] > 0 else "aging"
    ctx.vars["active_local_session_id"] = session_id

    return ActionResponse(
        measured_tps >= min_tps,
        {
            "session_id": session_id,
            "fresh_swap_without_retry": fresh_swap,
            "lane": lane,
            "measured_tps": measured_tps,
            "min_tps": min_tps,
            "uses_remaining": session["uses_remaining"],
            "state": session["state"],
        },
    )


def _close_local_model_session(ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    session_id = params.get("session_id")
    if not session_id:
        session_id = ctx.vars.get("active_local_session_id")
    if not session_id:
        cached = ctx.vars.get("steps", {}).get("open_local_session", {})
        session_id = cached.get("session_id")

    if not session_id:
        return ActionResponse(False, {"error": "no session_id provided"})

    existed = session_id in _LOCAL_MODEL_SESSIONS
    if existed:
        del _LOCAL_MODEL_SESSIONS[session_id]
    if ctx.vars.get("active_local_session_id") == session_id:
        ctx.vars["active_local_session_id"] = None
    return ActionResponse(True, {"session_id": session_id, "closed": existed})


ACTION_REGISTRY: Dict[str, Callable[[ExecutionContext, Dict[str, Any]], ActionResponse]] = {
    "enrich_ioc": _enrich_ioc,
    "query_edr": _query_edr,
    "isolate_host": _isolate_host,
    "disable_account": _disable_account,
    "block_ip": _block_ip,
    "snapshot_host": _snapshot_host,
    "create_ticket": _create_ticket,
    "notify_channel": _notify_channel,
    "discover_re_subjects": _discover_re_subjects,
    "benchmark_inference_backends": _benchmark_inference_backends,
    "four_d_strategy": _four_d_strategy,
    "plan_local_model_loading": _plan_local_model_loading,
    "open_local_model_session": _open_local_model_session,
    "run_local_model_inference": _run_local_model_inference,
    "close_local_model_session": _close_local_model_session,
}


def execute_action(action: str, ctx: ExecutionContext, params: Dict[str, Any]) -> ActionResponse:
    handler = ACTION_REGISTRY.get(action)
    if not handler:
        return ActionResponse(False, {"error": f"unknown action: {action}"})
    return handler(ctx, params)
