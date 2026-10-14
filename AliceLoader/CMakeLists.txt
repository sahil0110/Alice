cmake_minimum_required(VERSION 3.19)
project(AliceLoader LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

qt_add_executable(AliceLoader WIN32 loader_main.cpp)

target_link_libraries(AliceLoader PRIVATE Qt6::Core)

# Suppress auto-manifest — AliceLoader runs asInvoker (no elevation needed)
if(MSVC)
    target_link_options(AliceLoader PRIVATE /MANIFEST:NO)
    target_sources(AliceLoader PRIVATE AliceLoader.rc)
endif()

install(TARGETS AliceLoader RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
