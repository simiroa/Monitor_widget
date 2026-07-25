using System;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using LibreHardwareMonitor.Hardware;
using Newtonsoft.Json;

namespace SensorHost
{
    class Program
    {
        private static string _pipeName = "monitor_widget_pipe";
        private static int _interval = 1000;
        private static bool _verbose = false;
        private static bool _testMode = false;

        static void Main(string[] args)
        {
            // Debug Log Path: Next to the executable
            // Debug Log Path: Next to the executable
            string debugLogPath = "sensor_debug_v21.txt";

            try 
            {
                File.WriteAllText(debugLogPath, $"SensorHost Started at {DateTime.Now}\n");
            } 
            catch { /* best effort */ }

            void Log(string message)
            {
                try { File.AppendAllText(debugLogPath, message + "\n"); } catch { }
                if (_verbose || _testMode) Console.WriteLine(message);
            }

            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "--pipe" && i + 1 < args.Length) _pipeName = args[i + 1];
                if (args[i] == "--interval" && i + 1 < args.Length) int.TryParse(args[i + 1], out _interval);
                if (args[i] == "--verbose") _verbose = true;
                if (args[i] == "--test") { _testMode = true; _verbose = true; }
            }

            Log($"Pipe: {_pipeName}, Interval: {_interval}ms, TestMode: {_testMode}");

            var computer = new Computer
            {
                IsCpuEnabled = true,
                IsGpuEnabled = true,
                IsMemoryEnabled = false,
                IsMotherboardEnabled = true, 
                IsControllerEnabled = true,  
                IsNetworkEnabled = false,
                IsStorageEnabled = false
            };
            
            try
            {
                Log("Opening LHM Computer...");
                computer.Open();
                Log("LHM Computer Opened Successfully.");

                Log("--- Hardware Dump Start ---");
                void DumpToLog(IHardware hw, int level)
                {
                    string indent = new string(' ', level * 2);
                    hw.Update();
                    Log($"{indent}- [{hw.HardwareType}] {hw.Name} (ID: {hw.Identifier})");
                    foreach (var sensor in hw.Sensors)
                    {
                        Log($"{indent}  - ({sensor.SensorType}) {sensor.Name} = {sensor.Value}");
                    }
                    foreach (var sub in hw.SubHardware) DumpToLog(sub, level + 1);
                }
                foreach (var hw in computer.Hardware) DumpToLog(hw, 0);
                Log("--- Hardware Dump End ---");
            }
            catch (Exception ex)
            {
                Log($"CRITICAL ERROR opening LHM: {ex.Message}\n{ex.StackTrace}");
                return;
            }

