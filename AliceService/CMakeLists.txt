cmake_minimum_required(VERSION 3.19)
project(AliceService LANGUAGES CXX)

# Pure Win32 — no Qt dependency.
add_executable(AliceService WIN32 service_main.cpp)

target_compile_features(AliceService PRIVATE cxx_std_17)

target_link_libraries(AliceService PRIVATE
    advapi32    # OpenSCManager, CreateService, CreateProcessAsUser, token APIs
    wtsapi32    # WTSQueryUserToken, WTSGetActiveConsoleSessionId, WTSQuerySessionInformation
    userenv     # GetUserProfileDirectoryW, CreateEnvironmentBlock
    shell32     # CommandLineToArgvW
)

# Suppress auto-generated manifest — embed our own asInvoker one
if(MSVC)
    target_link_options(AliceService PRIVATE /MANIFEST:NO)
    target_sources(AliceService PRIVATE AliceService.rc)
endif()

install(TARGETS AliceService RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
