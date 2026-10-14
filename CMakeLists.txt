cmake_minimum_required(VERSION 3.19)
project(Alice LANGUAGES CXX)
# Prevent Qt Creator's maintenance-tool checker from marking Qt6 as NOT FOUND
# even when all required files are present on disk.
set(QT_CREATOR_ENABLE_MAINTENANCE_TOOL_PROVIDER OFF CACHE BOOL "" FORCE)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network WebEngineWidgets)

# Generate Alice.manifest with the correct execution level for the build type
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ALICE_EXECUTION_LEVEL "requireAdministrator")
else()
    set(ALICE_EXECUTION_LEVEL "asInvoker")
endif()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Alice.manifest.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Alice.manifest"
    @ONLY
)
# Let the Windows resource compiler find Alice.manifest in the build directory
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_BINARY_DIR}\"")

qt_standard_project_setup()
qt_add_executable(Alice
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mainwindow_ui.cpp
    mainwindow_ai.cpp
    mainwindow_settings.cpp
    mainwindow_history.cpp
    mainwindow_tasks.cpp
    mainwindow_profiles.cpp
    mainwindow_sequences.cpp
    bubbleedit.h
    commands.cpp
    commands.h
    webbrowser.cpp
    webbrowser.h
    aibridge.cpp
    aibridge.h
    chathud.cpp
    chathud.h
    theme.cpp
    theme.h
    spinner.cpp
    spinner.h
    toast.cpp
    toast.h
    animatedstack.cpp
    animatedstack.h
    mediactrl.cpp
    mediactrl.h
    Alice.rc
)
target_link_libraries(Alice
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Qt6::WebEngineWidgets
        ole32
        oleaut32
        shell32      # SHGetKnownFolderPath, IShellLink (used in mainwindow_settings.cpp)
        advapi32     # OpenSCManager, OpenService, token APIs
)
# Always embed file/line/function in qDebug context — needed for Alice's structured log
target_compile_definitions(Alice PRIVATE QT_MESSAGELOGCONTEXT)
# Disable auto-generated manifest — we embed our own via Alice.rc
if(MSVC)
    target_link_options(Alice PRIVATE /MANIFEST:NO)
endif()
install(TARGETS Alice
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
qt_generate_deploy_app_script(
    TARGET Alice
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# ── Sub-projects ──────────────────────────────────────────────────────────────
add_subdirectory(AliceLoader)   # Qt6 logon helper — placed in shell:startup via .lnk
add_subdirectory(AliceService)  # Win32 service — listens on pipe, launches Alice.exe