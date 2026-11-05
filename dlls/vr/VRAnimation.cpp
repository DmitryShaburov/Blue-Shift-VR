// Server-side bone setup for hitboxes and attachments, moved from animation.cpp

#include "extdll.h"
#include "util.h"

#include "studio.h"

#pragma warning(disable : 4244)

#include "vr/VRAnimation.h"
#include "vr/hlmv_mathlib.h"

int GetNumSequences(void* pmodel)
{
	studiohdr_t* pstudiohdr = static_cast<studiohdr_t*>(pmodel);
	if (!pstudiohdr)
		return 0;
	return pstudiohdr->numseq;
}

// Blatantly copied from client.dll's StudioSetupBones, modified for our VR weapons - Max Makes Mods, 2019-07-13
namespace
{
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef PITCH
// up / down
#define PITCH 0
#endif
#ifndef YAW
// left / right
#define YAW 1
#endif
#ifndef ROLL
// fall over
#define ROLL 2
#endif
void StudioCalcBoneAdj(studiohdr_t* pstudiohdr, float* adj, const byte* pcontroller)
{
	int i, j;
	float value = 0.f;
	mstudiobonecontroller_t* pbonecontroller = reinterpret_cast<mstudiobonecontroller_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->bonecontrollerindex);

	for (j = 0; j < pstudiohdr->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if ((pbonecontroller[j].type & STUDIO_RLOOP) != 0)
			{
				value = pcontroller[i] * (360.0 / 256.0) + pbonecontroller[j].start;
			}
			else
			{
				value = pcontroller[i] / 255.f;
				if (value < 0)
					value = 0;
				if (value > 1.f)
					value = 1.f;
				value = (1.f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
		}
		else
		{
			value = 0.f;
			value = (1.f - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}
// int VectorCompare(const float* v1, const float* v2)
// {
// 	int i = 0;

// 	for (i = 0; i < 3; i++)
// 		if (v1[i] != v2[i])
// 			return 0;

// 	return 1;
// }
void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q)
{
	int j, k;
	vec4_t q1, q2;
	Vector angle1, angle2;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			mstudioanimvalue_t* panimvalue = reinterpret_cast<mstudioanimvalue_t*>(reinterpret_cast<byte*>(panim) + panim->offset[j + 3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j + 3]];
			angle2[j] += adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1, angle2))
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1, q);
	}
}
void StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos)
{
	int j, k;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			mstudioanimvalue_t* panimvalue = reinterpret_cast<mstudioanimvalue_t*>(reinterpret_cast<byte*>(panim) + panim->offset[j]);
			/*
				if (i == 0 && j == 0)
					Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
				*/

			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.f - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.f - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1 && adj)
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}
void StudioCalcRotations(entvars_t* pev, studiohdr_t* pstudiohdr, float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f)
{
	int i = 0;
	int frame = 0;

	float s = 0.f;
	float adj[MAXSTUDIOCONTROLLERS];
	float dadt = 0.f;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0;
	}
	else if (f < -0.01f)
	{
		f = -0.01f;
	}

	frame = (int)f;

	dadt = 1.f;
	s = (f - frame);

	// add in programtic controllers
	mstudiobone_t* pbone = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->boneindex);

	StudioCalcBoneAdj(pstudiohdr, adj, pev->controller);

	for (i = 0; i < pstudiohdr->numbones; i++, pbone++, panim++)
	{
		StudioCalcBoneQuaterion(frame, s, pbone, panim, adj, q[i]);

		StudioCalcBonePosition(frame, s, pbone, panim, adj, pos[i]);
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
	}

	if ((pseqdesc->motiontype & STUDIO_X) != 0)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Y) != 0)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if ((pseqdesc->motiontype & STUDIO_Z) != 0)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.f - (f - (int)(f))) / (pseqdesc->numframes)) * pev->framerate;

	if ((pseqdesc->motiontype & STUDIO_LX) != 0)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if ((pseqdesc->motiontype & STUDIO_LY) != 0)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if ((pseqdesc->motiontype & STUDIO_LZ) != 0)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}
void StudioSlerpBones(studiohdr_t* pstudiohdr, vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s)
{
	int i = 0;
	vec4_t q3;
	float s1 = 0.f;

	if (s < 0)
		s = 0;
	else if (s > 1.0)
		s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}
// void AngleMatrix(const float* angles, float(*matrix)[4])
// {
// 	float angle = 0.f;
// 	float sr, sp, sy, cr, cp, cy;

// 	angle = angles[YAW] * (M_PI * 2 / 360);
// 	sy = sin(angle);
// 	cy = cos(angle);
// 	angle = angles[PITCH] * (M_PI * 2 / 360);
// 	sp = sin(angle);
// 	cp = cos(angle);
// 	angle = angles[ROLL] * (M_PI * 2 / 360);
// 	sr = sin(angle);
// 	cr = cos(angle);

