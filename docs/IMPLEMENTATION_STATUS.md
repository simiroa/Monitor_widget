# Monitor Widget C++ Port – Current State

## Overview
- The portable monitoring widget now has a statically built Qt6 `monitor_widget.exe` (CMake-based) and an auxiliary `.NET 8` helper called `sensor_host.exe`.  
- Data pipelines currently include: NVML (optional via `MONITOR_WIDGET_ENABLE_NVML`), DXGI/D3DKMT/DXCore fallbacks for GPU/VRAM stats, and a sensor host for deeper CPU/GPU sensor readings.
- The UI mirrors the original Python design: sidebar list, per-tab detail views (CPU/RAM/GPU/VRAM/Disk/Net/Servers) with overlaid capability notices and process lists.

## GPU/VRAM Data Flow
1. `GpuCollector` chooses between NVML (if enabled/available) and DXGI/D3DKMT/DXCore for adapter info, usage, VRAM, temperature, clock, and power.  
2. Capability flags now track NVML enablement/availability, per-GPU/per-VRAM counters, and vendor info for UI notices.  
3. GPU process stats use PDH; when PDH fails it negotiates DXCore and falling back to D3DKMT for per-process VRAM/GPU usage.  
4. `ProcessListPage` and `GpuPage` display GPU load/temp/clock/power with contextual notices and highlight NVML usage reasons.

## Sensor Host Integration (LHM)
- `sensor_host` is a .NET console app that uses LibreHardwareMonitor to sample CPU/GPU temperatures, power, and clocks. It exposes snapshots via a named pipe (`monitor_widget_sensors_<PID>`).  
- `SensorHostClient` in Qt spawns the helper, connects through `QLocalSocket`, parses JSON lines, and merges sensor snapshots into `SystemStats`.  
- The engine now keeps CPU power/temp fields and capability flags for those axes; the UI shows CPU power when available.  
- Sensor data is only used when NVML/DXCore data is absent and is disabled via `MONITOR_WIDGET_DISABLE_SENSOR_HOST` (useful for crash isolation).  
- Helper can be built via `dotnet build -c Release` (outputs under `cpp/sensor_host/bin/Release/net8.0`). Consider `dotnet publish` for standalone deployment.

## Known Issues
1. **Heap corruption crash (0xc0000374)** immediately after startup or tab switches (even with `MONITOR_WIDGET_DISABLE_SENSOR_HOST=1`). Crash buckets: `StackHash_29ac`. WER report at `C:\Users\HG\Desktop\CrashReport\Report.wer` indicates `ntdll.dll`. No `.mdmp` yet, so stack trace unknown.  
2. **GPU process/VRAM data still flaky**: PDH failures remain (`status=2147485653`), DXCore process engine count unsupported (hr=0x80070057), so per-process info is spotty.  
3. **Sensor host path**: needs packaging (helper + LibreHardwareMonitor, NT runtime). Also suspect memory handling when parsing JSON might contribute to crashes.

## Running & Diagnostics
- Run `cpp/build_new/monitor_widget.exe` – use `MONITOR_WIDGET_DISABLE_SENSOR_HOST=1` to bypass the helper when debugging stability.  
- Latest logs live in `cpp/build_new/logs/monitor_widget_<timestamp>.log` (more detailed than root `logs/`).  
- Crash dumps appear under `C:\ProgramData\Microsoft\Windows\WER\ReportArchive\AppCrash_monitor_widget…`. Copy `.wer`/`.mdmp` for stack traces.  
- The helper binary is at `cpp/sensor_host/bin/Release/net8.0/sensor_host.exe` after `dotnet build -c Release`.

## Next Steps (to continue later)
1. Capture `.mdmp` for the heap corruption crash or add verbose logging around `SensorHostClient::parseLine` & the GPU tab transitions.  
2. Harden `SensorHostClient` parsing (buffer boundaries, invalid JSON, exception handling) and consider running the helper in verbose mode to dump payloads before passing them to Qt.  
3. Revisit DXCore/D3DKMT fallback handling, especially around engine count, per-process VRAM, and GPU load direction, so the experience matches the original Python version.  
4. Once stability is restored, package `sensor_host` as part of the release (self-contained or with runtime) and document the NVML toggle/environment flags for installers.
