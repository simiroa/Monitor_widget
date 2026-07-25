# Monitor Widget (모니터 위젯) 🖥️❄️

시스템 상태와 로컬 날씨를 한눈에 파악할 수 있는 깔끔하고 강력한 데스크탑 위젯입니다. C++ Qt6 기반의 고성능 프론트엔드와 .NET 8 기반의 정밀 센서 백엔드로 구성되어 있습니다.

![Monitor Widget Preview](https://github.com/simiroa/Monitor_widget/blob/master/docs/preview.png?raw=true)

---

## 📥 다운로드 및 설치

**[👉 최신 버전 다운로드 (Github Releases)](https://github.com/simiroa/Monitor_widget/releases)**

| 파일명 | 유형 | 특징 |
|--------|------|------|
| `MonitorWidget_Setup_YYYYMMDD.exe` | **설치형** | 편리한 설치 마법사, 시작 메뉴/바탕화면 바로가기 자동 생성 |
| `MonitorWidget_Portable_YYYYMMDD.zip` | **무설치형** | 설치 없이 압축만 풀어서 즉시 실행 가능 |

> **⚠️ 중요: 관리자 권한 필수**
>
> 하드웨어 온도, 전력 소모(W), 로드율 등의 정밀한 수치를 읽기 위해서는 반드시 **관리자 권한**으로 실행해야 합니다. 권한이 없을 경우 수치가 `0`으로 표시되거나 제대로 작동하지 않을 수 있습니다.

---

## ✨ 핵심 기능

### 1. 하드웨어 모니터링 (PC Health)

- **CPU**: 실시간 점유율, 상세 코어/스레드 정보, 베이스 및 현재 클럭 조회
- **GPU**: NVIDIA/AMD 실시간 모니터링 (온도, 로드율, VRAM 점유율, 전력 소모량)
- **RAM**: 메모리 사용량, 가용량 및 동작 속도(MHz) 표시
- **I/O**: 디스크 및 네트워크 실시간 트래픽 모니터링
- **Process List**: 리소스 점유율 높은 프로세스 확인 및 관리(종료/일시정지)

### 2. 가상 모니터 및 디스플레이 제어 (Display Control)

- **가상 모니터(Virtual Monitor)**: 실제 모니터가 꺼진 상태에서도 원격 접속이 가능하도록 가상 모니터 드라이버 통합 지원
- **스마트 제어**: 야간 모드(Night Mode), HDR 토글, 모니터 절전 및 디스플레이 설정 바로가기
- **정밀 복구**: 앱 종료 시 이전의 밝기, 주사율, 디스플레이 배치를 감지하여 원상 복구

### 3. 스마트 날씨 서비스 (Weather)

- **위치 자동 감지**: IP 기반으로 현재 위치 자동 파악
- **기상 특보 알림**: 한파, 폭염, 황사 등 주의보 발생 시 실시간 알림
- **직관적인 상태 표시**: 풍속, 습도, 미세먼지 단계별 동적 색상 적용

---

## 🛠 기술 스택 (Developer Info)

- **Frontend**: C++ 17, Qt 6.10 (Modular Architecture)
- **Backend Host**: .NET 8 (Hardware Sensor Core)
- **Utilities**: Win32 API 캡슐화 (WinUtils), Virtual Display Driver (MttVDD)
- **Build Tool**: MinGW-w64, CMake, windeployqt

---

## 📝 라이선스

본 프로젝트는 **MIT License**를 따릅니다. 누구나 자유롭게 수정 및 배포가 가능합니다.
궁금하신 점이나 버그 제보는 Issue 탭을 이용해 주세요!
