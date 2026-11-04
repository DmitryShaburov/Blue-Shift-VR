#pragma once


enum class VRPoseType
{
	LEFT_HAND,
	RIGHT_HAND,
	LEFT_POINTER,
	RIGHT_POINTER
};

enum class VREyeMode
{
	Both,
	LeftOnly,		   //  Left eye is rendered, right eye is black
	RightOnly,		   // Right eye is rendered,  left eye is black
	LeftOnlyMirrored,  //  Left eye is rendered, right eye is mirror of left eye
	RightOnlyMirrored, // Right eye is rendered,  left eye is mirror of right eye
	PancakeMode
};

struct VRQuaternion
{
	float x, y, z, w;
};
