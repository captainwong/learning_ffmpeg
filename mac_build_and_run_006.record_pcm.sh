#/bin/bash

mkdir -p build/mac
cd build/mac
build=`clang++ -std=c++11 -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp -I/usr/local/ffmpeg/include -L/usr/local/ffmpeg/lib -lavcodec -lavdevice -lavformat -lavutil`
run=`./006.record_pcm avfoundation :0 006.out.pcm`

build && run

