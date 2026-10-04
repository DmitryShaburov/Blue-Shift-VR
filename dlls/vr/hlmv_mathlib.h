/***
*
*	Copyright (c) 1998, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/

// Modified to compile with hl.dll - Max Makes Mods - 2019-04-07

#ifndef __MATHLIB__
#define __MATHLIB__

// mathlib.h

#include <math.h>
#include "Platform.h"
#include "mathlib.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SIDE_FRONT 0
#define SIDE_ON    2
#define SIDE_BACK  1
#define SIDE_CROSS -2

#define Q_PI 3.14159265358979323846

	// Use this definition globally
#define ON_EPSILON    0.01
#define EQUAL_EPSILON 0.001

#define VectorFill(a, b) \
	{                    \
		(a)[0] = (b);    \
		(a)[1] = (b);    \
		(a)[2] = (b);    \
	}
#define VectorAvg(a) (((a)[0] + (a)[1] + (a)[2]) / 3)
#define VectorScale(a, b, c)   \
	{                          \
		(c)[0] = (b) * (a)[0]; \
		(c)[1] = (b) * (a)[1]; \
		(c)[2] = (b) * (a)[2]; \
	}

	vec_t Q_rint(vec_t in);

	void _VectorScale(Vector v, vec_t scale, Vector out);

	double VectorLength(Vector v);

	void ClearBounds(Vector mins, Vector maxs);
	void AddPointToBounds(Vector v, Vector mins, Vector maxs);

	void AngleQuaternion(const Vector angles, vec4_t quaternion);
	void QuaternionMatrix(const vec4_t quaternion, float(*matrix)[4]);
	void QuaternionSlerp(const vec4_t p, vec4_t q, float t, vec4_t qt);


#ifdef __cplusplus
}
#endif

#endif
