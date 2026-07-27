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
    /XF "*.txt" "Makefile" "CMakeCache.txt" "cmake_install.cmake" "*.cmake" "*.pdb" "*.ilk" "*.obj" "status.json" ^
    /R:2 /W:2 /NFL /NDL /NJH /NP
if errorlevel 8 (
    echo [ERROR] robocopy failed copying app files, exit code %errorlevel%
    exit /b 1
)

echo Removing unused GL/D3D DLLs ...
REM --- GL/D3D DLLs: 검증 완료, 삭제 ---
REM windeployqt가 Qt Quick/RHI 백엔드용으로 무조건 복사하는 DLL들이다.
REM 이 앱은 QWidget 전용이라 해당 경로를 타지 않는다:
REM   - cpp/src 전체에 QOpenGL*/QtQuick/QQuick*/QSurfaceFormat 사용 0건
REM   - cpp/CMakeLists.txt 링크 Qt 모듈은 Core/Gui/Widgets/Network 4개뿐
REM     (Qt6::Quick, Qt6::OpenGL, Qt6::Qml 링크 없음)
REM 합계 약 38.8MB 절감. 삭제 후 실제 기동 확인 완료.
REM 향후 QtQuick/QOpenGLWidget을 도입하면 이 블록을 되돌릴 것.
for %%D in (opengl32sw.dll dxcompiler.dll D3Dcompiler_47.dll dxil.dll) do (
    if exist "%DEST%\%%D" del /q "%DEST%\%%D"
)

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
