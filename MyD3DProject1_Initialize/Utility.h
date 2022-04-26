#pragma once

#include "PCH.h"

#include "Exceptions.h"

namespace Framework
{
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

	std::wstring MakeString(const wchar* format, ...);
	std::string MakeString(const char* format, ...);

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
}