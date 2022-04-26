#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "GraphicsTypes.h"

namespace Framework
{

// Texture loading
void LoadTexture(Texture& texture, const wchar* filePath, bool forceSRGB = false);

}