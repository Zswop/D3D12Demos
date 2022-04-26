#pragma once

#include <stdint.h>
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;
typedef wchar_t wchar;
typedef uint32_t bool32;

// Platform SDK defines, specifies that our min version is Windows 10
#ifndef WINVER
	#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0A00
#endif

#ifndef _WIN32_WINDOWS
	#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
	#define _WIN32_IE 0x0A00
#endif

#include <windows.h>
#include <wrl.h>

#include <comip.h>
#include <comdef.h>

#include <dxgi.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <D3Dcompiler.h>

// DirectX Math
#include <DirectXMath.h>

// D3DX12 utility library
#include "d3dx12.h"

// Un-define min and max from the windows headers
#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

// C RunTime Header Files
#include <stdlib.h>
#include <stdio.h>

// C++ Standard Library Header Files
#include <string>
#include <cmath>
#include <sstream>
#include <fstream>
#include <cstdio>

// Windows SDK imports
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")