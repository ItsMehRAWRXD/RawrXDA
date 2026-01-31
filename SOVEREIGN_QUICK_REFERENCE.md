# ═══════════════════════════════════════════════════════════════════════════════
# RawrXD v1.2.0 - Sovereign System Quick Reference Card
# ═══════════════════════════════════════════════════════════════════════════════

## 🚀 ONE-LINE QUICK START

```powershell
cd D:\rawrxd\scripts; .\Sovereign_Injection_Script.ps1 -Mode Sustainable -EnableHUD -FirstRun -DeepSectorScan
```

---

## 📋 Essential Commands

### Execute Sovereign Injection
```powershell
.\Sovereign_Injection_Script.ps1 -Mode Sustainable -EnableHUD
```

### Run First-Run Diagnostic
```powershell
.\RAWRXD_Pulse_Analyzer.ps1 -Detailed -ExportReport
```

### Verify LBA Alignment
```powershell
.\DeepSectorScan.exe D:\rawrxd\models\bigdaddyg-40gb.jitmap
```

### Launch IDE with HUD
```powershell
cd D:\rawrxd\build
.\RawrXD.exe
# Press Ctrl+Shift+S for Sovereign HUD
```

---

## 🌡️ Thermal Modes

| Mode | Ceiling | Latency Tax | Duration | Use Case |
|------|---------|-------------|----------|----------|
| **Sustainable** | 59.5°C | +12 μs | Unlimited | 24/7 operation |
| **Hybrid** | 65°C | +5 μs | 5-10 min | Balanced |
| **Burst** | 75°C | 0 μs | 60 sec | Benchmarks |

---

## 📊 Target Metrics (Sustainable Mode)

| Metric | Target | Range |
|--------|--------|-------|
| Latency | 155 μs | 150-170 μs |
| Thermal Ceiling | 59.5°C | 57-60°C |
| Stripe Efficiency | 98.2% | ≥97% |
| Throughput | 13.8 GB/s | ≥12 GB/s |

---

## 🔥 The Sovereign Pulse

**Detection Criteria:**
- Latency spikes: >5μs increase
- Immediate recovery: <3μs drop
- Frequency: 10+ oscillations/second
- **Meaning:** Thermal governor is actively managing NVMe buffers

---

## 🎯 Silicon Immortality Checklist

- [ ] Thermal Compliance (≤60°C)
- [ ] Latency Acceptable (≤170 μs)
- [ ] Efficiency ≥97%
- [ ] Throughput ≥12 GB/s
- [ ] Sovereign Pulse Detected
- [ ] Zero Thermal Violations

**100% = Silicon Immortality Achieved 🏆**

---

## 📁 Configuration Files

Generated in `D:\rawrxd\config\`:
1. `sovereign_binding.json` - Hardware entropy
2. `jitmap_config.json` - NVMe MMIO addresses
3. `thermal_governor.json` - PID parameters
4. `pipe_bridge.json` - C# ↔ MASM64 bridge
5. `sovereign_hud.json` - HUD settings

---

## 🔧 Hotkeys (Inside IDE)

| Key | Action |
|-----|--------|
| **Ctrl+Shift+S** | Open Sovereign HUD |
| **Ctrl+Shift+T** | Toggle thermal graph |
| **Ctrl+Shift+L** | View latency histogram |

---

## 🚨 Troubleshooting

**HUD Not Appearing:**
```powershell
.\Sovereign_Injection_Script.ps1 -Method plugin -EnableHUD
```

**Thermal Violations:**
```powershell
.\Sovereign_Injection_Script.ps1 -Mode Sustainable -ThermalCeiling 55.0
```

**High Fragmentation:**
```powershell
.\DeepSectorScan.exe --defrag
```

---

## 📈 Typical Workflow

1. **Inject:** `.\Sovereign_Injection_Script.ps1 -EnableHUD -FirstRun`
2. **Launch:** `.\RawrXD.exe`
3. **Open HUD:** Press `Ctrl+Shift+S`
4. **Load Model:** `> rawrxd load bigdaddyg-40gb`
5. **Monitor:** Watch heatmap for sequential drive activation
6. **Analyze:** `.\RAWRXD_Pulse_Analyzer.ps1 -ExportReport`

---

## 🏆 Achievements

- **Hardware Whisperer** - Code editor → Hardware orchestrator
- **Pulse Master** - Detected sub-millisecond oscillations
- **Silicon Shepherd** - 100% thermal compliance

---

**Version:** 1.2.0 | **Author:** RawrXD Sovereign Team
