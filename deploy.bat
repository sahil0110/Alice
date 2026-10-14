@echo off
setlocal
set BUILD=C:\Users\JASKA\OneDrive\Documents\QT\Alice\build\Desktop_Qt_6_11_0_MSVC2022_64bit-Release
set DEST=C:\Alice

echo Deploying Alice...
"C:\Qt\6.11.0\msvc2022_64\bin\windeployqt.exe" --release "%BUILD%\Alice.exe"

echo Copying new helper exes...
copy /Y "%BUILD%\AliceLoader.exe"   "%DEST%\AliceLoader.exe"
copy /Y "%BUILD%\AliceService.exe"  "%DEST%\AliceService.exe"

echo Done.
pause
