@echo off
setlocal enableextensions

rem ============================================================
rem  Eragon [SLES-54160] [FullRUS] launcher
rem  Edit EXE / ELF / LOGDIR below if your paths change.
rem ============================================================

set "EXE=C:\Users\Leon510\Desktop\Soft\Projects\PS2Recomp-main\out\build\ps2xRuntime\Debug\ps2EntryRunner.exe"
set "ELF=E:\Eragon [SLES-54160] [FullRUS]\SLES_541.60"
set "LOGDIR=C:\Users\Leon510\AppData\Local\Temp\opencode"

if not exist "%EXE%" (
    echo [ERROR] Runner not found:
    echo         %EXE%
    echo         Build it first: cmake --build ... --target ps2EntryRunner
    pause
    exit /b 1
)

if not exist "%ELF%" (
    echo [ERROR] Game ELF not found:
    echo         %ELF%
    pause
    exit /b 1
)

if not exist "%LOGDIR%" mkdir "%LOGDIR%"

for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value 2^>nul') do set "STAMP=%%I"
set "STAMP=%STAMP:~0,8%-%STAMP:~8,6%"

echo ============================================
echo  Runner : %EXE%
echo  ELF    : %ELF%
echo  Logs   : %LOGDIR%\run_%STAMP%.log / .err
echo ============================================
echo.
echo Starting game... (close the game window to exit)
echo.

pushd "%~dp0out\build\ps2xRuntime\Debug"
"%EXE%" "%ELF%" > "%LOGDIR%\run_%STAMP%.log" 2> "%LOGDIR%\run_%STAMP%.err"
set "RC=%ERRORLEVEL%"
popd

echo.
echo Game exited with code %RC%.
echo Logs saved to:
echo   %LOGDIR%\run_%STAMP%.log
echo   %LOGDIR%\run_%STAMP%.err
pause
endlocal
