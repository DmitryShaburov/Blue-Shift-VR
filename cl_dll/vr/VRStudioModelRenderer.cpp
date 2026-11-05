// VR parts of CStudioModelRenderer (bone setup, VR hands, dragged entities), moved from StudioModelRenderer.cpp

#include "hud.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"

#include <string.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"

#include <string>
#include <unordered_set>
#include "vr/DoOnDestruct.h"
#include "vr/VRShared.h"
#include "vr/VRRenderer.h"
#include <math.h>

// Defined in StudioModelRenderer.cpp
extern engine_studio_api_t IEngineStudio;
extern std::unordered_set<cl_entity_t*> g_curFrameTempEnts;
studiohdr_t* Mod_Extradata(const char* callerInfo, cl_entity_t* ent, model_t* mod);

namespace
{
enum Finger
{
	Finger_Thumb = 0,
	Finger_Index,
	Finger_Middle,
	Finger_Ring,
	Finger_Pinky,
	Finger_Count
};

constexpr const int VRHAND_Bones_Count = 31;

enum HandModelSequences
{
	IDLE = 0,
	POINT_START,
	POINT_END,
	WAIT_START,
	WAIT_END,
	HALFGRAB_START,
	HALFGRAB_END,
	FULLGRAB_START,
	FULLGRAB_END
};

const char* FingerBoneName(Finger finger)
{
	switch (finger)
	{
	case Finger_Thumb: return "Finger0";
	case Finger_Index: return "Finger1";
	case Finger_Middle: return "Finger2";
	case Finger_Ring: return "Finger3";
	case Finger_Pinky: return "Finger4";
	default: return nullptr;
	}
}

bool IsFingerBoneName(Finger finger, const std::string& name)
{
	const char* fingerbonename = FingerBoneName(finger);
	return fingerbonename != nullptr && name.find(fingerbonename) != std::string::npos;
}
} // namespace

