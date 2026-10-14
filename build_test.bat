@echo off
echo Testing build after merge...
echo.
cd "C:\Users\JASKA\OneDrive\Documents\QT\Alice"
echo Checking if cmake is available...
cmake --version 2>nul
if %errorlevel% equ 0 (
    echo CMake found - attempting build
    cmake --build . --config Release
) else (
    echo CMake not found, trying to use MSVC directly
    echo This requires Visual Studio command line tools
)
echo.
pause