            if (_testMode)
            {
                Log("Starting Test Mode Loop (No Pipe)...");
                using (var writer = new StreamWriter(Stream.Null)) 
                {
                    while (true)
                    {
                        UpdateAndSend(computer, writer, debugLogPath);
                        Thread.Sleep(_interval);
                    }
                }
            }
            else
            {
                while (true)
                {
                    try
                    {
                        using (var server = new NamedPipeServerStream(_pipeName, PipeDirection.Out, 1))
                        {
                            Log("Waiting for client connection...");
                            server.WaitForConnection();
                            Log("Client connected.");

                            using (var writer = new StreamWriter(server))
                            {
                                writer.AutoFlush = true;

                                while (server.IsConnected)
                                {
                                    UpdateAndSend(computer, writer, debugLogPath);
                                    Thread.Sleep(_interval);
                                }
                            }
                        }
                    }
                    catch (IOException)
                    {
                        // Client disconnected
                    }
                    catch (Exception ex)
                    {
                        Log($"Pipe Error: {ex.Message}");
                        Thread.Sleep(1000);
                    }
                }
            }
        }

        private static void UpdateAndSend(Computer computer, StreamWriter writer, string debugLogPath)
        {
            var data = new SensorData();
            data.timestampMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

            double fallbackTempStart = 0;

            // Helper to process sensors
            void ProcessSensors(IHardware hw)
            {
                foreach (var sensor in hw.Sensors)
                {
                    // GLOBAL CPU TEMP SEARCH (Any Hardware)
                    if (sensor.SensorType == SensorType.Temperature)
                    {
                        // 1. Exact CPU Match (Package/Tdie) - Trusted Hardware Only
                        if (hw.HardwareType == HardwareType.Cpu && 
                           (sensor.Name.IndexOf("Package", StringComparison.OrdinalIgnoreCase) >= 0 ||
                            sensor.Name.IndexOf("Tdie", StringComparison.OrdinalIgnoreCase) >= 0))
                        {
                            // Only overwrite if CoreTemp failed or is zero
                            if (data.cpuTempC == 0) data.cpuTempC = sensor.Value ?? 0;
                        }
                        
                        // 2. Generic "CPU" name (Any Hardware: Mobo, Cooler, etc) - Only if no value yet
                        else if (data.cpuTempC == 0 && sensor.Name.IndexOf("CPU", StringComparison.OrdinalIgnoreCase) >= 0)
                        {
                            data.cpuTempC = sensor.Value ?? 0;
                        }
                        
                        // 3. Liquid/Coolant Temp (AIO Coolers) - Fallback
                        else if (data.cpuTempC == 0 && 
                                (sensor.Name.IndexOf("Liquid", StringComparison.OrdinalIgnoreCase) >= 0 || 
                                 sensor.Name.IndexOf("Coolant", StringComparison.OrdinalIgnoreCase) >= 0 ||
                                 sensor.Name.IndexOf("Water", StringComparison.OrdinalIgnoreCase) >= 0 ||
                                 sensor.Name.IndexOf("Pump", StringComparison.OrdinalIgnoreCase) >= 0))
                        {
                            data.cpuTempC = sensor.Value ?? 0;
                        }

                        // 4. Fallback: First generic temp on Motherboard
                        else if (data.cpuTempC == 0 && fallbackTempStart == 0 && hw.HardwareType == HardwareType.Motherboard)
                        {
                            fallbackTempStart = sensor.Value ?? 0;
                        }
                    }

                    if (sensor.SensorType == SensorType.Power)
                    {
                         // CPU Power (Package)
                        if (sensor.Name.IndexOf("Package", StringComparison.OrdinalIgnoreCase) >= 0 && hw.HardwareType == HardwareType.Cpu)
                            data.cpuPowerW = sensor.Value ?? data.cpuPowerW;
                    }

                    // GPU Handler (Explicit)
                    if (hw.HardwareType == HardwareType.GpuNvidia || hw.HardwareType == HardwareType.GpuAmd || hw.HardwareType == HardwareType.GpuIntel)
                    {
                        if (data.gpuName == null) data.gpuName = hw.Name;
                        
                        if (sensor.SensorType == SensorType.Temperature && sensor.Name.Contains("Core"))
                            data.gpuTempC = sensor.Value ?? 0;
                        if (sensor.SensorType == SensorType.Power && sensor.Name.Contains("Package"))
                            data.gpuPowerW = sensor.Value ?? 0;
                        if (sensor.SensorType == SensorType.Clock && sensor.Name.Contains("Core"))
                            data.gpuClockMHz = sensor.Value ?? 0;
                    }
                }
            }

            foreach (var hardware in computer.Hardware)
            {
                hardware.Update();
                if (hardware.HardwareType == HardwareType.Cpu && data.cpuName == null) data.cpuName = hardware.Name;
                
                ProcessSensors(hardware);

                foreach (var subHardware in hardware.SubHardware)
                {
                    subHardware.Update();
                    ProcessSensors(subHardware);
                }
            }

            // Apply fallback only if EVERYTHING else failed
            if (data.cpuTempC == 0 && fallbackTempStart > 0)
            {
                data.cpuTempC = fallbackTempStart;
            }

            string json = JsonConvert.SerializeObject(data);
            writer.WriteLine(json);
            
            if (_verbose || _testMode) 
            {
                Console.WriteLine($"[JSON] {json}");
                if (data.cpuTempC > 0) Console.WriteLine($"   -> CPU Temp Active: {data.cpuTempC}");
                else Console.WriteLine($"   -> CPU Temp ZERO (Fail - LHM access blocked)");
            }
        }

        class SensorData
        {
            public long timestampMs;
            public string cpuName;
            public double cpuTempC;
            public double cpuPowerW;
            public string gpuName;
            public double gpuTempC;
            public double gpuPowerW;
            public double gpuClockMHz;
        }
    }
}
