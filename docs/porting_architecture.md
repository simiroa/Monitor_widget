Monitor Widget C++/Qt Port - Architecture Draft

Goals
- Port the current Python/Qt widget to C++/Qt with improved stability and memory usage.
- Keep the current UI layout and behavior as close as possible.
- Support Windows 10/11 only.
- Keep disk cleanup and network speed test features.
- Remove "process boost" (suspending background processes).

Target stack
- Qt 6.5 LTS (stable Windows support).
- Qt modules: Core, Gui, Widgets, Network, Concurrent (optional), Svg (if icon set needs it).
- Windows APIs: PDH, IPHLPAPI, PSAPI, SetupAPI, DXGI, DXVA2, GDI, WMI (COM), Shell32.
- Optional vendor SDKs: NVML for NVIDIA; AMD/Intel will use generic backends.

High-level module layout
- src/app
  - main.cpp (entry, single instance, admin checks)
  - AppConfig (QSettings wrapper)
- src/engine
  - StatsEngine (worker thread, timers, caching, signals)
  - LeakDetector (ring-buffer trends)
  - ServerDetector (GetExtendedTcpTable)
  - ProcessAnalyzer (CPU/RAM/GPU per process)
- src/collectors
  - CpuMemCollector
  - DiskCollector
  - NetCollector
  - DriveCollector
  - GpuCollector (backend dispatch)
  - ProcessCollector
- src/gpu
  - GpuBackend (interface)
  - DxgiBackend (name, vram, load where possible)
  - NvmlBackend (optional, NVIDIA only)
- src/models
  - SystemStats, ProcessInfo, DriveInfo, ServerPoint
  - CapabilityFlags (what is supported on this machine)
- src/ui
  - MainWindow (frameless, tray, sidebar, stacked pages)
  - SidebarItem, ProcessRow, DriveRow, ServerRow
  - Pages: CpuPage, RamPage, GpuPage, VramPage, DiskPage, NetPage, ServerPage
- src/services
  - MonitorController (brightness, refresh, night mode, sleep)
  - CleanupService (disk cleanup)
  - SpeedTestRunner (download/upload/ping)
- src/utils
  - WinHelpers (admin, processes, handles, error strings)

Data flow and threading
- StatsEngine runs in a dedicated QThread with a QTimer (2s tick).
- Process snapshots update less frequently (5s tick) and are cached.
- All collectors are called in the engine thread only.
- StatsEngine emits:
  - statsUpdated(SystemStats)
  - processesUpdated(QVector<ProcessInfo>)
  - serversUpdated(QVector<ServerPoint>)
  - capabilitiesUpdated(CapabilityFlags)
- UI connects with queued signals; no direct cross-thread access.

Key data models (C++ structs/classes)
- SystemStats
  - cpuPercent, cpuTempC, ramPercent, ramUsedGB, ramTotalGB
  - gpuLoad, gpuTempC, vramPercent, vramUsedGB, vramTotalGB
  - gpuClockMHz, gpuPowerW, gpuName
  - diskReadMBs, diskWriteMBs
  - netRecvMBs, netSentMBs
  - drives (vector of DriveInfo)
  - serverPoints (vector of ServerPoint)
  - capabilities (CapabilityFlags)
- ProcessInfo
  - pid, name, cpuPercent, ramMB, vramMB, gpuPercent, diskIoMBs
  - isServer, serverPorts, isLeakSuspect, isGpu, isWhitelisted
  - iconPath (optional)
- DriveInfo
  - mountpoint, totalGB, usedGB, freeGB, percent
  - readMBs, writeMBs, ioValid
- ServerPoint
  - port, name, pid, serverType, exePath

GPU support policy
- Always attempt generic DXGI/WMI for name and VRAM.
- GPU load and temperature are best-effort and may be unavailable on AMD/Intel.
- If a feature requires NVML (NVIDIA), report "NVIDIA only" and keep UI stable.
- CapabilityFlags should drive UI labels (eg "Temp: N/A", "Load: N/A").

Disk cleanup service (CleanupService)
- Temp, Recycle Bin, Downloads, Shader caches, Engine caches, Delivery Opt, WinSxS, Hibernate.
- Admin-required operations should be disabled with a clear label if not elevated.
- Use QProcess for DISM / powercfg / net stop-start where needed.

Network speed test (SpeedTestRunner)
- Use QNetworkAccessManager in a worker thread.
- Download: stream data from a known URL for N seconds and compute Mbps.
- Upload: POST N MB payload to a known endpoint for N seconds.
- Ping: TCP connect timing to a public DNS (or ICMP if allowed).
- Emit progress and final results through Qt signals.

UI behavior parity notes
- Frameless always-on-top window with drag and edge snap.
- Sidebar indicators with sparkline history.
- Expand/collapse panel with stacked pages.
- System tray icon with show/hide and quit.
- Opacity slider (window-level opacity).
- Settings persisted via QSettings.

Feature mapping (Python -> C++/Qt)
- MonitorEngine -> StatsEngine + collectors
- ProcessManager (kill/suspend/resume) -> ProcessControlService (kill/resume only)
- MonitorController -> MonitorController (same features, Windows APIs)
- DiskCleaner/CleanupDialog -> CleanupService + CleanupDialog
- NetPage speed test -> SpeedTestRunner + NetPage

Implementation order (next steps)
1) Create CMake + Qt project skeleton with core data models and StatsEngine interface.
2) Implement CpuMemCollector, DiskCollector, NetCollector.
3) Implement GPU backends and ProcessAnalyzer.
4) Implement ServerDetector and LeakDetector.
5) Build UI pages and wire to engine signals.
6) Add cleanup and speed test services with proper privilege gating.
