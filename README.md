# Alice

Alice is a Windows desktop task runner built with C++ and Qt6. It runs AI tasks and automation, and it's built for personal use.

## Features

- App profiles: save settings for different apps and switch between them.
- Sequences: chain several actions together and run them in order.
- Blackout and focus commands: quick commands to hide distractions and focus on one window.
- System tray: runs in the background and can be opened from the tray icon.
- Embedded ChatGPT view: chat with ChatGPT inside the app.
- Auto-start with admin rights: starts on login using a three-part setup (AliceService, AliceLoader, Alice).

## Built with

- C++
- Qt6 (including QtWebEngine)
- CMake
- MSVC (Visual Studio compiler)

## Project structure

- `mainwindow*.cpp`: main UI and feature code
- `AliceService/`: background service for startup
- `AliceLoader/`: loads the main app with the right permissions
- `alice_extension/`: browser extension files
- `theme.*`, `toast.*`, `spinner.*`: UI helpers

## Build

1. Install Qt6 and Visual Studio with the C++ tools.
2. Open the project in Qt Creator, or run:

```
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```


## Author

Jaskaran Singh Ahluwalia ([GitHub](https://github.com/sahil0110))