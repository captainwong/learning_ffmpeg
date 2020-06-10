#/bin/bash

mkdir -p build/mac
cd build/mac
clang -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp -I/usr/local/ffmpeg/include -L/usr/local/ffmpeg/lib -lavcodec -lavdevice -lavformat -lavutil 
./006.record_pcm
