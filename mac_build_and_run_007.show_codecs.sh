#/bin/bash

mkdir -p build/mac
cd build/mac
flags=`pkg-config --libs --cflags libavcodec libavutil`
project='007.show_codecs'
folder=`../../${project}`
sources="${folder}/${project}.cpp"
clang++ -o ${project} ${sources} -I${folder} ${flags} && ./${project}

