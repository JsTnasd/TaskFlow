@echo off
setlocal
if not exist build mkdir build
rc /nologo /i resources /fo build\resources.res resources\resources.rc
cl /nologo /std:c++20 /O2 /EHsc /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN src\main.cpp build\resources.res /Fe:build\TaskFlow.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comdlg32.lib dwmapi.lib winmm.lib msimg32.lib
if errorlevel 1 exit /b 1
echo Built build\TaskFlow.exe
