
#include <filesystem>

#include "hud.h"
#include "cl_util.h"
#include "VRInput.h"
#include "eiface.h"

#include "VRTextureHelper.h"
#include "VRCommon.h"

#include "vr_gl.h"
#include "LodePNG/lodepng.h"

unsigned int VRTextureHelper::GetHDGameTexture(const std::string& name)
{
	return GetTexture("game/" + name + ".png");
}

unsigned int VRTextureHelper::GetSkyboxTexture(const std::string& name, int index)
{
	unsigned int texture = GetTexture("skybox/" + name + m_mapSkyboxIndices[index] + ".png");
	if (!texture && name != "desert")
	{
		gEngfuncs.Con_DPrintf("Skybox texture %s not found, falling back to desert.\n", (name + m_mapSkyboxIndices[index]).data());
		texture = GetTexture("skybox/desert" + m_mapSkyboxIndices[index] + ".png");
	}
	return texture;
}

const char* VRTextureHelper::GetSkyboxNameFromMapName(const std::string& mapName)
{
	auto& skyboxName = m_mapSkyboxNames.find(mapName);
	if (skyboxName != m_mapSkyboxNames.end())
		return skyboxName->second.data();
	else
		return "desert";
}

unsigned int VRTextureHelper::GetTexture(const std::string& name)
{
	if (m_textures.count(name) > 0)
		return m_textures[name];

	GLuint texture{ 0 };

	std::filesystem::path texturePath = GetPathFor("/textures/" + name);
	if (std::filesystem::exists(texturePath))
	{
		std::vector<unsigned char> image;
		unsigned int width, height;
		unsigned int error = lodepng::decode(image, width, height, texturePath.string().data());
		if (error)
		{
			gEngfuncs.Con_DPrintf("Error (%i) trying to load texture %s: %s\n", error, name.data(), lodepng_error_text(error));
		}
		else if ((width & (width - 1)) || (height & (height - 1)))
		{
			gEngfuncs.Con_DPrintf("Invalid texture %s, width and height must be power of 2!\n", name.data());
		}
		else
		{
			// Images are upside down, since we use them to replace textures used by Half-Life,
			// we cannot change the line order while rendering (engine code), so we flip the image here:
			std::vector<unsigned char> flippedImage;
			flippedImage.resize(image.size());
			for (unsigned int imageLine = 0; imageLine < height; imageLine++)
			{
				for (unsigned int x = 0; x < width; x++)
				{
					for (unsigned int c = 0; c < 4; c++)
					{
						flippedImage[4 * (height - imageLine - 1) * width + 4 * x + c] = image[4 * imageLine * width + 4 * x + c];
					}
				}
			}

			// Now load it into OpenGL
			glActiveTexture(GL_TEXTURE0);
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, 4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, flippedImage.data());
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	else
	{
		gEngfuncs.Con_DPrintf("Couldn't load texture %s, it doesn't exist.\n", name.data());
	}

	m_textures[name] = texture;
	return texture;
}

VRTextureHelper VRTextureHelper::instance{};

// from openvr docs: Order is Front, Back, Left, Right, Top, Bottom.
const std::array<std::string, 6> VRTextureHelper::m_mapSkyboxIndices{
	{
		{"ft"},
		{"bk"},
		{"lf"},
		{"rt"},
		{"up"},
		{"dn"}
	}
};

const std::unordered_map<std::string, std::string> VRTextureHelper::m_mapSkyboxNames{
	{
		{"maps/c0a0.bsp", "desert"},
		{"maps/c0a0a.bsp", "desert"},
		{"maps/c0a0b.bsp", "2desert"},
		{"maps/c0a0c.bsp", "desert"},
		{"maps/c0a0d.bsp", "desert"},
		{"maps/c0a0e.bsp", "desert"},
		{"maps/c1a0.bsp", "desert"},
		{"maps/c1a0a.bsp", "desert"},
		{"maps/c1a0b.bsp", "desert"},
		{"maps/c1a0c.bsp", "desert"},
		{"maps/c1a0d.bsp", "desert"},
		{"maps/c1a0e.bsp", "xen9"},
		{"maps/c1a1.bsp", "desert"},
		{"maps/c1a1a.bsp", "desert"},
		{"maps/c1a1b.bsp", "desert"},
		{"maps/c1a1c.bsp", "desert"},
		{"maps/c1a1d.bsp", "desert"},
		{"maps/c1a1f.bsp", "desert"},
		{"maps/c1a2.bsp", "desert"},
		{"maps/c1a2a.bsp", "desert"},
		{"maps/c1a2b.bsp", "desert"},
		{"maps/c1a2c.bsp", "desert"},
		{"maps/c1a2d.bsp", "desert"},
		{"maps/c1a3.bsp", "desert"},
		{"maps/c1a3a.bsp", "desert"},
		{"maps/c1a3b.bsp", "dusk"},
		{"maps/c1a3c.bsp", "dusk"},
		{"maps/c1a3d.bsp", "desert"},
		{"maps/c1a4.bsp", "desert"},
		{"maps/c1a4b.bsp", "desert"},
		{"maps/c1a4d.bsp", "desert"},
		{"maps/c1a4e.bsp", "desert"},
		{"maps/c1a4f.bsp", "desert"},
		{"maps/c1a4g.bsp", "desert"},
		{"maps/c1a4i.bsp", "desert"},
		{"maps/c1a4j.bsp", "desert"},
		{"maps/c1a4k.bsp", "desert"},
		{"maps/c2a1.bsp", "desert"},
		{"maps/c2a1a.bsp", "desert"},
		{"maps/c2a1b.bsp", "desert"},
		{"maps/c2a2.bsp", "night"},
		{"maps/c2a2a.bsp", "day"},
		{"maps/c2a2b1.bsp", "desert"},
		{"maps/c2a2b2.bsp", "desert"},
		{"maps/c2a2c.bsp", "desert"},
		{"maps/c2a2d.bsp", "desert"},
		{"maps/c2a2e.bsp", "night"},
		{"maps/c2a2f.bsp", "desert"},
		{"maps/c2a2g.bsp", "night"},
		{"maps/c2a2h.bsp", "night"},
		{"maps/c2a3.bsp", "desert"},
		{"maps/c2a3a.bsp", "desert"},
		{"maps/c2a3b.bsp", "desert"},
		{"maps/c2a3c.bsp", "dawn"},
		{"maps/c2a3d.bsp", "2desert"},
		{"maps/c2a3e.bsp", "desert"},
		{"maps/c2a4.bsp", "morning"},
		{"maps/c2a4a.bsp", "desert"},
		{"maps/c2a4b.bsp", "desert"},
		{"maps/c2a4c.bsp", "desert"},
		{"maps/c2a4d.bsp", "desert"},
		{"maps/c2a4e.bsp", "desert"},
		{"maps/c2a4f.bsp", "desert"},
		{"maps/c2a4g.bsp", "desert"},
		{"maps/c2a5.bsp", "desert"},
		{"maps/c2a5a.bsp", "cliff"},
		{"maps/c2a5b.bsp", "desert"},
		{"maps/c2a5c.bsp", "desert"},
		{"maps/c2a5d.bsp", "desert"},
		{"maps/c2a5e.bsp", "desert"},
		{"maps/c2a5f.bsp", "desert"},
		{"maps/c2a5g.bsp", "desert"},
		{"maps/c2a5w.bsp", "desert"},
		{"maps/c2a5x.bsp", "desert"},
		{"maps/c3a1.bsp", "desert"},
		{"maps/c3a1a.bsp", "desert"},
		{"maps/c3a1b.bsp", "desert"},
		{"maps/c3a2.bsp", "desert"},
		{"maps/c3a2a.bsp", "desert"},
		{"maps/c3a2b.bsp", "desert"},
		{"maps/c3a2c.bsp", "desert"},
		{"maps/c3a2d.bsp", "desert"},
		{"maps/c3a2e.bsp", "desert"},
		{"maps/c3a2f.bsp", "desert"},
		{"maps/c4a1.bsp", "neb7"},
		{"maps/c4a1a.bsp", "neb7"},
		{"maps/c4a1b.bsp", "alien1"},
		{"maps/c4a1c.bsp", "xen8"},
		{"maps/c4a1d.bsp", "xen8"},
		{"maps/c4a1e.bsp", "xen8"},
		{"maps/c4a1f.bsp", "black"},
		{"maps/c4a2.bsp", "neb6"},
		{"maps/c4a2a.bsp", "neb6"},
		{"maps/c4a2b.bsp", "neb6"},
		{"maps/c4a3.bsp", "black"},
		{"maps/c5a1.bsp", "xen10"}
	}
};