/*
====================
StudioSetupBonesInline

====================
*/
void CStudioModelRenderer::StudioSetupBonesInline(float bonetransform[MAXSTUDIOBONES][3][4], float lighttransform[MAXSTUDIOBONES][3][4], float* overrideFrame)
{
	int i = 0;
	float f = 0.f;

	mstudiobone_t* pbones;
	mstudioseqdesc_t* pseqdesc;
	mstudioanim_t* panim;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	float bonematrix[3][4];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = overrideFrame ? *overrideFrame : StudioEstimateFrame(pseqdesc);

	panim = StudioGetAnim(m_pRenderModel, pseqdesc);
	StudioCalcRotations(pos, q, pseqdesc, panim, f);

	if (pseqdesc->numblends > 1)
	{
		float s = 0.f;
		float dadt = 0.f;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations(pos2, q2, pseqdesc, panim, f);

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;

		StudioSlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos3, q3, pseqdesc, panim, f);

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos4, q4, pseqdesc, panim, f);

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones(q3, pos3, q4, pos4, s);

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0f - dadt)) / 255.0f;
			StudioSlerpBones(q, pos, q3, pos3, s);
		}
	}

	if (m_fDoInterp &&
		0 != m_pCurrentEntity->latched.sequencetime &&
		(m_pCurrentEntity->latched.sequencetime + 0.2f > m_clTime) &&
		(m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq))
	{
		// blend from last sequence
		static float pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s = 0.f;

		pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		// clip prevframe
		StudioCalcRotations(pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0f;
			StudioSlerpBones(q1b, pos1b, q2, pos2, s);

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations(pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe);

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0f;
				StudioSlerpBones(q3, pos3, q4, pos4, s);

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0f;
				StudioSlerpBones(q1b, pos1b, q3, pos3, s);
			}
		}

		s = 1.0f - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2f;
		StudioSlerpBones(q, pos, q1b, pos1b, s);
	}
	else
	{
		//Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->boneindex);

	// calc gait animation
	if (m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0)
	{
		if (m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq)
		{
			m_pPlayerInfo->gaitsequence = 0;
		}

		pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->seqindex) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim(m_pRenderModel, pseqdesc);
		StudioCalcRotations(pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe);

		for (i = 0; i < m_pStudioHeader->numbones; i++)
		{
			if (strcmp(pbones[i].name, "Bip01 Spine") == 0)
				break;
			memcpy(pos[i], pos2[i], sizeof(pos[i]));
			memcpy(q[i], q2[i], sizeof(q[i]));
		}
	}

	// backup rotation matrix
	float rotationmatrix_backup[3][4];
	MatrixCopy(*m_protationmatrix, rotationmatrix_backup);

	// Added studio model scaling - Max Makes Mods, 2017-08-26
	if (g_curFrameTempEnts.count(m_pCurrentEntity) == 0 && m_pCurrentEntity->curstate.scale > 0.f && m_pCurrentEntity->prevstate.scale == m_pCurrentEntity->curstate.scale)
	{
		for (int j = 0; j < 3; j++)
		{
			for (int k = 0; k < 3; k++)
			{
				(*m_protationmatrix)[j][k] *= m_pCurrentEntity->curstate.scale;
			}
		}
	}

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			if (0 != IEngineStudio.IsHardware())
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, bonetransform[i]);
				MatrixCopy(bonetransform[i], lighttransform[i]);
			}
			else
			{
				ConcatTransforms((*m_paliastransform), bonematrix, bonetransform[i]);
				ConcatTransforms((*m_protationmatrix), bonematrix, lighttransform[i]);
			}

			// Apply client-side effects to the transformation matrix
			StudioFxTransform(m_pCurrentEntity, bonetransform[i]);
		}
		else
		{
			ConcatTransforms(bonetransform[pbones[i].parent], bonematrix, bonetransform[i]);
			ConcatTransforms(lighttransform[pbones[i].parent], bonematrix, lighttransform[i]);
		}
	}

	// restore rotation matrix
	MatrixCopy(rotationmatrix_backup, *m_protationmatrix);
}

// The engine only fires client side studio events (e.g. reload sounds) for entities it draws itself, not for
// the controller models drawn by DrawVREntity, so play the sound events (5004) crossed since the last frame here.
void CStudioModelRenderer::StudioDispatchVRSoundEvents(const char* modelname, const Vector& origin, bool mirrored)
{
	struct EventState
	{
		std::string modelname;
		int sequence = -1;
		float frame = -1.f;
	};
	static EventState states[2];
	EventState& state = states[mirrored ? 1 : 0];

	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
		return;

	mstudioseqdesc_t* pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
	float f = StudioEstimateFrame(pseqdesc);

	// New model or sequence, or the sequence restarted/looped: events from the start are due again
	float lastFrame = state.frame;
	if (state.sequence != m_pCurrentEntity->curstate.sequence || state.modelname != modelname || f < lastFrame)
		lastFrame = -1.f;

	state.modelname = modelname;
	state.sequence = m_pCurrentEntity->curstate.sequence;
	state.frame = f;

	mstudioevent_t* pevent = reinterpret_cast<mstudioevent_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + pseqdesc->eventindex);
	for (int i = 0; i < pseqdesc->numevents; i++)
	{
		if (pevent[i].event == 5004 && pevent[i].frame > lastFrame && pevent[i].frame <= f)
		{
			gEngfuncs.pfnPlaySoundByNameAtLocation(pevent[i].options, 1.0f, const_cast<float*>(&origin.x));
		}
	}
}

