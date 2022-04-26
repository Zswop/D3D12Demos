#include "Constants.hlsl"

struct SH9
{
	float c[9];
};

struct SH9Color
{
	float3 c[9];
};

// Cosine kernel for SH
static const float CosineA0 = Pi;
static const float CosineA1 = (2.0f * Pi) / 3.0f;
static const float CosineA2 = (0.25f * Pi);


// == SH9 =========================================================================================

//-------------------------------------------------------------------------------------------------
// Computes the dot project of two SH9 vectors
//-------------------------------------------------------------------------------------------------

float3 SHDotProduct(in SH9Color a, in SH9Color b)
{
	float3 result = 0.0f;

	[unroll]
	for (uint i = 0; i < 9; ++i)
		result += a.c[i] * b.c[i];

	return result;
}

//-------------------------------------------------------------------------------------------------
// Projects a direction onto SH and convolves with a given kernel
//-------------------------------------------------------------------------------------------------

SH9 ProjectOntoSH9(in float3 n, in float intensity, in float A0, in float A1, in float A2)
{
	SH9 sh;

	// Band 0
	sh.c[0] = 0.282095f * A0 * intensity;

	// Band 1
	sh.c[1] = 0.488603f * n.y * A1 * intensity;
	sh.c[2] = 0.488603f * n.z * A1 * intensity;
	sh.c[3] = 0.488603f * n.x * A1 * intensity;

	// Band 2
	sh.c[4] = 1.092548f * n.x * n.y * A2 * intensity;
	sh.c[5] = 1.092548f * n.y * n.z * A2 * intensity;
	sh.c[6] = 0.315392f * (3.0f * n.z * n.z - 1.0f) * A2 * intensity;
	sh.c[7] = 1.092548f * n.x * n.z * A2 * intensity;
	sh.c[8] = 0.546274f * (n.x * n.x - n.y * n.y) * A2 * intensity;

	return sh;
}

SH9Color ProjectOntoSH9Color(in float3 n, in float3 color, in float A0, in float A1, in float A2)
{
	SH9Color sh;

	// Band 0
	sh.c[0] = 0.282095f * A0 * color;

	// Band 1
	sh.c[1] = 0.488603f * n.y * A1 * color;
	sh.c[2] = 0.488603f * n.z * A1 * color;
	sh.c[3] = 0.488603f * n.x * A1 * color;

	// Band 2
	sh.c[4] = 1.092548f * n.x * n.y * A2 * color;
	sh.c[5] = 1.092548f * n.y * n.z * A2 * color;
	sh.c[6] = 0.315392f * (3.0f * n.z * n.z - 1.0f) * A2 * color;
	sh.c[7] = 1.092548f * n.x * n.z * A2 * color;
	sh.c[8] = 0.546274f * (n.x * n.x - n.y * n.y) * A2 * color;

	return sh;
}

//-------------------------------------------------------------------------------------------------
// Projects a direction onto SH9, convolves with a cosine kernel, and dots it with another
// SH9 vector
//-------------------------------------------------------------------------------------------------

float3 EvalSH9Irradiance(in float3 dir, in SH9Color sh)
{
	SH9Color dirSH = ProjectOntoSH9Color(dir, 1.0f, CosineA0, CosineA1, CosineA2);
	return SHDotProduct(dirSH, sh);
}