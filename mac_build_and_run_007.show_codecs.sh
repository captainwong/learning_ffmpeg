#/bin/bash

mkdir -p build/mac
cd build/mac
flags=`pkg-config --libs --cflags libavcodec libavutil`
folder='../../006.show_codecs'
sources="${folder}/006.show_codecs"
clang++ -o 006.show_codecs ${sources} -I${folder} ${flags} && ./006.show_codecs

