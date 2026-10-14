
#include <filesystem>

#include "hud.h"
#include "cl_util.h"
#include "VRInput.h"
#include "eiface.h"
#include "pm_defs.h"
#include "pm_movevars.h"

#include "VRTextureHelper.h"
#include "VRCommon.h"

#include "vr_gl.h"
#include "lodepng.h"

void VRTextureHelper::PreloadAllTextures(const std::filesystem::path& path)
{
	if (std::filesystem::exists(path))
	{
		for (auto& p : std::filesystem::directory_iterator(path))
		{
			if (p.is_directory())
			{
				PreloadAllTextures(p.path());
			}
			else
			{
				unsigned int dummy = 0;
				GetTextureInternal(p.path(), dummy, dummy);
			}
		}
	}
}

void VRTextureHelper::Init()
{
	if (m_isInitialized)
		return;

	UpdateTextureMode();

	m_isInitialized = true;
}

unsigned int VRTextureHelper::GetVRSkyboxTexture(const std::string& name, int index)
{
	unsigned int texture = GetTexture("skybox/" + name + m_vrMapSkyboxIndices[index] + ".png");
	if (!texture && name != "desert")
	{
		gEngfuncs.Con_DPrintf("Skybox texture %s not found, falling back to desert.\n", (name + m_vrMapSkyboxIndices[index]).data());
		texture = GetTexture("skybox/desert" + m_vrMapSkyboxIndices[index] + ".png");
	}
	return texture;
}

const char* VRTextureHelper::GetSkyboxNameFromMapName(const std::string& mapName)
{
	auto skyboxName = m_mapSkyboxNames.find(mapName);
	if (skyboxName != m_mapSkyboxNames.end())
		return skyboxName->second.data();
	else
		return "desert";
}

const char* VRTextureHelper::GetCurrentSkyboxName()
{
	extern playermove_t* pmove;
	if (pmove != nullptr && pmove->movevars->skyName[0] != 0)
		return pmove->movevars->skyName;
	else
		return "desert";
}

void VRTextureHelper::UpdateTextureMode()
{
	std::string textureMode = CVAR_GET_STRING("gl_texturemode");
	std::transform(textureMode.begin(), textureMode.end(), textureMode.begin(), ::toupper);
	if (m_lastTextureMode.empty() || textureMode != m_lastTextureMode)
	{
		if (textureMode == "GL_NEAREST")
		{
			m_glTexMinFilter = GL_NEAREST;
			m_glTexMagFilter = GL_NEAREST;
		}
		else if (textureMode == "GL_LINEAR")
		{
			m_glTexMinFilter = GL_LINEAR;
			m_glTexMagFilter = GL_LINEAR;
		}
		else if (textureMode == "GL_NEAREST_MIPMAP_NEAREST")
		{
			m_glTexMinFilter = GL_NEAREST_MIPMAP_NEAREST;
			m_glTexMagFilter = GL_NEAREST;
		}
		else if (textureMode == "GL_LINEAR_MIPMAP_NEAREST")
		{
			m_glTexMinFilter = GL_LINEAR_MIPMAP_NEAREST;
			m_glTexMagFilter = GL_LINEAR;
		}
		else if (textureMode == "GL_NEAREST_MIPMAP_LINEAR")
		{
			m_glTexMinFilter = GL_NEAREST_MIPMAP_LINEAR;
			m_glTexMagFilter = GL_NEAREST;
		}
		else /*if (textureMode == "GL_LINEAR_MIPMAP_LINEAR")*/
		{
			m_glTexMinFilter = GL_LINEAR_MIPMAP_LINEAR;
			m_glTexMagFilter = GL_LINEAR;
		}

		m_lastTextureMode = textureMode;

		PreloadAllTextures(GetPathFor("/textures/hud"));
	}
}

