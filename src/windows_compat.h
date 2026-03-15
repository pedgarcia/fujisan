#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32

// Prevent common macro conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOSERVICE
#define NOSERVICE
#endif
#ifndef NOMCX
#define NOMCX
#endif
#ifndef NOIME
#define NOIME
#endif
#ifndef NOKANJI
#define NOKANJI
#endif
#ifndef NOHELP
#define NOHELP
#endif
#ifndef NOPROFILER
#define NOPROFILER
#endif
#ifndef NODEFERWINDOWPOS
#define NODEFERWINDOWPOS
#endif

// Include Windows headers first to prevent conflicts
#include <windows.h>

// Tell atari800 headers (netsio.h etc.) that Windows headers are available,
// so they use the Winsock2 path instead of trying to include POSIX <pthread.h>.
#ifndef HAVE_WINDOWS_H
#define HAVE_WINDOWS_H 1
#endif

// Undefine any problematic macros that might conflict with C++ keywords
#ifdef string
#undef string
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

#endif // _WIN32

#endif // WINDOWS_COMPAT_H