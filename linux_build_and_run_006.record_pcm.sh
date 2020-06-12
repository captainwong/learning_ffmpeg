#/bin/bash

mkdir -p build/linux
cd build/linux
flags=`pkg-config --libs --cflags libavcodec libavdevice libavformat libavutil`
g++ -std=c++11 -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp ${flags} && ./006.record_pcm alsa hw:0 006.out.pcm
