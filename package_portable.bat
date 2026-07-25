@echo off
REM package_portable.bat
REM
REM Builds Release\portable from build outputs:
REM   1. cpp\build_new       - CMake build dir. Mixed runtime files + build
REM                            artifacts (CMakeFiles, autogen, logs, ...).
REM                            Only the runtime subset is copied.
REM   2. sensor_host publish - single-file publish output (exe + 2 native
REM                            helper DLLs). .pdb is skipped.
REM
REM Safety: this script only ever creates/deletes files under
REM Release\portable. It never touches Release\installer or
REM Release\MonitorWidget_Portable.zip.

setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "SRC_APP=%ROOT%cpp\build_new"
set "SRC_SENSOR=%ROOT%sensor_host\bin\Release\net8.0\win-x64\publish"
set "DEST=%ROOT%Release\portable"

if not exist "%SRC_APP%\monitor_widget.exe" (
    echo [ERROR] "%SRC_APP%\monitor_widget.exe" not found. Build the C++ app first.
    exit /b 1
)
if not exist "%SRC_SENSOR%\sensor_host.exe" (
    echo [ERROR] "%SRC_SENSOR%\sensor_host.exe" not found. Publish sensor_host first.
    exit /b 1
)

tasklist /FI "IMAGENAME eq monitor_widget.exe" 2>NUL | find /I "monitor_widget.exe" >NUL
if not errorlevel 1 (
    echo [ERROR] monitor_widget.exe is running. Close the widget before packaging.
    exit /b 1
)
tasklist /FI "IMAGENAME eq sensor_host.exe" 2>NUL | find /I "sensor_host.exe" >NUL
if not errorlevel 1 (
    echo [ERROR] sensor_host.exe is running. Close the widget before packaging.
    exit /b 1
)

echo Resetting "%DEST%" ...
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%"

echo Copying app runtime files from cpp\build_new ...
robocopy "%SRC_APP%" "%DEST%" /E ^
    /XD "CMakeFiles" "monitor_widget_autogen" ".qt" "logs" "translations" "VirtualDisplayDriver" ^
    /XF "*.txt" "Makefile" "CMakeCache.txt" "cmake_install.cmake" "*.cmake" "*.pdb" "*.ilk" "*.obj" ^
    /R:2 /W:2 /NFL /NDL /NJH /NP
if errorlevel 8 (
    echo [ERROR] robocopy failed copying app files, exit code %errorlevel%
    exit /b 1
)

REM --- GL/D3D DLLs: unverified as removable, left in place ---
REM QWidget 전용 앱이라 불필요할 가능성이 높으나 미검증. 활성화 전 실제 실행
REM 확인 필요. 합계 약 39.5MB 절감.
REM     del "%DEST%\opengl32sw.dll"
REM     del "%DEST%\dxcompiler.dll"
REM     del "%DEST%\D3Dcompiler_47.dll"
REM     del "%DEST%\dxil.dll"

echo Copying sensor_host publish output ...
mkdir "%DEST%\sensor_host"
robocopy "%SRC_SENSOR%" "%DEST%\sensor_host" sensor_host.exe MonoPosixHelper.dll libMonoPosixHelper.dll /R:2 /W:2 /NFL /NDL /NJH /NP
if errorlevel 8 (
    echo [ERROR] robocopy failed copying sensor_host files, exit code %errorlevel%
    exit /b 1
)

echo.
echo === Package summary ===
for /f %%C in ('powershell -NoProfile -Command "(Get-ChildItem -Recurse -File -Path '%DEST%' | Measure-Object).Count"') do set FILECOUNT=%%C
for /f %%S in ('powershell -NoProfile -Command "'{0:N1} MB' -f ((Get-ChildItem -Recurse -File -Path '%DEST%' | Measure-Object -Property Length -Sum).Sum / 1MB)"') do set FILESIZE=%%S
echo Files: %FILECOUNT%
echo Size:  %FILESIZE%
echo Output: %DEST%

endlocal
