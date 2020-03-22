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

