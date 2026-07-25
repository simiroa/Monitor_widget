---
trigger: model_decision
description: when c++ qt project build
---

## C++ Qt 프로젝트 빌드 시 주의사항

1. **const 안전성**: 함수 파라미터로 받은 `const` 데이터에 `.replace()`, `.remove()` 등 수정 메서드를 직접 호출하지 말 것. 반드시 `QString(원본).replace(...)` 형태로 복사본을 만들어 사용할 것.

2. **리팩토링 후 검증**: 코드 블록을 이동하거나 삭제할 때, 해당 블록에서 선언된 지역 변수가 다른 곳에서 사용되는지 반드시 확인할 것.

3. **빌드 전 프로세스 종료**: Windows에서 실행 파일이 사용 중이면 덮어쓰기가 불가능하므로, 빌드 오류 발생 시 먼저 해당 프로세스를 종료할 것.

---

1. **Qt 배포 자동화 (DLL 관리)**:
   Windows 환경에서 Qt 빌드 후 실행 시 "Entry Point Not Found"나 "DLL missing" 오류가 잦음.
   반드시 CMakeLists.txt에 `windeployqt` 명령을 `POST_BUILD`로 포함하여 필요한 런타임 라이브러리가 자동으로 복사되도록 할 것.

2. **관리자 권한 필수성**:
   하드웨어 센서 리딩(LPC/SMBus 접근)이 포함된 경우, 일반 권한으로는 수치가 0으로 나오거나 동작하지 않음.
   사용자에게 항상 '관리자 권한' 실행이 필요함을 문서에 명시하고, 가능하면 매니페스트(`requestedExecutionLevel`) 설정을 제안할 것.

3. **비동기 람다 캡처 안전성**:
   `connect()`나 `QTimer::singleShot`에서 람다를 쓸 때, 캡처된 객체(`this` 등)가 실행 시점에 이미 삭제되었을 가능성을 고려할 것.
   특히 네트워크 응답 처리(`QNetworkReply`) 시 `reply->deleteLater()` 호출 위치를 철저히 관리할 것.

4. **Relative Path vs Resource**:
   아이콘이나 폰트 등 외부 리소스는 상대 경로보다 Qt Resource System(`.qrc`)을 사용하거나, 실행 파일 위치를 기준으로 절대 경로를 계산하여(예: `qApp->applicationDirPath()`) 리소스 로딩 실패를 방지할 것.
