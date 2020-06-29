#/bin/bash

mkdir -p build/mac
cd build/mac
flags=`pkg-config --libs --cflags libavcodec libavdevice libavfilter libavformat libavresample libavutil libpostproc libswresample libswscale`
#build=`clang++ -std=c++11 -o 006.record_pcm ../../006.record_pcm/006.record_pcm.cpp ${flags}`
#run=`./006.record_pcm avfoundation :0 006.out.pcm`
rm -f ffmpeg++
folder='../../ffmpeg++'
sources="${folder}/ffmpeg.cpp ${folder}/cmdutils.cpp ${folder}/ffmpeg_filter.cpp ${folder}/ffmpeg_hw.cpp ${folder}/ffmpeg_opt.cpp"
clang++ -std=c++20 -o ffmpeg++ ${sources} -I${folder} -lpthread ${flags} && ./ffmpeg++
#${build} && ${run}

