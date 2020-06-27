#pragma once

#include "structs.h"
#include <stdint.h>

extern "C" {
#include <libavutil/avassert.h>
}

extern const HWAccel hwaccels[];
extern const OptionDef options[];

inline const OptionDef* find_option(const OptionDef* po, const char* name)
{
    const char* p = strchr(name, ':');
    int len = p ? p - name : strlen(name);

    while (po->name) {
        if (!strncmp(name, po->name, len) && strlen(po->name) == len)
            break;
        po++;
    }
    return po;
}

inline void check_options(const OptionDef* po)
{
    while (po->name) {
        if (po->flags & OPT_PERFILE)
            av_assert0(po->flags & (OPT_INPUT | OPT_OUTPUT));
        po++;
    }
}

/**
 * Parse the command line arguments.
 *
 * @param optctx an opaque options context
 * @param argc   number of command line arguments
 * @param argv   values of command line arguments
 * @param options Array with the definitions required to interpret every
 * option of the form: -option_name [argument]
 * @param parse_arg_function Name of the function called to process every
 * argument without a leading option name flag. NULL if such arguments do
 * not have to be processed.
 */
void parse_options(void* optctx, int argc, char** argv, const OptionDef* options,
                   void (*parse_arg_function)(void* optctx, const char*));

/**
 * Parse one given option.
 *
 * @return on success 1 if arg was consumed, 0 otherwise; negative number on error
 */
int parse_option(void* optctx, const char* opt, const char* arg,
                 const OptionDef* options);









/**
 * Print the program banner to stderr. The banner contents depend on the
 * current version of the repository and of the libav* libraries used by
 * the program.
 */
void show_banner(int argc, char** argv, const OptionDef* options);

/**
 * Print the version of the program to stdout. The version message
 * depends on the current versions of the repository and of the libav*
 * libraries.
 * This option processing function does not utilize the arguments.
 */
int show_version(void* optctx, const char* opt, const char* arg);

/**
 * Print the build configuration of the program to stdout. The contents
 * depend on the definition of FFMPEG_CONFIGURATION.
 * This option processing function does not utilize the arguments.
 */
int show_buildconf(void* optctx, const char* opt, const char* arg);

/**
 * Print the license of the program to stdout. The license depends on
 * the license of the libraries compiled into the program.
 * This option processing function does not utilize the arguments.
 */
