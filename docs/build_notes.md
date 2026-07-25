# Build Notes (Korean)

## Basic Build Flow

- Use the existing build directory: `cpp/build_new`
- Command:

  ```powershell
  cmake --build cpp/build_new --config Release --parallel 8
  ```

- Run (optional verification):

  ```powershell
  Start-Process -FilePath "cpp\\build_new\\monitor_widget.exe"
  ```

## First-Time Configure (If CMakeCache.txt is missing)

```powershell
cmake -S cpp -B cpp/build_new -G "MinGW Makefiles"
cmake --build cpp/build_new --config Release --parallel 8
```

## Common Failures and Fixes

1) `Error: not a CMake build directory (missing CMakeCache.txt)`

- Cause: Running `cmake --build` from `cpp` or `cpp/build`
- Fix: Use `cpp/build_new` or reconfigure with `-S/-B` (see above)

1) `cannot open output file monitor_widget.exe: Permission denied`

- Cause: `monitor_widget.exe` is still running
- Fix:

  ```powershell
  tasklist /FI "IMAGENAME eq monitor_widget.exe"
  taskkill /F /IM monitor_widget.exe
  ```

- If `windeployqt`/`qtpaths` says `Access denied`, close any running app, retry as Admin, and check AV exclusions.

1) Missing icon constants (compile errors like `Icons::kLanguage not found`)

- Cause: Using new icon IDs without adding them to `cpp/src/ui/icons_material.h`
- Fix: Add the icon constants first, then use them in UI files

1) Syntax errors like `expected declaration before '}' token`

- Cause: Unbalanced `namespace {}` or stray `}` in source file
- Fix: Check the top of the `.cpp` file and ensure the namespace block is properly opened/closed

1) `error: redefinition of 'void MainWindow::mouseReleaseEvent(QMouseEvent*)'`

- Cause: 코드 수정 중 함수가 중복 정의됨 (특히 자동화된 코드 교체 시)
- Fix: 파일에서 중복된 함수 정의를 검색하여 하나만 남기고 삭제

1) `error: expected unqualified-id before 'if'` (함수 본문 바깥에 코드)

- Cause: 함수 시그니처 없이 본문만 남아있는 상태 (코드 교체 실패 시 발생)
- Fix: 해당 코드 블록 위에 올바른 함수 시그니처 추가 (예: `void MainWindow::mouseMoveEvent(QMouseEvent *event) {`)

1) `'gcc' is not recognized` / `windres: preprocessing failed`

- Cause: 특정 빌드 디렉토리(`build_release`)의 CMake 캐시가 잘못된 환경을 참조
- Fix: `build_new` 디렉토리 사용 권장. 또는 CMakeCache.txt 삭제 후 재구성

## Known Warnings

- `Warning: Cannot find any version of the dxcompiler.dll and dxil.dll.`
  - This comes from `windeployqt` and is usually safe to ignore unless you rely on DX12 shader compilation.
  - 대응 선택지:
    - 무시: DX12 셰이더 컴파일을 사용하지 않으면 런타임 영향 없음.
    - 해결: `dxcompiler.dll`/`dxil.dll`을 `monitor_widget.exe` 옆에 배치하거나 PATH에 추가.
    - 확인: 실행 시 그래픽 관련 크래시/검은 화면이 없다면 현재 구성으로 충분.
  - 현재 정책: 빌드는 성공했으면 경고는 무시.

### Optional: DX12 warning fix (copy DLLs)

If you want to remove the warning, copy the DLLs from Windows SDK:

```powershell
$dst = "C:\\Users\\HG\\Documents\\monitor_widget_portable\\cpp\\build_new"
$candidates = @(
  "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\D3D\\x64",
  "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\*\\x64"
)
$src = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($src) {
  Copy-Item "$src\\dxcompiler.dll","$src\\dxil.dll" -Destination $dst -Force
} else {
  Write-Host "Windows SDK path not found. Install Windows 10/11 SDK first."
}
```

## Release Packaging

1) Ensure `Release/portable` contains the latest build outputs.
2) Build installer:

```powershell
& "C:\\Program Files (x86)\\Inno Setup 6\\ISCC.exe" "C:\\Users\\HG\\Documents\\monitor_widget_portable\\installer\\setup.iss"
```

3) Outputs:
   - Portable: `Release/portable`
   - Installer: `Release/installer/MonitorWidget_Setup.exe`

## Tips

- Keep build commands consistent; do not use `cpp` or `cpp/build` as build directories.
- If linking fails intermittently, re-check that the app process is fully stopped before rebuilding.
