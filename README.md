# Learning FFmpeg

Most code was <del>stolen</del> liberally  borrowed from [dranger](http://dranger.com/ffmpeg/ffmpeg.html) and [雷宵骅的博客](https://blog.csdn.net/leixiaohua1020) , but with modifications.

## FFmpeg version

prebuild by [zeranoe](https://ffmpeg.zeranoe.com/builds/), 4.2.2, 20200311-36aaee2, Windows 32-bit, linking [Static](https://ffmpeg.zeranoe.com/builds/win32/static/ffmpeg-20200311-36aaee2-win32-static.zip), [Shared](https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-20200311-36aaee2-win32-shared.zip) and [Dev](https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-20200311-36aaee2-win32-dev.zip)

## SDL version

[2.0.12-win32-x86](http://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip)

## Ubuntu 16.04 编译 FFmpeg

```bash
# 得安装，否则找不到 alsa
apt install libasound2-dev

```

## Ubuntu 16.04 开启 `rtmp` 服务

* 已安装 `nginx 1.16.1`
* 安装 `nginx-module-rtmp`
    ```bash
    sudo apt install nginx-module-rtmp
    ```
* 编辑 `/etc/nginx/nginx.conf`，追加

    ```conf
    rtmp {
            server {
                    listen 10086;
                    publish_time_fix on;
                    application live {
                            live on;
                            allow publish all;
                            allow play all;
                    }
            }
    }
    ```
* 重启 `nginx`
    ```bash
    sudo systemctl reload nginx.service
    ```
* 使用 `ffmpeg` 推流：
    ```bash
    ffmpeg -re -i test.mp4 -acodec aac -f rtmp://192.168.1.168:10086/live/test
    ```
* 随便找个能播放 `rtmp` 流的播放器测试下。

## FFmpeg转码命令笔记

### 音频提取、播放

1. AAC
    
    `ffmpeg -i xyj.mkv -acodec aac -vn xyj.aac`

2. MP3

    `ffmpeg -i xyj.mkv -acodec mp3 -vn xyj.mp3`

4. PCM

    `ffmpeg -i xyj.mkv -vn -ar 44100 -ac 2 -f s16le xyj.pcm`

    ```txt
    -ar 44100: sample rate 44100
    -ac 2: channels 2
    -f s16le: signed integer, 16bit , little endien
    ```

    `ffplay -ar 44100 -ac 2 -f s16le xyj.pcm`

    如果不指定 sample rate 和 channels ，播放出来有杂音

### 视频提取、播放


1. H264

    `ffmpeg -i xyj.mkv -f h264 -bsf: h264_mp4toannexb xyj.h264`

    `ffplay xyj.h264`

2. YUV

    `ffmpeg -t 00:00:10 -i xyj.mkv -an -f rawvideo -pix_fmt yuv420p -s 800x600 xyj.yuv`

    ```txt
    -t 00:00:10: 提取前10秒
    -an: no audio
    -f rawvideo: 不封装
    -pix_fmt yuv420p: 编码格式yuv420p
    -s 800x600: 视频宽高800x600，如果不指定大小，等会没法播放：两边都-s 800x600才能正确播放
    ```

    `ffplay -pix_fmt yuv420p -s 800x600 xyj.yuv`

3. RGB

    `ffmpeg -t 00:00:10 -i xyj.mkv -an -f rawvideo -pix_fmt rgb24 -s 800x600 xyj.rgb`

    `ffplay -pix_fmt yuv420p -s 800x600 xyj.rgb`

### 批量转换技巧

1. 批量转换 `ogg` 到 `mp4`

    希望将当前目录下的所有 `ogg` 视频转换为 `mp4` 格式（不删除原文件），可以使用脚本：
    ```bash
    #/bin/bash
    for f in *.ogg; do
    file="${f##*/}"
    out="${file%.Ogg}".mp4
    echo "file=$file, out=$out"
    ffmpeg.exe -i $file $out
    done
    ```

2. 批量转换 `mp3` 到 `ogg`

    假设有 `/music/mp3` 文件夹存放 `mp3` 音频，希望将这些音频文件批量转换为 `ogg` 格式（不删除原文件）并存储到 `/music/ogg`文件夹内（保持文件名不变，将后缀改为 `ogg`），可以用命令：
    ```bash
    find /music/mp3 -iname "*.mp3" -print0 | xargs -0 -i --max-procs 0 sh -c 'ffmpeg.exe -hide_banner -y -loglevel warning -vn -i "{}" "/music/ogg/`basename "{}" .mp3`.ogg"'
    ```

    **xargs 参数**
    * -0

        文件名可能有空格，因此使用 `-0` 告诉 `xargs` 该参数以 `null` 结尾
    * --max-procs 0

        不限制最大进程数，尽可能压榨CPU。毕竟是小文件，瓶颈不在文件 `IO` 而在 `CPU`。当然如果处理视频文件单进程跑就行了。

    **ffmpeg 参数**
    * -vn

        有些mp3有封面，不加 `-vn` 则生成的 `ogg` 会带有视频信息，导致 `欧卡2` 的音乐播放器播不出来。。。

