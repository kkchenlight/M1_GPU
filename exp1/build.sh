xcrun -sdk macosx metal -c add.metal -o add.air
xcrun -sdk macosx metallib add.air -o add.metallib
clang++ -std=c++17 -lstdc++ -g ./main.cpp -I../metal-cpp -fno-objc-arc -framework Foundation -framework Metal -framework MetalKit  -o ./test
