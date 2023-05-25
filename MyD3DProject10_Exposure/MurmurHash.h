#pragma once

#include "PCH.h"

namespace Framework
{

struct Hash
{
	uint64 A;
	uint64 B;

	Hash() : A(0), B(0) {}
	Hash(uint64 a, uint64 b) : A(a), B(b) {}

	std::wstring ToString() const;

	bool operator==(const Hash& other)
	{
		return A == other.A && B == other.B;
	}
};

Hash GenerateHash(const void* key, const uint32 len, const uint32 seed = 0);
Hash CombineHashes(Hash a, Hash b);

}