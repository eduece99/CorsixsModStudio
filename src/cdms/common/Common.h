#pragma once

#include "frame/Construct.h"
#include "Utility.h"
#include "strconv.h"
#include "strings.h"
#include <wx/wxprec.h>

// Debug memory tracking disabled — #define new conflicts with C++17 headers
#if defined(_DEBUG) && defined(_MSC_VER)
#include <crtdbg.h>
#endif
