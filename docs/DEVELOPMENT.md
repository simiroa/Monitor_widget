# Development Guide (개발 가이드)

본 프로젝트는 두 가지 파트(C++ 프론트엔드와 C# 백엔드)로 구성되어 있습니다.

## 📁 프로젝트 구조

```text
monitor_widget_portable/
├── cpp/                # C++ Qt6 기반 위젯 소스 (Frontend)
│   ├── src/
│   │   ├── api/        # 날씨 API 클라이언트
│   │   ├── services/   # 비즈니스 로직 (위치 관리, 날씨 서비스)
│   │   ├── models/     # 데이터 구조체
│   │   └── ui/         # Qt 위젯 및 페이지
│   └── CMakeLists.txt
├── sensor_host/       # .NET 8 기반 센서 데이터 추출기 (Backend)
│   └── Program.cs
├── Release/            # 배포 산출물 (portable/installer)
└── installer/          # Inno Setup 스크립트
```

## 🛠 빌드 환경 설정

### 1. 전제 조건

- **Qt 6.10+**: `mingw_64` 툴체인 포함
- **CMake 3.16+**
- **MinGW-w64**: (GCC 13.1.0 권장)
- **.NET 8 SDK**: `sensor_host` 빌드용

### 2. 빌드 방법 (C++ Frontend)

빌드 디렉토리는 `cpp/build_new`를 사용합니다:

```powershell
cmake -S cpp -B cpp/build_new -G "MinGW Makefiles"
cmake --build cpp/build_new --config Release --parallel 8
```

- **팁**: 빌드 완료 후 `windeployqt`가 자동 실행되어 필요한 DLL을 `cpp/build_new/`에 배치합니다.

### 3. 배포 패키징 과정

1. `sensor_host`를 `publish` 모드로 빌드합니다.
2. `cpp/build_new`의 출력물과 `sensor_host` 결과물을 `Release/portable`에 모읍니다.
3. `installer/setup.iss`를 Inno Setup으로 빌드하여 `Release/installer`에 설치 파일을 생성합니다.

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "C:\Users\HG\Documents\monitor_widget_portable\installer\setup.iss"
```

### 4. 주의 사항

- 본 앱은 관리자 권한으로 실행됩니다. 빌드 중 파일 잠김 오류가 나면 실행 중인 앱을 먼저 종료하세요.
- `dxcompiler.dll`/`dxil.dll` 경고는 DX12 셰이더를 쓰지 않으면 무시해도 됩니다.

## 🤝 기여 안내

- 새로운 센서 데이터를 추가하려면 `sensor_host/Program.cs`에서 JSON 출력을 수정한 후, `cpp` 쪽의 데이터 파싱 부를 업데이트하십시오.
- UI 스타일 변경은 `cpp/src/ui/style_tokens.h`를 참고하십시오.
