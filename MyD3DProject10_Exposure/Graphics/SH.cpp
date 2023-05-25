#include "SH.h"
#include "Textures.h"

namespace Framework
{

SH9 ProjectOntoSH9(const Float3& dir)
{
	SH9 sh;

	// Band 0
	sh.Coefficients[0] = 0.282095f;

	// Band 1
	sh.Coefficients[1] = 0.488603f * dir.y;
	sh.Coefficients[2] = 0.488603f * dir.z;
	sh.Coefficients[3] = 0.488603f * dir.x;

	// Band 2
	sh.Coefficients[4] = 1.092548f * dir.x * dir.y;
	sh.Coefficients[5] = 1.092548f * dir.y * dir.z;
	sh.Coefficients[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
	sh.Coefficients[7] = 1.092548f * dir.x * dir.z;
	sh.Coefficients[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);

	return sh;
}

SH9Color ProjectOntoSH9Color(const Float3& dir, const Float3& color)
{
	SH9 sh = ProjectOntoSH9(dir);
	SH9Color shColor;
	for (uint64 i = 0; i < 9; ++i)
		shColor.Coefficients[i] = color * sh.Coefficients[i];
	return shColor;
}


Float3 EvalSH9Radiance(const Float3& dir, const SH9Color& sh)
{
	SH9 dirSH = ProjectOntoSH9(dir);

	Float3 radiance;
	for (uint64 i = 0; i < 9; ++i)
		radiance += sh.Coefficients[i] * dirSH.Coefficients[i];
	return radiance;
}

Float3 EvalSH9Irradiance(const Float3& dir, const SH9Color& sh)
{
	SH9 dirSH = ProjectOntoSH9(dir);
	dirSH.Coefficients[0] *= CosineA0;
	dirSH.Coefficients[1] *= CosineA1;
	dirSH.Coefficients[2] *= CosineA1;
	dirSH.Coefficients[3] *= CosineA1;
	dirSH.Coefficients[4] *= CosineA2;
	dirSH.Coefficients[5] *= CosineA2;
	dirSH.Coefficients[6] *= CosineA2;
	dirSH.Coefficients[7] *= CosineA2;
	dirSH.Coefficients[8] *= CosineA2;

	Float3 result;
	for (uint64 i = 0; i < 9; ++i)
		result += sh.Coefficients[i] * dirSH.Coefficients[i];
	return result;
}

H4 ProjectOntoH4(const Float3& dir)
{
	H4 result;

	result[0] = (1.0f / std::sqrt(2.0f * 3.14159f));

	// Band 1
	result[1] = std::sqrt(1.5f / 3.14159f) * dir.y;
	result[2] = std::sqrt(1.5f / 3.14159f) * (2 * dir.z - 1.0f);
	result[3] = std::sqrt(1.5f / 3.14159f) * dir.x;

	return result;
}

float EvalH4(const H4& h, const Float3& dir)
{
	H4 b = ProjectOntoH4(dir);
	return H4::Dot(h, b);
}

H4 ConvertToH4(const SH9& sh)
{
	const float rt2 = sqrt(2.0f);
	const float rt32 = sqrt(3.0f / 2.0f);
	const float rt52 = sqrt(5.0f / 2.0f);
	const float rt152 = sqrt(15.0f / 2.0f);
	const float convMatrix[4][9] =
	{
		{ 1.0f / rt2, 0, 0.5f * rt32, 0, 0, 0, 0, 0, 0 },
		{ 0, 1.0f / rt2, 0, 0, 0, (3.0f / 8.0f) * rt52, 0, 0, 0 },
		{ 0, 0, 1.0f / (2.0f * rt2), 0, 0, 0, 0.25f * rt152, 0, 0 },
		{ 0, 0, 0, 1.0f / rt2, 0, 0, 0, (3.0f / 8.0f) * rt52, 0 }
	};

	H4 hBasis;

	for (uint64 row = 0; row < 4; ++row)
	{
		hBasis.Coefficients[row] = 0.0f;

		for (uint64 col = 0; col < 9; ++col)
			hBasis.Coefficients[row] += convMatrix[row][col] * sh.Coefficients[col];
	}

	return hBasis;
}

SH9Color ProjectCubemapToSH(const Texture& texture)
{
	Assert_(texture.Cubemap);

	TextureData<Float4> textureData;
	GetTextureData(texture, textureData);
	Assert_(textureData.NumSlices == 6);

	const uint32 width = textureData.Width;
	const uint32 height = textureData.Height;

	SH9Color result;
	float weightSum = 0.0f;
	for (uint32 face = 0; face < 6; ++face)
	{
		for (uint32 y = 0; y < height; ++y)
		{
			for (uint32 x = 0; x < width; ++x)
			{
				const uint32 idx = face * (width * height) + y * (width) + x;
				Float3 sample = textureData.Texels[idx].To3D();

				float u = (x + 0.5f) / width;
				float v = (y + 0.5f) / height;

				// Account for cubemap texel distribution
				u = u * 2.0f - 1.0f;
				v = v * 2.0f - 1.0f;
				const float temp = 1.0f + u * u + v * v;
				const float weight = 4.0f / (sqrt(temp) * temp);

				Float3 dir = MapXYSToDirection(x, y, face, width, height);
				result += ProjectOntoSH9Color(dir, sample) * weight;
				weightSum += weight;
			}
		}
	}

	result *= (4.0f * 3.14159f) / weightSum;
	return result;
}

void CreateCubemapFromSH9Color(Texture& cubemap, const SH9Color sh)
{
	// For this we again pre-scale by our FP16 scale factor so that we can use an FP16 format.
	const uint64 CubeMapRes = 128;
	Array<Half4> texels;

	texels.Init(CubeMapRes * CubeMapRes * 6);

	float weightSum = 0.0f;

	for (uint64 s = 0; s < 6; ++s)
	{
		for (uint64 y = 0; y < CubeMapRes; ++y)
		{
			for (uint64 x = 0; x < CubeMapRes; ++x)
			{
				Float3 dir = MapXYSToDirection(x, y, s, CubeMapRes, CubeMapRes);
				Float3 radiance = EvalSH9Radiance(dir, sh);
				//Float3 radiance = EvalSH9Irradiance(dir, sh);

				uint64 idx = (s * CubeMapRes * CubeMapRes) + (y * CubeMapRes) + x;
				texels[idx] = Half4(Float4(radiance, 1.0f));
			}
		}
	}

	Create2DTexture(cubemap, CubeMapRes, CubeMapRes, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, true, texels.Data());
}

void CreateTexture2DFromSH9Color(const SH9Color sh, const wchar* filePath)
{
	TextureData<Half4> texData;
	texData.Init(256, 128, 1);

	for (uint32 y = 0; y < texData.Height; ++y)
	{
		for (uint32 x = 0; x < texData.Width; ++x)
		{
			float u = (x + 0.5f) / float(texData.Width);
			float v = (y + 0.5f) / float(texData.Height);

			Float3 dir = LatLongTexcoordToCartesian(Float2(u, v));

			//Float3 radiance = EvalSH9Radiance(dir, sh);
			Float3 irradiance = EvalSH9Irradiance(dir, sh);

			uint32 idx = (y * texData.Width) + x;
			texData.Texels[idx] = Half4(Float4(irradiance, 1.0f));
		}
	}
	
	SaveTextureAsDDS(texData, filePath);
}

}
