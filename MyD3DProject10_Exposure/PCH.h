#pragma once

#include "AppConfig.h"

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

// Disabled compiler warnings
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4324) // structure was padded due to alignment specifier

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

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRICT                          // Use strict declarations for Windows types

#define assert(expression) ((void)0)

// Windows Header Files:
#include <windows.h>
#include <wrl.h>

// MSVC COM Support
#include <comip.h>
#include <comdef.h>

// DirectX Includes
#ifdef _DEBUG
	#ifndef D3D_DEBUG_INFO
		#define D3D_DEBUG_INFO
	#endif
#endif


#include <dxgi.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <D3Dcompiler.h>

// DirectX Math
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#include <DirectXColors.h>

// DirectX Tex
#include "Externals\\DirectXTex July 2017\\Include\\DirectXTex.h"
#ifdef _DEBUG
	#pragma comment(lib, "Externals\\DirectXTex July 2017\\Lib 2017\\Debug\\DirectXTex.lib")
#else
	#pragma comment(lib, "Externals\\DirectXTex July 2017\\Lib 2017\\Release\\DirectXTex.lib")
#endif

// Pix for Windows
#define USE_PIX
#include "Externals\\WinPixEventRuntime\\Include\\WinPixEventRuntime\\pix3.h"
#pragma comment(lib, "Externals\\WinPixEventRuntime\\bin\\WinPixEventRuntime.lib")

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
#include <map>

// Assimp
#include "Externals\\Assimp-4.1.0\\include\\assimp\\Importer.hpp"
#include "Externals\\Assimp-4.1.0\\include\\assimp\\scene.h"
#include "Externals\\Assimp-4.1.0\\include\\assimp\\postprocess.h"
#pragma comment(lib, "Externals\\Assimp-4.1.0\\lib\\assimp-vc140-mt.lib")

// Windows SDK imports
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")

#include "Externals\\DXCompiler\\Include\\dxcapi.h"
#pragma comment(lib, "Externals\\DXCompiler\\Lib\\dxcompiler.lib")