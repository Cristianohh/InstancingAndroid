@echo off

set ANDROID_SAMPLE=../InstancingAndroid/

if "%1"=="clean" (
     goto clean
)

REM this command doesn't need to be run every time but is quick and does not
REM seem to have any negative effect
call android update project -t "android-19" -p %ANDROID_SAMPLE%

call ndk-build NDK_DEBUG=1 -C %ANDROID_SAMPLE%
if ERRORLEVEL 2 (
     echo Compilation failed, exiting
     exit /b 2
)
if ERRORLEVEL 1 (
     exit /b 1
)

call ant debug -f %ANDROID_SAMPLE%

REM output results of get-state to a temporary file and then read that
REM back in to a variable. Easiest way to do this in windows .bat files
call adb get-state > 480a53f8-2138-41ae-9d09-00bef8006346.tmp
set /p DEVICE_STATE= < 480a53f8-2138-41ae-9d09-00bef8006346.tmp
del 480a53f8-2138-41ae-9d09-00bef8006346.tmp

REM Check to see if a device is attached. The copy command blocks if there is no
REM device attached.
if "%DEVICE_STATE%" == "device" (
     call ant installd -f %ANDROID_SAMPLE%
)

exit /b 0

:clean
call ndk-build NDK_DEBUG=1 -C %ANDROID_SAMPLE% clean
ant clean -f %ANDROID_SAMPLE%