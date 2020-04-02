# Learning FFmpeg

Most code was <del>stolen</del> liberally  borrowed from [dranger](http://dranger.com/ffmpeg/ffmpeg.html) and [雷宵骅的博客](https://blog.csdn.net/leixiaohua1020) , but with modifications.

## FFmpeg version

prebuild by [zeranoe](https://ffmpeg.zeranoe.com/builds/), 4.2.2, 20200311-36aaee2, Windows 32-bit, linking [Static](https://ffmpeg.zeranoe.com/builds/win32/static/ffmpeg-20200311-36aaee2-win32-static.zip), [Shared](https://ffmpeg.zeranoe.com/builds/win32/shared/ffmpeg-20200311-36aaee2-win32-shared.zip) and [Dev](https://ffmpeg.zeranoe.com/builds/win32/dev/ffmpeg-20200311-36aaee2-win32-dev.zip)

## SDL version

[2.0.12-win32-x86](http://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip)

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

