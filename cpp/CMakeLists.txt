cmake_minimum_required(VERSION 3.21)

project(MonitorWidgetCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(monitor_widget WIN32
    src/app/main.cpp
    src/collectors/cpu_mem_collector.h
    src/collectors/cpu_mem_collector.cpp
    src/collectors/d3dkmt_process_collector.h
    src/collectors/d3dkmt_process_collector.cpp
    src/collectors/disk_collector.h
    src/collectors/disk_collector.cpp
    src/collectors/drive_collector.h
    src/collectors/drive_collector.cpp
    src/collectors/dxcore_process_collector.h
    src/collectors/dxcore_process_collector.cpp
    src/collectors/gpu_adapter_memory_collector.h
    src/collectors/gpu_adapter_memory_collector.cpp
    src/collectors/gpu_collector.h
    src/collectors/gpu_collector.cpp
    src/collectors/gpu_process_collector.h
    src/collectors/gpu_process_collector.cpp
    src/collectors/net_collector.h
    src/collectors/net_collector.cpp
    src/collectors/net_etw_collector.h
    src/collectors/net_etw_collector.cpp
    src/collectors/net_process_collector.h
    src/collectors/net_process_collector.cpp
    src/collectors/process_collector.h
    src/collectors/process_collector.cpp
    src/collectors/server_detector.h
    src/collectors/server_detector.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
    src/ui/layout_utils.h
    src/ui/style_tokens.h
    src/ui/net_tooltip_helper.h
    src/ui/net_tooltip_helper.cpp
    src/ui/sidebar_item.h
    src/ui/sidebar_item.cpp
    src/ui/drive_row.h
    src/ui/drive_row.cpp
    src/ui/process_row.h
    src/ui/process_row.cpp
    src/ui/cleanup_dialog.h
    src/ui/cleanup_dialog.cpp
    src/ui/dialogs/location_selection_dialog.h
    src/ui/dialogs/location_selection_dialog.cpp
    src/ui/dialogs/speed_test_dialog.h
    src/ui/dialogs/speed_test_dialog.cpp
    src/ui/pages/process_list_page.h
    src/ui/pages/process_list_page.cpp
    src/ui/pages/gpu_page.h
    src/ui/pages/gpu_page.cpp
    src/ui/pages/disk_page.h
    src/ui/pages/disk_page.cpp
    src/ui/pages/net_page.h
    src/ui/pages/net_page.cpp
    src/ui/pages/server_page.h
    src/ui/pages/server_page.cpp
    src/ui/pages/dashboard_page.h
    src/ui/pages/dashboard_page.cpp
    src/ui/widgets/daily_forecast_widget.h
    src/ui/widgets/daily_forecast_widget.cpp
    src/ui/widgets/weather_card_widget.h
    src/ui/widgets/weather_card_widget.cpp
    src/ui/widgets/hourly_graph_widget.h
    src/ui/widgets/hourly_graph_widget.cpp
    src/engine/stats_engine.h
    src/engine/stats_engine.cpp
    src/services/weather_service.h
    src/services/weather_service.cpp
    src/services/location_manager.h
    src/services/location_manager.cpp
    src/api/weather_api_client.h
    src/api/weather_api_client.cpp
    src/services/cleanup_service.h
    src/services/cleanup_service.cpp
    src/services/monitor_control_service.h
    src/services/monitor_control_service.cpp
    src/services/process_control_service.h
    src/services/process_control_service.cpp
    src/services/instance_ipc.h
    src/services/instance_ipc.cpp
    src/services/sensor_host_client.h
    src/services/sensor_host_client.cpp
    src/services/status_writer.h
    src/services/status_writer.cpp
    src/services/speed_test_service.h
    src/services/speed_test_service.cpp
    src/utils/win_utils.h
    src/utils/win_utils.cpp
    src/utils/logger.h
    src/utils/logger.cpp
    src/utils/geo_utils.h
    src/utils/geo_utils.cpp
    src/gpu/gpu_backend.h
    src/gpu/dxgi_backend.h
    src/gpu/dxgi_backend.cpp
    src/gpu/dxcore_min.h
    src/gpu/dxcore_backend.h
    src/gpu/dxcore_backend.cpp
    src/gpu/d3dkmt_backend.h
    src/gpu/d3dkmt_backend.cpp
    src/gpu/nvml_backend.h
    src/gpu/nvml_backend.cpp
    src/models/system_stats.h
    src/models/weather_data.h
    src/models/capability_flags.h
    src/models/process_info.h
    src/models/speed_test_result.h
    src/resources/resources.qrc
    src/resources/win_admin.rc
)

target_include_directories(monitor_widget PRIVATE src)

# win_admin.rc가 RT_MANIFEST로 app.manifest를 임베드하는데, CMake는 .rc가
# 참조하는 파일을 의존성으로 추적하지 않는다. 이게 없으면 app.manifest를
# 수정해도 리소스가 재컴파일되지 않아 옛 매니페스트가 exe에 그대로 남는다
# (asInvoker로 바꿔도 requireAdministrator가 유지되어 ContextHub에서 740 실패).
set_property(SOURCE src/resources/win_admin.rc APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/app.manifest")

target_link_libraries(monitor_widget
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
)

if (WIN32)
    target_compile_definitions(monitor_widget PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    target_link_libraries(monitor_widget PRIVATE pdh iphlpapi psapi PowrProf dxgi ws2_32 shell32 ole32 oleaut32 dxva2 wbemuuid advapi32 tdh)
endif()

if (WIN32 AND MINGW)
    add_custom_command(TARGET monitor_widget POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/Qt/Tools/mingw1310_64/bin/libstdc++-6.dll"
            "C:/Qt/Tools/mingw1310_64/bin/libgcc_s_seh-1.dll"
            "C:/Qt/Tools/mingw1310_64/bin/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:monitor_widget>
        COMMENT "Copying MinGW runtime DLLs..."
    )
    add_custom_command(TARGET monitor_widget POST_BUILD
        COMMAND "C:/Qt/6.10.1/mingw_64/bin/windeployqt.exe" --no-translations --compiler-runtime "$<TARGET_FILE:monitor_widget>"
        COMMENT "Running windeployqt..."
    )
endif()
