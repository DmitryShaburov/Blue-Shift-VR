
// StudioModel helper functions for better collision detection in VR - Max Makes Mods - 2019-04-07

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "vector.h"

#include "VRModelHelper.h"
#include "animation.h"

VRModelInfo::VRModelInfo() : m_isValid{false},
							 m_numSequences{0}
{
}

VRModelInfo::VRModelInfo(CBaseEntity* pModel) : m_isValid{false},
												m_numSequences{0}
{
	if (0 != pModel->pev->model && STRING(pModel->pev->model)[0] != '\0')
	{
		void* pmodel = GET_MODEL_PTR(pModel->edict());
		if (pmodel)
		{
			m_numSequences = GetNumSequences(pmodel);
			if (m_numSequences > 0)
			{
				// Valid only if every sequence can be read
				for (int sequence = 0; sequence < m_numSequences; sequence++)
				{
					Vector mins;
					Vector maxs;
					float framerate = 0.f;
					int numFrames = 0;
					bool isLooping;
					float dummy = 0.f;
					if (!ExtractBbox(pmodel, sequence, mins, maxs) || 0 == GetSequenceInfo(pmodel, sequence, &dummy, &dummy, &framerate, &numFrames, &isLooping))
					{
						return;
					}
				}
				m_isValid = true;
			}
		}
	}
}


const VRModelInfo& VRModelHelper::GetModelInfo(CBaseEntity* pEntity)
{
	if (!pEntity)
		return VRModelInfo::INVALID;

	auto info = m_instance.m_modelInfoMap.find(pEntity->pev->model);
	if (info != m_instance.m_modelInfoMap.end())
	{
		if (!info->second.m_isValid)
		{
			info->second = VRModelInfo{pEntity};
		}
		return info->second;
	}
	else
	{
		return (m_instance.m_modelInfoMap[pEntity->pev->model] = VRModelInfo{pEntity});
	}
}

VRModelHelper VRModelHelper::m_instance{};
const VRModelInfo VRModelInfo::INVALID{};
