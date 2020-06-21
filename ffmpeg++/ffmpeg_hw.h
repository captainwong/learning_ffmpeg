#pragma once

#include "config.h"
#include "ffmpeg.h"


enum HWAccelID {
    HWACCEL_NONE = 0,
    HWACCEL_AUTO,
    HWACCEL_GENERIC,
    HWACCEL_VIDEOTOOLBOX,
    HWACCEL_QSV,
    HWACCEL_CUVID,
};

struct HWAccel {
    const char* name;
    int (*init)(AVCodecContext* s);
    enum HWAccelID id;
    enum AVPixelFormat pix_fmt;
};

struct HWDevice {
    const char* name;
    enum AVHWDeviceType type;
    AVBufferRef* device_ref;
};

HWDevice* hw_device_get_by_name(const char* name);
int hw_device_init_from_string(const char* arg, HWDevice** dev);
void hw_device_free_all(void);

int hw_device_setup_for_decode(InputStream* ist);
int hw_device_setup_for_encode(OutputStream* ost);

int hwaccel_decode_init(AVCodecContext* avctx);
