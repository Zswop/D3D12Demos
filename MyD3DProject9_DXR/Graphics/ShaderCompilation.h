#pragma once

#include "..\\PCH.h"

#include "..\\Assert.h"
#include "..\\InterfacePointers.h"
#include "..\\MurmurHash.h"
#include "..\\Containers.h"

namespace Framework
{

class CompileOptions
{
public:

	// constants
	static const uint32 MaxDefines = 16;
	static const uint32 BufferSize = 1024;

	CompileOptions();

	void Add(const std::string& name, uint32 value);
	void Reset();
	
	void MakeDefines(D3D_SHADER_MACRO defines[MaxDefines + 1]) const;

private:

	uint32 nameOffset[MaxDefines];
	uint32 defineOffsets[MaxDefines];
	char buffer[BufferSize];
	uint32 numDefines;
	uint32 bufferIdx;
};

enum class ShaderType
{
	Vertex = 0,
	Hull,
	Domain,
	Geometry,
	Pixel,
	Compute,
	Library,

	NumTypes
};

enum class ShaderProfile
{
	SM50 = 0,
	SM51,

	NumProfiles
};

class CompiledShader
{
public:

	std::wstring FilePath;
	std::string FunctionName;
	CompileOptions CompileOpts;
	Array<uint8> ByteCode;
	ShaderType Type;
	Hash ByteCodeHash;

	CompiledShader(const wchar* filePath, const char* functionName,
		const CompileOptions& compileOptions, ShaderType type)
		: FilePath(filePath),
		CompileOpts(compileOptions),
		Type(type)
	{
		if (functionName != nullptr)
			FunctionName = functionName;
	}
};

class CompiledShaderPtr
{
public:

	CompiledShaderPtr() : ptr(nullptr)
	{
	}

	CompiledShaderPtr(const CompiledShader* ptr_) : ptr(ptr_)
	{
	}

	const CompiledShader* operator->() const
	{
		Assert_(ptr != nullptr);
		return ptr;
	}

	const CompiledShader& operator*() const
	{
		Assert_(ptr != nullptr);
		return *ptr;
	}

	bool Valid() const
	{
		return ptr != nullptr;
	}

	D3D12_SHADER_BYTECODE ByteCode() const
	{
		Assert_(ptr != nullptr);
		D3D12_SHADER_BYTECODE byteCode;
		byteCode.pShaderBytecode = ptr->ByteCode.Data();
		byteCode.BytecodeLength = ptr->ByteCode.Size();
		return byteCode;
	}

private:

	const CompiledShader* ptr;
};

typedef CompiledShaderPtr VertexShaderPtr;
typedef CompiledShaderPtr HullShaderPtr;
typedef CompiledShaderPtr DomainShaderPtr;
typedef CompiledShaderPtr GeometryShaderPtr;
typedef CompiledShaderPtr PixelShaderPtr;
typedef CompiledShaderPtr ComputeShaderPtr;
typedef CompiledShaderPtr ShaderPtr;

// Compiles a shader from file and creates the appropriate shader instance
CompiledShaderPtr CompileFromFile(const wchar* path,
	const char* functionName,
	ShaderType type,
	const CompileOptions& compileOpts = CompileOptions());

VertexShaderPtr CompileVSFromFile(const wchar* path,
	const char* functionName = "VS",
	const CompileOptions& compileOpts = CompileOptions());

PixelShaderPtr CompilePSFromFile(const wchar* path,
	const char* functionName = "PS",
	const CompileOptions& compileOpts = CompileOptions());

GeometryShaderPtr CompileGSFromFile(const wchar* path,
	const char* functionName = "GS",
	const CompileOptions& compileOpts = CompileOptions());

HullShaderPtr CompileHSFromFile(const wchar* path,
	const char* functionName = "HS",
	const CompileOptions& compileOpts = CompileOptions());

DomainShaderPtr CompileDSFromFile(const wchar* path,
	const char* functionName = "DS",
	const CompileOptions& compileOpts = CompileOptions());

ComputeShaderPtr CompileCSFromFile(const wchar* path,
	const char* functionName = "CS",
	const CompileOptions& compileOpts = CompileOptions());


bool UpdateShaders();
void ShutdownShaders();

}