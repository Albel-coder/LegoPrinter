# Для Windows
mkdir build-windows && cd build-windows
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release

# Для Android
mkdir build-android-arm64 && cd build-android-arm64
cmake .. -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21
cmake --build . --config Release