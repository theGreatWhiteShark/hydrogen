#ifndef HYD_CONFIG_H
#define HYD_CONFIG_H

#include <string>

#ifndef QT_BEGIN_NAMESPACE
#    define QT_BEGIN_NAMESPACE
#endif

#ifndef QT_END_NAMESPACE
#    define QT_END_NAMESPACE
#endif

#define H2_USR_PATH ".hydrogen"
#define H2_SYS_PATH "/usr/local/share/hydrogen"
#define CONFIG_PREFIX "/usr/local"

#define H2CORE_VERSION_MAJOR 1
#define H2CORE_VERSION_MINOR 2
#define H2CORE_VERSION_PATCH 0

#define H2CORE_GIT_REVISION 
#define H2CORE_VERSION "1.2.0-beta1"
#define H2CORE_IS_DEVEL_BUILD true


#define MAX_INSTRUMENTS 1000
#define MAX_COMPONENTS  32
#define MAX_NOTES       192
#define MAX_FX          4
#define MAX_BUFFER_SIZE 8192

#define HAVE_SSCANF
#define HAVE_RTCLOCK
#define HAVE_JACK_PORT_RENAME
#define HAVE_EXECINFO_H

#ifndef H2CORE_HAVE_DEBUG
#define H2CORE_HAVE_DEBUG
#endif
#ifndef H2CORE_HAVE_BUNDLE
/* #undef H2CORE_HAVE_BUNDLE */
#endif
#ifndef H2CORE_HAVE_LIBSNDFILE
#define H2CORE_HAVE_LIBSNDFILE
#endif
#ifndef H2CORE_HAVE_LIBARCHIVE
#define H2CORE_HAVE_LIBARCHIVE
#endif
#ifndef H2CORE_HAVE_OSS
/* #undef H2CORE_HAVE_OSS */
#endif
#ifndef H2CORE_HAVE_ALSA
#define H2CORE_HAVE_ALSA
#endif
#ifndef H2CORE_HAVE_JACK
#define H2CORE_HAVE_JACK
#endif
#ifndef H2CORE_HAVE_LASH
/* #undef H2CORE_HAVE_LASH */
#endif
#ifndef H2CORE_HAVE_OSC
#define H2CORE_HAVE_OSC
#endif
#ifndef H2CORE_HAVE_PORTAUDIO
/* #undef H2CORE_HAVE_PORTAUDIO */
#endif
#ifndef H2CORE_HAVE_PORTMIDI
/* #undef H2CORE_HAVE_PORTMIDI */
#endif
#ifndef H2CORE_HAVE_COREAUDIO
/* #undef H2CORE_HAVE_COREAUDIO */
#endif
#ifndef H2CORE_HAVE_COREMIDI
/* #undef H2CORE_HAVE_COREMIDI */
#endif
#ifndef H2CORE_HAVE_PULSEAUDIO
#define H2CORE_HAVE_PULSEAUDIO
#endif
#ifndef H2CORE_HAVE_LRDF
/* #undef H2CORE_HAVE_LRDF */
#endif
#ifndef H2CORE_HAVE_LADSPA
#define H2CORE_HAVE_LADSPA
#endif
#ifndef H2CORE_HAVE_RUBBERBAND
/* #undef H2CORE_HAVE_RUBBERBAND */
#endif

#endif