int show_license(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the formats supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_formats(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the muxers supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_muxers(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the demuxer supported by the
 * program (including devices).
 * This option processing function does not utilize the arguments.
 */
int show_demuxers(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the devices supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_devices(void* optctx, const char* opt, const char* arg);

#if CONFIG_AVDEVICE
/**
 * Print a listing containing autodetected sinks of the output device.
 * Device name with options may be passed as an argument to limit results.
 */
int show_sinks(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing autodetected sources of the input device.
 * Device name with options may be passed as an argument to limit results.
 */
int show_sources(void* optctx, const char* opt, const char* arg);
#endif

/**
 * Print a listing containing all the codecs supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_codecs(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the decoders supported by the
 * program.
 */
int show_decoders(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the encoders supported by the
 * program.
 */
int show_encoders(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the filters supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_filters(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the bit stream filters supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_bsfs(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the protocols supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_protocols(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the pixel formats supported by the
 * program.
 * This option processing function does not utilize the arguments.
 */
int show_pix_fmts(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the standard channel layouts supported by
 * the program.
 * This option processing function does not utilize the arguments.
 */
int show_layouts(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the sample formats supported by the
 * program.
 */
int show_sample_fmts(void* optctx, const char* opt, const char* arg);

/**
 * Print a listing containing all the color names and values recognized
 * by the program.
 */
int show_colors(void* optctx, const char* opt, const char* arg);


/**
 * Show help for all options with given flags in class and all its
 * children.
 */
void show_help_children(const AVClass* class_, int flags);

/**
 * Per-fftool specific help handler. Implemented in each
 * fftool, called by show_help().
 */
void show_help_default(const char* opt, const char* arg);

/**
 * Generic -h handler common to all fftools.
 */
int show_help(void* optctx, const char* opt, const char* arg);
void show_usage(void);

/**
 * Print help for all options matching specified flags.
 *
 * @param options a list of options
 * @param msg title of this group. Only printed if at least one option matches.
 * @param req_flags print only options which have all those flags set.
 * @param rej_flags don't print options which have any of those flags set.
 * @param alt_flags print only options that have at least one of those flags set
 */
void show_help_options(const OptionDef* options, const char* msg, int req_flags,
                       int rej_flags, int alt_flags);



#if CONFIG_AVDEVICE
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE                                                                                \
    { "sources"    , OPT_EXIT | HAS_ARG, { .func_arg = show_sources },                                                  \
      "list sources of the input device", "device" },                                                                   \
    { "sinks"      , OPT_EXIT | HAS_ARG, { .func_arg = show_sinks },                                                    \
      "list sinks of the output device", "device" },                                                                    \

#else
#define CMDUTILS_COMMON_OPTIONS_AVDEVICE
#endif

#define CMDUTILS_COMMON_OPTIONS                                                                                         \
    { "L",           OPT_EXIT,             { show_license },     "show license" },                          \
    { "h",           OPT_EXIT,             { show_help },        "show help", "topic" },                    \
    { "?",           OPT_EXIT,             { show_help },        "show help", "topic" },                    \
    { "help",        OPT_EXIT,             { show_help },        "show help", "topic" },                    \
    { "-help",       OPT_EXIT,             { show_help },        "show help", "topic" },                    \
    { "version",     OPT_EXIT,             { show_version },     "show version" },                          \
    { "buildconf",   OPT_EXIT,             { show_buildconf },   "show build configuration" },              \
    { "formats",     OPT_EXIT,             { show_formats },     "show available formats" },                \
    { "muxers",      OPT_EXIT,             { show_muxers },      "show available muxers" },                 \
    { "demuxers",    OPT_EXIT,             { show_demuxers },    "show available demuxers" },               \
    { "devices",     OPT_EXIT,             { show_devices },     "show available devices" },                \
    { "codecs",      OPT_EXIT,             { show_codecs },      "show available codecs" },                 \
    { "decoders",    OPT_EXIT,             { show_decoders },    "show available decoders" },               \
    { "encoders",    OPT_EXIT,             { show_encoders },    "show available encoders" },               \
    { "bsfs",        OPT_EXIT,             { show_bsfs },        "show available bit stream filters" },     \
    { "protocols",   OPT_EXIT,             { show_protocols },   "show available protocols" },              \
    { "filters",     OPT_EXIT,             { show_filters },     "show available filters" },                \
    { "pix_fmts",    OPT_EXIT,             { show_pix_fmts },    "show available pixel formats" },          \
    { "layouts",     OPT_EXIT,             { show_layouts },     "show standard channel layouts" },         \
    { "sample_fmts", OPT_EXIT,             { show_sample_fmts }, "show available audio sample formats" },   \
    { "colors",      OPT_EXIT,             { show_colors },      "show available color names" },            \
    { "loglevel",    HAS_ARG,              { opt_loglevel },     "set logging level", "loglevel" },         \
    { "v",           HAS_ARG,              { opt_loglevel },     "set logging level", "loglevel" },         \
    { "report",      0,                    { (void*)opt_report },            "generate a report" },                     \
    { "max_alloc",   HAS_ARG,              { opt_max_alloc },    "set maximum size of a single allocated block", "bytes" }, \
    { "cpuflags",    HAS_ARG | OPT_EXPERT, { opt_cpuflags },     "force specific cpu flags", "flags" },     \
    { "hide_banner", OPT_BOOL | OPT_EXPERT, {&hide_banner},     "do not show program banner", "hide_banner" },          \
    CMDUTILS_COMMON_OPTIONS_AVDEVICE                                                                                    \



