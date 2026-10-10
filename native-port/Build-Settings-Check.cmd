@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cmake -S "%~dp0.." -B "%~dp0..\build-full-native-dx12" -DENABLE_TESTS=ON
if errorlevel 1 exit /b 1
cmake --build "%~dp0..\build-full-native-dx12" --target shadps4_settings_test --parallel 6
if errorlevel 1 exit /b 1
"%~dp0..\build-full-native-dx12\tests\shadps4_settings_test.exe"
set "task_test_result=%errorlevel%"
cmake -S "%~dp0.." -B "%~dp0..\build-full-native-dx12" -DENABLE_TESTS=OFF
if errorlevel 1 exit /b 1
exit /b %task_test_result%
