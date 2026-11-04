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
extern "C" {
#endif

#define Q_PI 3.14159265358979323846

#define VectorScale(a, b, c)   \
	{                          \
		(c)[0] = (b) * (a)[0]; \
		(c)[1] = (b) * (a)[1]; \
		(c)[2] = (b) * (a)[2]; \
	}

double VectorLength(Vector v);

void AngleQuaternion(const Vector angles, vec4_t quaternion);
void QuaternionMatrix(const vec4_t quaternion, float (*matrix)[4]);
void QuaternionSlerp(const vec4_t p, vec4_t q, float t, vec4_t qt);


#ifdef __cplusplus
}
#endif

#endif
