@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
pushd "%~dp0..\..\..\..\.."
work\fsr-sdk-local\sdk\tools\binary_store\glslangValidator.exe -V -S comp -DIS_MACRO_TILED=1 -DBITS_PER_PIXEL=32 work\shadps4-dx12-src\src\video_core\host_shaders\tiling.comp -o work\shadps4-dx12-src\src\video_core\renderer_d3d12\original_tiling.spv
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W3 /DNDEBUG /DFMT_HEADER_ONLY /DNOMINMAX /DWIN32_LEAN_AND_MEAN /Iwork\shadps4-dx12-src\src /Iwork\shadps4-xess-src\build-xess\_deps\fmt-src\include /Iwork\fsr-sdk-local\sdk\tools\ffx_shader_compiler\libs\dxc\inc /Iwork\mesa-dxil-source\src\microsoft\spirv_to_dxil /Iwork\mesa-dxil-source\src\microsoft\compiler work\shadps4-dx12-src\src\video_core\renderer_d3d12\guest_texture_detile_reference_test.cpp work\shadps4-dx12-src\src\video_core\renderer_d3d12\guest_texture_cache.cpp work\shadps4-dx12-src\src\shader_recompiler\backend\dxil\spirv_converter.cpp /Fo:work\shadps4-dx12-src\src\video_core\renderer_d3d12\ /Fe:work\shadps4-dx12-src\src\video_core\renderer_d3d12\guest-detile-reference.exe /link d3d12.lib dxgi.lib ole32.lib
if errorlevel 1 exit /b 1
work\shadps4-dx12-src\src\video_core\renderer_d3d12\guest-detile-reference.exe work\shadps4-dx12-src\src\video_core\renderer_d3d12\original_tiling.spv "%CD%\work\mesa-dxil-build-full\src\microsoft\spirv_to_dxil\spirv_to_dxil.dll"
exit /b %ERRORLEVEL%