// 	// matrix = (YAW * PITCH) * ROLL
// 	matrix[0][0] = cp * cy;
// 	matrix[1][0] = cp * sy;
// 	matrix[2][0] = -sp;
// 	matrix[0][1] = sr * sp * cy + cr * -sy;
// 	matrix[1][1] = sr * sp * sy + cr * cy;
// 	matrix[2][1] = sr * cp;
// 	matrix[0][2] = cr * sp * cy + -sr * -sy;
// 	matrix[1][2] = cr * sp * sy + -sr * cy;
// 	matrix[2][2] = cr * cp;
// 	matrix[0][3] = 0.0;
// 	matrix[1][3] = 0.0;
// 	matrix[2][3] = 0.0;
// }
void MatrixAngles(const float (*matrix)[4], float* angles)
{
	float cp = sqrtf(matrix[0][0] * matrix[0][0] + matrix[1][0] * matrix[1][0]);
	if (cp < 0.000001f)
	{
		angles[YAW] = 0;
		angles[PITCH] = atan2(-matrix[2][0], cp) * 180.f / M_PI;
		angles[ROLL] = atan2(-matrix[1][2], matrix[1][1]) * 180.f / M_PI;
	}
	else
	{
		angles[YAW] = atan2(matrix[1][0], matrix[0][0]) * 180.f / M_PI;
		angles[PITCH] = atan2(-matrix[2][0], cp) * 180.f / M_PI;
		angles[ROLL] = atan2(matrix[2][1], matrix[2][2]) * 180.f / M_PI;
	}
}
void MatrixCopy(float in[3][4], float out[3][4])
{
	memcpy(out, in, sizeof(float) * 3 * 4);
}
void ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}
void StudioSetUpTransform(entvars_t* pev, float (*modeltransform)[3][4], bool mirrored)
{
	Vector angles;
	Vector modelpos;

	VectorCopy(pev->origin, modelpos);
	VectorCopy(pev->angles, angles);

	angles[PITCH] = -angles[PITCH];
	AngleMatrix(angles, (*modeltransform));

	// Mirror hand model
	if (mirrored)
	{
		// create mirror matrix
		static float mirrormatrix[3][4] = {
			{1.f, 0.f, 0.f, 1.f},
			{0.f, -1.f, 0.f, 1.f},
			{0.f, 0.f, 1.f, 1.f},
		};

		// copy rotation matrix
		float modeltransform_copy[3][4] = {0};
		MatrixCopy(*modeltransform, modeltransform_copy);

		// concat mirror and rotation matrix
		ConcatTransforms(modeltransform_copy, mirrormatrix, (*modeltransform));
	}

	(*modeltransform)[0][3] = modelpos[0];
	(*modeltransform)[1][3] = modelpos[1];
	(*modeltransform)[2][3] = modelpos[2];
}
void VectorTransform(const Vector& in1, float in2[3][4], Vector& out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}
struct BoneTransform
{
	Vector origin;
	Vector angles;
};
int GetBoneTransforms(entvars_t* pev, studiohdr_t* pstudiohdr, int sequence, float frame, BoneTransform* bonetransforms, StudioAttachment* attachments, bool mirrored)
{
	if (pstudiohdr->numbones <= 0)
		return 0;

	if (pstudiohdr->numbones > MAXSTUDIOBONES)
		return 0;

	float modeltransform[3][4];
	StudioSetUpTransform(pev, &modeltransform, mirrored);

	int i = 0;

	static float pos[MAXSTUDIOBONES][3];
	static vec4_t q[MAXSTUDIOBONES];
	float bonematrix[3][4];

	static float pos2[MAXSTUDIOBONES][3];
	static vec4_t q2[MAXSTUDIOBONES];
	static float pos3[MAXSTUDIOBONES][3];
	static vec4_t q3[MAXSTUDIOBONES];
	static float pos4[MAXSTUDIOBONES][3];
	static vec4_t q4[MAXSTUDIOBONES];

	if (sequence >= pstudiohdr->numseq)
	{
		sequence = 0;
		frame = 0.f;
	}

	mstudioseqdesc_t* pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->seqindex) + sequence;
	if (pseqdesc->seqgroup != 0)
	{
		sequence = 0;
		frame = 0.f;
		pseqdesc = reinterpret_cast<mstudioseqdesc_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->seqindex);
	}

	if (pseqdesc->seqgroup != 0)
	{
		return 0;
	}

	mstudioseqgroup_t* pseqgroup = reinterpret_cast<mstudioseqgroup_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->seqgroupindex) + pseqdesc->seqgroup;
	mstudioanim_t* panim = reinterpret_cast<mstudioanim_t*>(reinterpret_cast<byte*>(pstudiohdr) + pseqgroup->unused2 + pseqdesc->animindex);

	StudioCalcRotations(pev, pstudiohdr, pos, q, pseqdesc, panim, frame);

	if (pseqdesc->numblends > 1)
	{
		float s = 0.f;

		panim += pstudiohdr->numbones;
		StudioCalcRotations(pev, pstudiohdr, pos2, q2, pseqdesc, panim, frame);

		s = (pev->blending[0]) / 255.f;

		StudioSlerpBones(pstudiohdr, q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += pstudiohdr->numbones;
			StudioCalcRotations(pev, pstudiohdr, pos3, q3, pseqdesc, panim, frame);

			panim += pstudiohdr->numbones;
			StudioCalcRotations(pev, pstudiohdr, pos4, q4, pseqdesc, panim, frame);

			s = (pev->blending[0]) / 255.f;
			StudioSlerpBones(pstudiohdr, q3, pos3, q4, pos4, s);

			s = (pev->blending[1]) / 255.f;
			StudioSlerpBones(pstudiohdr, q, pos, q3, pos3, s);
		}
	}

	mstudiobone_t* pbones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->boneindex);

	if (pev->scale != 0.f)
	{
		for (int j = 0; j < 3; j++)
		{
			for (int k = 0; k < 3; k++)
			{
				modeltransform[j][k] *= pev->scale;
			}
		}
	}

	float bonetransform[MAXSTUDIOBONES][3][4];

	for (i = 0; i < pstudiohdr->numbones; i++)
	{
		QuaternionMatrix(q[i], bonematrix);

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1)
		{
			ConcatTransforms(modeltransform, bonematrix, bonetransform[i]);
		}
		else
		{
			ConcatTransforms(bonetransform[pbones[i].parent], bonematrix, bonetransform[i]);
		}

		bonetransforms[i].origin[0] = bonetransform[i][0][3];
		bonetransforms[i].origin[1] = bonetransform[i][1][3];
		bonetransforms[i].origin[2] = bonetransform[i][2][3];
		MatrixAngles(bonetransform[i], bonetransforms[i].angles);
		if (mirrored)
		{
			bonetransforms[i].angles[ROLL] = -bonetransforms[i].angles[ROLL];
		}
	}

	// calculate attachment points
	if (attachments)
	{
		mstudioattachment_t* pattachment = reinterpret_cast<mstudioattachment_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->attachmentindex);
		for (int i = 0; i < pstudiohdr->numattachments; i++)
		{
			VectorTransform(pattachment[i].org, bonetransform[pattachment[i].bone], attachments[i].pos);
		}
	}

	return 1;
}

} // namespace

