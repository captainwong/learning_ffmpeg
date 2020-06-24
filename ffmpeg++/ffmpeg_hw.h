#pragma once

#include "structs.h"



HWDevice* hw_device_get_by_name(const char* name);
int hw_device_init_from_string(const char* arg, HWDevice** dev);
void hw_device_free_all(void);

int hw_device_setup_for_decode(InputStream* ist);
int hw_device_setup_for_encode(OutputStream* ost);

int hwaccel_decode_init(AVCodecContext* avctx);
