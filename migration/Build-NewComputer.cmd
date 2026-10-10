@echo off
setlocal
set "PORT_FORK=%~dp0.."
set "PORT_WORK=%~dp0..\.."
set "PORT_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PORT_VSWHERE%" exit /b 2
for /f "usebackq delims=" %%I in (`"%PORT_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PORT_VS=%%I"
if not defined PORT_VS exit /b 3
call "%PORT_VS%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 4
set "PATH=%PORT_VS%\VC\Tools\Llvm\x64\bin;%PORT_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PORT_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
for /f "delims=" %%I in ('where python') do if not defined PORT_PYTHON set "PORT_PYTHON=%%I"
if not defined PORT_PYTHON exit /b 5
cmake -S "%PORT_FORK%" -B "%PORT_FORK%\build-migration-core" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS" "-DCMAKE_CXX_FLAGS=/EHsc /DWIN32 /D_WINDOWS" "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG" "-DPython3_EXECUTABLE=%PORT_PYTHON%" -DENABLE_NATIVE_DX12_CORE=ON -DENABLE_TESTS=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_UPDATER=OFF "-DSHADPS4_EXTERNALS_DIR=%PORT_WORK%\shadps4-xess-src\externals" "-DSHADPS4_DXC_INCLUDE_DIR=%PORT_WORK%\dxc-sdk\inc"
if errorlevel 1 exit /b %errorlevel%
if not defined SHADPS4_BUILD_JOBS for /f "delims=" %%I in ('powershell -NoProfile -Command "$physical = 0; foreach($cpu in (Get-CimInstance Win32_Processor)){$physical += $cpu.NumberOfCores}; $free = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory; [Math]::Max(1,[Math]::Min($physical - 2,[Math]::Floor($free / 1572864)))"') do set "SHADPS4_BUILD_JOBS=%%I"
if not defined SHADPS4_BUILD_JOBS set "SHADPS4_BUILD_JOBS=6"
cmake --build "%PORT_FORK%\build-migration-core" --target shadps4 --parallel %SHADPS4_BUILD_JOBS%
exit /b %errorlevel%
