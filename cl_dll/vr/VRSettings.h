#pragma once

#include <unordered_map>
#include <string>
#include <chrono>


class VRSettings
{
public:
	void Init();

	void CheckCVARsForChanges();

private:
	enum class RetryMode
	{
		NONE,
		WRITE,
		LOAD
	};

	void RegisterCVAR(const char* name, const char* value);

	std::unordered_map<std::string, std::string> m_cvarCache;

	void UpdateTextureMode();
	std::string m_vr_texturemode;
};

extern VRSettings g_vrSettings;
