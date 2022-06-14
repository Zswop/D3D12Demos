#pragma once

#include "PCH.h"

#include "Exceptions.h"
#include "F12_Math.h"

namespace Framework
{

// Converts an ANSI string to a std::wstring
inline std::wstring AnsiToWString(const char* ansiString)
{
	wchar buffer[512];
	Win32Call(MultiByteToWideChar(CP_ACP, 0, ansiString, -1, buffer, 512));
	return std::wstring(buffer);
}

inline std::string WStringToAnsi(const wchar* wideString)
{
	char buffer[512];
	Win32Call(WideCharToMultiByte(CP_ACP, 0, wideString, -1, buffer, 512, NULL, NULL));
	return std::string(buffer);
}

// Converts a number to a string
template<typename T> inline std::wstring ToString(const T& val)
{
	std::wostringstream stream;
	if (!(stream << val))
		throw Exception(L"Error converting value to string");
	return stream.str();
}

// Converts a number to an ansi string
template<typename T> inline std::string ToAnsiString(const T& val)
{
	std::ostringstream stream;
	if (!(stream << val))
		throw Exception(L"Error converting value to string");
	return stream.str();
}

void WriteLog(const wchar* format, ...);
void WriteLog(const char* format, ...);

std::wstring MakeString(const wchar* format, ...);
std::string MakeString(const char* format, ...);

std::wstring FrameworkDir();

// Outputs a string to the debugger output and stdout
inline void DebugPrint(const std::wstring& str)
{
	std::wstring output = str + L"\n";
	OutputDebugStringW(output.c_str());
	std::printf("%ls", output.c_str());
}

template<typename T, uint64 N>
uint64 ArraySize(T(&)[N])
{
	return N;
}

#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

inline uint64 AlignTo(uint64 num, uint64 alignment)
{
	Assert_(alignment > 0);
	return ((num + alignment - 1) / alignment) * alignment;
}

}