unsigned int VRTextureHelper::GetTextureInternal(const std::filesystem::path& path, unsigned int& outwidth, unsigned int& outheight)
{
	UpdateTextureMode();

	std::string canonicalpathstring;
	try
	{
		canonicalpathstring = std::filesystem::canonical(path).string();
	}
	catch (std::filesystem::filesystem_error e)
	{
		canonicalpathstring = std::filesystem::absolute(path).string();
	}

	if (!canonicalpathstring.empty())
	{
		auto it = m_textures.find(canonicalpathstring);
		if (it != m_textures.end())
		{
			outwidth = it->second.width;
			outheight = it->second.height;
			return it->second.texnum;
		}
	}

	GLuint texture{ 0 };

	unsigned int lodewidth = 0, lodeheight = 0;
	if (std::filesystem::exists(path))
	{
		std::vector<unsigned char> image;
		unsigned int error = lodepng::decode(image, lodewidth, lodeheight, path.string().data());
		if (error)
		{
			gEngfuncs.Con_DPrintf("Error (%i) trying to load texture %s: %s\n", error, path.string().data(), lodepng_error_text(error));
		}
		else if ((lodewidth & (lodewidth - 1)) || (lodeheight & (lodeheight - 1)))
		{
			gEngfuncs.Con_DPrintf("Invalid texture %s, width and height must be power of 2!\n", path.string().data());
			lodewidth = 0;
			lodeheight = 0;
			texture = 0;
		}
		else
		{
			// Now load it into OpenGL
			ClearGLErrors();
			try
			{
				TryGLCall(glActiveTexture, GL_TEXTURE0);
				TryGLCall(glGenTextures, 1, &texture);
				TryGLCall(glBindTexture, GL_TEXTURE_2D, texture);
				TryGLCall(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA8, lodewidth, lodeheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
				TryGLCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_glTexMinFilter);
				TryGLCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_glTexMagFilter);
				TryGLCall(glGenerateMipmap, GL_TEXTURE_2D);
				TryGLCall(glBindTexture, GL_TEXTURE_2D, 0);
			}
			catch (OGLErrorException e)
			{
				gEngfuncs.Con_DPrintf("Couldn't create texture %s, error: %s\n", path.string().data(), e.what());
				lodewidth = 0;
				lodeheight = 0;
				texture = 0;
			}
		}
	}
	else
	{
		gEngfuncs.Con_DPrintf("Couldn't load texture %s, it doesn't exist.\n", path.string().data());
		lodewidth = 0;
		lodeheight = 0;
		texture = 0;
	}

	m_textures[canonicalpathstring] = Texture{ texture, lodewidth, lodeheight };
	outwidth = lodewidth;
	outheight = lodeheight;
	return texture;
}

unsigned int VRTextureHelper::GetTexture(const std::string& name, unsigned int& width, unsigned int& height)
{
	return GetTextureInternal(GetPathFor("/textures/" + name), width, height);
}

unsigned int VRTextureHelper::GetTexture(const std::string& name)
{
	unsigned int dummy = 0;
	return GetTexture(name, dummy, dummy);
}

VRTextureHelper VRTextureHelper::instance{};

// From openvr docs: "Order is Front, Back, Left, Right, Top, Bottom."
// But: SteamVR's left and right are switched, and top and bottom are rotated 90�,
// so we need to adjust the order to get everything correctly in place.
// The order as defined here works.
// (Actually now that I look at the values I realize the sides are just in reverse.)
const std::array<std::string, 6> VRTextureHelper::m_vrMapSkyboxIndices{
	{{"rt"},
	 {"lf"},
	 {"bk"},
	 {"ft"},
	 {"up"},
	 {"dn"}}
};

const std::unordered_map<std::string, std::string> VRTextureHelper::m_mapSkyboxNames{
	{{"maps/ba_canal1.bsp", "desert"},
	 {"maps/ba_canal1b.bsp", "desert"},
	 {"maps/ba_canal2.bsp", "desert"},
	 {"maps/ba_canal3.bsp", "desert"},
	 {"maps/ba_elevator.bsp", "desert"},
	 {"maps/ba_hazard1.bsp", "desert"},
	 {"maps/ba_hazard2.bsp", "desert"},
	 {"maps/ba_hazard3.bsp", "desert"},
	 {"maps/ba_hazard4.bsp", "desert"},
	 {"maps/ba_hazard5.bsp", "desert"},
	 {"maps/ba_hazard6.bsp", "desert"},
	 {"maps/ba_maint.bsp", "desert"},
	 {"maps/ba_outro.bsp", "2desert"},
	 {"maps/ba_power1.bsp", "desert"},
	 {"maps/ba_power2.bsp", "desert"},
	 {"maps/ba_security1.bsp", "desert"},
	 {"maps/ba_security2.bsp", "desert"},
	 {"maps/ba_teleport1.bsp", "desert"},
	 {"maps/ba_teleport2.bsp", "desert"},
	 {"maps/ba_tram1.bsp", "desert"},
	 {"maps/ba_tram2.bsp", "desert"},
	 {"maps/ba_tram3.bsp", "desert"},
	 {"maps/ba_xen1.bsp", "alien1"},
	 {"maps/ba_xen2.bsp", "alien1"},
	 {"maps/ba_xen3.bsp", "alien1"},
	 {"maps/ba_xen4.bsp", "alien1"},
	 {"maps/ba_xen5.bsp", "alien1"},
	 {"maps/ba_xen6.bsp", "alien1"},
	 {"maps/ba_yard1.bsp", "desert"},
	 {"maps/ba_yard2.bsp", "desert"},
	 {"maps/ba_yard3.bsp", "desert"},
	 {"maps/ba_yard3a.bsp", "desert"},
	 {"maps/ba_yard3b.bsp", "desert"},
	 {"maps/ba_yard4.bsp", "desert"},
	 {"maps/ba_yard4a.bsp", "desert"},
	 {"maps/ba_yard5.bsp", "desert"},
	 {"maps/ba_yard5a.bsp", "desert"}}
};
