@echo off
rem Builds the Arabic Chatterino (Release). Output: build\bin\chatterino.exe
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
set PY=%LOCALAPPDATA%\Programs\Python\Python312\python.exe
"%PY%" tools\ar\translate.py || exit /b 1
if not exist build mkdir build
cd build
"%PY%" -m conans.conan install .. -s build_type=Release -c tools.cmake.cmaketoolchain:generator="NMake Makefiles" --build=missing --output-folder=. || exit /b 1
cmake -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64" -DBUILD_WITH_QT6=ON .. || exit /b 1
nmake || exit /b 1
"C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe" bin\chatterino.exe --release --no-compiler-runtime --no-translations || exit /b 1
echo BUILD OK
