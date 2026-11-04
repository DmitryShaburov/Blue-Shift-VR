#pragma once

#include <unordered_map>
#include "VRCommons.h"

class CBaseEntity;

class VRModelInfo
{
public:
	VRModelInfo();
	VRModelInfo(CBaseEntity* pEntity);

	bool m_isValid;

	int m_numSequences = 0;

	static const VRModelInfo INVALID;
};

class VRModelHelper
{
public:
	const VRModelInfo& GetModelInfo(CBaseEntity* pModel);

	static VRModelHelper& GetInstance() { return m_instance; }

private:
	static VRModelHelper m_instance;

	std::unordered_map<string_t, VRModelInfo> m_modelInfoMap;
};