bool CStudioModelRenderer::DrawVREntity(
	const char* modelname,
	const Vector& origin, const Vector& angles,
	int body, int skin, float scale,
	float frame, float framerate,
	float animtime, int sequence,
	int effects,
	int rendermode, int renderamt, int renderfx, color24 rendercolor,
	bool isController, bool mirrored)
{
	if (strlen(modelname) == 0)
		return false;

	if (modelname[0] == '*')
		return false;

	cl_entity_t* viewent = IEngineStudio.GetViewEntity();
	if (!viewent)
		return false;

	auto model = IEngineStudio.Mod_ForName(modelname, 0);
	if (!model || model->type != mod_studio)
		return false;

	cl_entity_t backupviewent = *viewent;
	DoOnDestruct doOnDestruct{[&]()
		{
			*viewent = backupviewent;
		}};

	m_pCurrentEntity = viewent;

	IEngineStudio.SetRenderModel(m_pCurrentEntity->model = m_pRenderModel = model);
	m_pCurrentEntity->baseline.body = m_pCurrentEntity->prevstate.body = m_pCurrentEntity->curstate.body = body;
	m_pCurrentEntity->baseline.skin = m_pCurrentEntity->prevstate.skin = m_pCurrentEntity->curstate.skin = skin;
	m_pCurrentEntity->baseline.scale = m_pCurrentEntity->prevstate.scale = m_pCurrentEntity->curstate.scale = scale;
	m_pCurrentEntity->baseline.frame = m_pCurrentEntity->prevstate.frame = m_pCurrentEntity->curstate.frame = frame;
	m_pCurrentEntity->baseline.framerate = m_pCurrentEntity->prevstate.framerate = m_pCurrentEntity->curstate.framerate = framerate;
	m_pCurrentEntity->baseline.animtime = m_pCurrentEntity->prevstate.animtime = m_pCurrentEntity->curstate.animtime = animtime;
	m_pCurrentEntity->baseline.sequence = m_pCurrentEntity->prevstate.sequence = m_pCurrentEntity->curstate.sequence = sequence;
	m_pCurrentEntity->baseline.origin = m_pCurrentEntity->prevstate.origin = m_pCurrentEntity->curstate.origin = origin;
	m_pCurrentEntity->baseline.angles = m_pCurrentEntity->prevstate.angles = m_pCurrentEntity->curstate.angles = angles;
	m_pCurrentEntity->baseline.effects = m_pCurrentEntity->prevstate.effects = m_pCurrentEntity->curstate.effects = effects;
	m_pCurrentEntity->baseline.rendermode = m_pCurrentEntity->prevstate.rendermode = m_pCurrentEntity->curstate.rendermode = rendermode;
	m_pCurrentEntity->baseline.renderamt = m_pCurrentEntity->prevstate.renderamt = m_pCurrentEntity->curstate.renderamt = renderamt;
	m_pCurrentEntity->baseline.renderfx = m_pCurrentEntity->prevstate.renderfx = m_pCurrentEntity->curstate.renderfx = renderfx;
	m_pCurrentEntity->baseline.rendercolor = m_pCurrentEntity->prevstate.rendercolor = m_pCurrentEntity->curstate.rendercolor = rendercolor;
	m_pCurrentEntity->origin = origin;
	m_pCurrentEntity->angles = angles;
	m_pCurrentEntity->curstate.movetype = MOVETYPE_NOCLIP;

	m_isCurrentModelMirrored = mirrored;

	GetTimes();
	IEngineStudio.GetViewInfo(m_vRenderOrigin, m_vUp, m_vRight, m_vNormal);
	IEngineStudio.GetAliasScale(&m_fSoftwareXScale, &m_fSoftwareYScale);

	m_pStudioHeader = Mod_Extradata("DrawVREntity", m_pCurrentEntity, m_pRenderModel);
	if (!m_pStudioHeader)
	{
		m_isCurrentModelMirrored = false;
		return false;
	}

	IEngineStudio.StudioSetHeader(m_pStudioHeader);
	IEngineStudio.SetRenderModel(m_pRenderModel);

	StudioSetUpTransform(false);

	VRQuaternion bone_quaternions[VRHAND_Bones_Count];
	Vector bone_positions[VRHAND_Bones_Count];
	float fingerCurl[Finger_Count];
	if (isController && m_pStudioHeader->numbones == VRHAND_Bones_Count && gVRRenderer.IsHandSkeletalModel(modelname) && gVRRenderer.HasSkeletalDataForHand(mirrored, bone_quaternions, bone_positions))
	{
		m_pCurrentEntity->curstate.frame = 0;
		m_pCurrentEntity->curstate.framerate = 0;
		m_pCurrentEntity->curstate.animtime = m_clTime;
		m_pCurrentEntity->curstate.sequence = IDLE;

		mstudiobone_t* pbones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->boneindex);

		for (int i = 0; i < VRHAND_Bones_Count; i++)
		{
			float bonematrix[3][4];

			vec_t quat[4];
			quat[0] = bone_quaternions[i].x;
			quat[1] = bone_quaternions[i].y;
			quat[2] = bone_quaternions[i].z;
			quat[3] = bone_quaternions[i].w;
			QuaternionMatrix(quat, bonematrix);
			bonematrix[0][3] = bone_positions[i][0];
			bonematrix[1][3] = bone_positions[i][1];
			bonematrix[2][3] = bone_positions[i][2];

			if (pbones[i].parent == -1)
			{
				ConcatTransforms((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);
			}
			else
			{
				ConcatTransforms((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			}
			MatrixCopy((*m_pbonetransform)[i], (*m_plighttransform)[i]);
		}
	}
	else if (isController && gVRRenderer.IsHandModel(modelname) && gVRRenderer.HasFingerDataForHand(mirrored, fingerCurl))
	{
		// Use skeletal data from OpenVR to animate curled fingers on hand models:
		// 1. Call StudioSetupBones twice: Once with initial frame of IDLE (flat hand), and once with initial frame of FULLGRAB_END (fist)
		// 2. Use fingerCurl data (0 = flat hand, 1 = fully curled) to interpolate between appropriate bone matrices
		// (Use hardcoded map of bone names (g_handmodelfingerbonenames) to modify correct bone matrices)
		// - Max Makes Mods, 2019-10-22

		m_pCurrentEntity->curstate.frame = 0;
		m_pCurrentEntity->curstate.framerate = 0;
		m_pCurrentEntity->curstate.animtime = m_clTime;
		m_pCurrentEntity->curstate.sequence = IDLE;
		StudioSetupBones();

		mstudiobone_t* pbones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->boneindex);
		for (int finger = 0; finger < Finger_Count; finger++)
		{
			float f = fingerCurl[finger];
			if (f > 0.f)
			{
				m_pCurrentEntity->curstate.frame = 0;
				m_pCurrentEntity->curstate.framerate = 0;
				m_pCurrentEntity->curstate.animtime = m_clTime;
				m_pCurrentEntity->curstate.sequence = FULLGRAB_START;

				mstudioseqdesc_t* pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;
				float overrideFrame = (pseqdesc->numframes - 1) * f;
				m_pCurrentEntity->curstate.frame = overrideFrame;

				float pGrabbingBoneTransform[MAXSTUDIOBONES][3][4];
				float pGrabbingLightTransform[MAXSTUDIOBONES][3][4];
				StudioSetupBonesInline(pGrabbingBoneTransform, pGrabbingLightTransform, &overrideFrame);

				for (int i = 0; i < m_pStudioHeader->numbones; i++)
				{
					if (IsFingerBoneName(Finger(finger), pbones[i].name))
					{
						MatrixCopy(pGrabbingBoneTransform[i], (*m_pbonetransform)[i]);
						MatrixCopy(pGrabbingLightTransform[i], (*m_plighttransform)[i]);
					}
				}
			}
		}

		m_pCurrentEntity->curstate.frame = 0;
		m_pCurrentEntity->curstate.framerate = 0;
		m_pCurrentEntity->curstate.animtime = m_clTime;
		m_pCurrentEntity->curstate.sequence = IDLE;
	}
	else
	{
		StudioSetupBones();

		if (isController)
		{
			StudioDispatchVRSoundEvents(modelname, origin, mirrored);
		}
	}
	StudioSaveBones();

	if (m_isCurrentModelMirrored)
		gVRRenderer.ReverseCullface();
	else
		gVRRenderer.RestoreCullface();

	alight_t lighting;
	Vector dir;
	lighting.plightvec = dir;
	IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);
	IEngineStudio.StudioEntityLight(&lighting);

	// model and frame independant
	IEngineStudio.StudioSetupLighting(&lighting);

	// get remap colors
	m_nTopColor = m_pCurrentEntity->curstate.colormap & 0xFF;
	m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;

	IEngineStudio.StudioSetRemapColors(m_nTopColor, m_nBottomColor);

	StudioRenderModel();

	gVRRenderer.RestoreCullface();

	m_isCurrentModelMirrored = false;

	return true;
}

