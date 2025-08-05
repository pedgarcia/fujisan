#ifndef WINDOWS_COMPAT_H
#define WINDOWS_COMPAT_H

#ifdef _WIN32

// Disable NETSIO on Windows - officially not supported on Windows platform
// NetSIO only works with Linux/macOS. Windows users should use Altirra + FujiNet-PC instead
#ifdef NETSIO
#undef NETSIO
#endif

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