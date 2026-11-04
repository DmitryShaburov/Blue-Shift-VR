#pragma once


class VRSettings
{
public:
	void Init();

	void CheckCVARsForChanges();

private:
	void RegisterCVAR(const char* name, const char* value);
};

extern VRSettings g_vrSettings;