void CStudioModelRenderer::StudioDrawVRHand(const ControllerModelData& controllerModelData, const Vector& origin, const Vector& angles, bool mirrored, int* out_numattachments, float out_attachments[4][3])
{
	std::string controllermodelname = controllerModelData.controller.modelname;

	if (DrawVREntity(controllermodelname.c_str(),
			origin, angles,
			controllerModelData.controller.body, controllerModelData.controller.skin, controllerModelData.controller.scale,
			controllerModelData.controller.frame, controllerModelData.controller.framerate,
			controllerModelData.controller.animtime, controllerModelData.controller.sequence,
			controllerModelData.controller.effects,
			controllerModelData.controller.rendermode, controllerModelData.controller.renderamt, controllerModelData.controller.renderfx, controllerModelData.controller.rendercolor,
			true, mirrored))
	{
		int numattachments = std::min(4, m_pStudioHeader->numattachments);
		mstudioattachment_t* pattachment = reinterpret_cast<mstudioattachment_t*>(reinterpret_cast<byte*>(m_pStudioHeader) + m_pStudioHeader->attachmentindex);
		for (int i = 0; i < numattachments; i++)
		{
			VectorTransform(pattachment[i].org, (*m_plighttransform)[pattachment[i].bone], out_attachments[i]);
		}
		*out_numattachments = numattachments;
	}

	if (controllerModelData.hasDraggedEnt)
	{
		DrawVREntity(controllerModelData.draggedEnt.modelname,
			origin, angles,
			controllerModelData.draggedEnt.body, controllerModelData.draggedEnt.skin, controllerModelData.draggedEnt.scale,
			controllerModelData.draggedEnt.frame, controllerModelData.draggedEnt.framerate,
			controllerModelData.draggedEnt.animtime, controllerModelData.draggedEnt.sequence,
			controllerModelData.draggedEnt.effects,
			controllerModelData.draggedEnt.rendermode, controllerModelData.draggedEnt.renderamt, controllerModelData.draggedEnt.renderfx, controllerModelData.draggedEnt.rendercolor,
			false, false);
	}
}

bool IsDraggedEntity(cl_entity_t* ent)
{
	return (gVRRenderer.HasValidLeftController() && gHUD.m_leftControllerModelData.hasDraggedEnt && ent->index == gHUD.m_leftControllerModelData.draggedEntIndex) || (gVRRenderer.HasValidRightController() && gHUD.m_rightControllerModelData.hasDraggedEnt && ent->index == gHUD.m_rightControllerModelData.draggedEntIndex);
}

int VRGlobalNumAttachmentsForEntity(cl_entity_t* ent)
{
	if (ent && ent->model && ent->model->type == mod_studio)
	{
		studiohdr_t* pstudiohdr = Mod_Extradata("VRGlobalNumAttachmentsForEntity", ent, ent->model);
		if (pstudiohdr)
		{
			return pstudiohdr->numattachments;
		}
	}
	return 0;
}
