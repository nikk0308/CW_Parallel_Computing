# Parallel Search Project

## Overview
- **C++ server** — handles search queries  
- **C++ console client** — command line interaction  
- **Web client** (Node.js + Express) — browser interface  
- **Locust** — load testing for the server  

---

## Requirements

- [CMake 3.20+](https://cmake.org/download/) — for building C++ projects  
- **C++17 compiler**  
  - Recommended: [MSYS2](https://www.msys2.org/)  
  - After installation, run inside the MSYS2 shell:  
    ```bash
    pacman -S --needed \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-make \
    mingw-w64-ucrt-x86_64-ninja

    ```
  - After this, add to your system PATH:
    ```bash
    C:\msys64\ucrt64\bin
    ```
- [Node.js 18+](https://nodejs.org/en/download/) — for running the web client  
- [Python 3.10+](https://www.python.org/downloads/) — for Locust load testing
  - Don't forget to add to your system PATH important Python folders
  - After installation, run this inside the PowerShell to install Locust
    ```bash
    pip install locust
    ```
> **Note**  
> You should restart your PC after any installation.
---

## Build and Run

### C++ server and client
- Use a folder with only English characters in its path. Open a terminal and run:
  ```bash
  git clone https://github.com/nikk0308/CW_Parallel_Computing.git
  cd CW_Parallel_Computing
  mkdir build
  cd build
  cmake .. -G "MinGW Makefiles" -D CMAKE_BUILD_TYPE=Release -D CMAKE_C_COMPILER=gcc -D CMAKE_CXX_COMPILER=g++
  mingw32-make -j
  ```
- You have created `Bin\search_client.exe` and `Bin\search_server.exe` files
- Test your files:
  - Paste in console the whole server.exe path (my path looks like `D:\CW_Parallel_Computing\build\Bin\search_server.exe`)
  - You have launched the server!
  - Paste in the console the whole client.exe path to launch the client (you can do it many times)
 

### Web client
> **Note**  
> You should start C++ server before starting the web client.
- Go to the project folder (`CW_Parallel_Computing`)
- Paste this code
  ```bash
  cd SrcWeb
  npm install
  node server/proxy.js
  ```
- Open `http://localhost:3000` in your browser

---
## Tests
> **Note**  
> Before any test starts, you must launch the server.

### C++ client test
> **Note**  
> This test represents the peak users concurrency.

- You should have installed the locust program
- Go to your project folder (`CW_Parallel_Computing`)
- Open a terminal and run:
  ```bash
  locust -f Tests/locust_tcp.py
  ```
- Input test settings and start test

### Web client test
> **Note**  
> This test represents the peak search requests concurrency.

> **Note**  
> Before starting this test, launch the web client.
- You should have installed the locust program
- Go to your project folder (`CW_Parallel_Computing`)
- Open a terminal and run:
  ```bash
  locust -f Tests/locust_web.py --host=http://127.0.0.1:3000
  ```
- Input test settings and start test