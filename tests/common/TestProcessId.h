#pragma once

#ifdef _WIN32
#include <process.h>
inline int GetTestProcessId() { return _getpid(); }
#else
#include <unistd.h>
inline int GetTestProcessId() { return getpid(); }
#endif
