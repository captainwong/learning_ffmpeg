#/bin/bash

mkdir -p build/mac
cd build/mac
flags=`pkg-config --libs --cflags libavcodec libavdevice libavformat libavutil`
build=`clang++ -std=c++11 -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp ${flags}`
run=`./006.record_pcm avfoundation :0 006.out.pcm`

clang++ -std=c++11 -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp ${flags} && ./006.record_pcm avfoundation :0 006.out.pcm
#${build} && ${run}

