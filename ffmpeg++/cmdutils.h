#pragma once

#include "config.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
}

/**
 * Register a program-specific cleanup routine.
 */
void register_exit(void (*cb)(int ret));

/**
 * Wraps exit with a program-specific cleanup routine.
 */
void exit_program(int ret) av_noreturn;

/**
 * Initialize dynamic library loading
 */
void init_dynload();

/**
 * Initialize the cmdutils option system, in particular
 * allocate the *_opts contexts.
 */
void init_opts(void);

/**
 * Uninitialize the cmdutils option system, in particular
 * free the *_opts contexts and their contents.
 */
void uninit_opts(void);

