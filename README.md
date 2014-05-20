=== INSTALLATION ===

While a device with USB debugging enabled is plugged in / an emulator is open, navigate to ./Utilities/ and run buildandroid.bat

or

Navigate to ./InstancingAndroid/ and run the following:
android update project -t "android-19" -p .
ndk-build NDK_DEBUG=1 -C .
ant debug -f .
ant installd -f .
