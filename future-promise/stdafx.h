// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#if _DEBUG
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRTDBG_MAP_ALLOC 1
#include <crtdbg.h>
#endif

/////////////////////////////////

#include <memory>
#include <functional>
#include <queue>

/////////////////////////////////

#include <windows.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <stack>
