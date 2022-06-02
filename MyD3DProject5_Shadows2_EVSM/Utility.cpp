#include "Utility.h"

namespace Framework
{

void WriteLog(const wchar* format, ...)
{
	wchar buffer[1024] = { 0 };
	va_list args;
	va_start(args, format);
	vswprintf_s(buffer, ArraySize_(buffer), format, args);;

	OutputDebugStringW(buffer);
	OutputDebugStringW(L"\n");
}

void WriteLog(const char* format, ...)
{
	char buffer[1024] = { 0 };
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, ArraySize_(buffer), format, args);

	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");
}

std::wstring MakeString(const wchar* format, ...)
{
	wchar buffer[1024] = { 0 };
	va_list args;
	va_start(args, format);
	vswprintf_s(buffer, ArraySize_(buffer), format, args);
	return std::wstring(buffer);
}

std::string MakeString(const char* format, ...)
{
	char buffer[1024] = { 0 };
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, ArraySize_(buffer), format, args);
	return std::string(buffer);
}

std::wstring FrameworkDir()
{
	return std::wstring(L"");
}

// Return the number of mip levels given a texture size
uint64 NumMipLevels(uint64 width, uint64 height, uint64 depth)
{
	uint64 numMips = 0;
	uint64 size = std::max(std::max(width, height), depth);
	while ((1ull << numMips) <= size)
		++numMips;

	if ((1ull << numMips) < size)
		++numMips;

	return numMips;
}

}
