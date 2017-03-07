// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <Psapi.h>
#include <shellapi.h>
#include <gdiplus.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// TODO: reference additional headers your program requires here
#include <thread>

#include <memory>
#include <map>
#include <list>
#include <mutex>
#include <atomic>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <strsafe.h>

#include <cpprest\json.h>