int GetNumHitboxes(void* pmodel)
{
	studiohdr_t* pstudiohdr = static_cast<studiohdr_t*>(pmodel);
	if (!pstudiohdr)
		return 0;

	return pstudiohdr->numhitboxes;
}
int GetNumBodies(void* pmodel)
{
	studiohdr_t* pstudiohdr = static_cast<studiohdr_t*>(pmodel);
	if (!pstudiohdr)
		return 0;

	return pstudiohdr->numbodyparts;
}
int GetNumAttachments(void* pmodel)
{
	studiohdr_t* pstudiohdr = static_cast<studiohdr_t*>(pmodel);
	if (!pstudiohdr)
		return 0;

	return pstudiohdr->numattachments;
}
int GetHitboxesAndAttachments(entvars_t* pev, void* pmodel, int sequence, float frame, StudioHitBox* hitboxes, StudioAttachment* attachments, bool mirrored)
{
	studiohdr_t* pstudiohdr = static_cast<studiohdr_t*>(pmodel);
	if (!pstudiohdr)
		return 0;

	BoneTransform bonetransforms[MAXSTUDIOBONES];
	if (0 == GetBoneTransforms(pev, pstudiohdr, sequence, frame, bonetransforms, attachments, mirrored))
		return 0;

	mstudiobbox_t* pbbox = reinterpret_cast<mstudiobbox_t*>(reinterpret_cast<byte*>(pstudiohdr) + pstudiohdr->hitboxindex);

	for (int i = 0; i < pstudiohdr->numhitboxes; i++)
	{
		auto& bbox = pbbox[i];
		auto& hitbox = hitboxes[i];

		if (pev->scale != 0.f)
		{
			VectorScale(bbox.bbmin, pev->scale, hitbox.mins);
			VectorScale(bbox.bbmax, pev->scale, hitbox.maxs);
		}
		else
		{
			VectorCopy(bbox.bbmin, hitbox.mins);
			VectorCopy(bbox.bbmax, hitbox.maxs);
		}

		VectorCopy(bonetransforms[bbox.bone].origin, hitbox.origin);
		VectorCopy(bonetransforms[bbox.bone].angles, hitbox.angles);
		hitbox.hitgroup = bbox.group;

		// Some models have hitboxes with size 0. Fix that here
		Vector size;
		VectorSubtract(hitbox.maxs, hitbox.mins, size);
		for (int j = 0; j < 3; j++)
		{
			if (fabs(size[j]) < 0.1f)
			{
				hitbox.mins[j] = -4.f;
				hitbox.maxs[j] = 4.f;
			}
		}

		// Defined in VRPhysicsHelper.cpp
		extern void CalculateHitboxAbsCenter(StudioHitBox & hitbox);
		CalculateHitboxAbsCenter(hitbox);
	}

	return 1;
}
