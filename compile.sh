make clean
qmake Laycan.pro -r -v -spec macx-clang CONFIG+=release CONFIG+=x86_64
qmake
make
bin/release/Laycan.app/Contents/MacOS/Laycan
