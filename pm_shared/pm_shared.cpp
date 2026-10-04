/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "Platform.h"

#include <assert.h>
#include "mathlib.h"
#include "cdll_dll.h"
#include "const.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_shared.h"
#include "pm_materials.h"
#include "pm_movevars.h"
#include "pm_debug.h"
#include <stdio.h>	// NULL
#include <string.h> // strcpy
#include <stdlib.h> // atoi
#include <ctype.h>	// isspace

#ifdef CLIENT_DLL
// Spectator Mode
bool iJumpSpectator;
float vJumpOrigin[3];
float vJumpAngles[3];
#endif

static bool pm_shared_initialized = false;

#pragma warning(disable : 4305)

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_studio
} modtype_t;

playermove_t* pmove = NULL;

// BSVR start
// Set by VRControllerInteractionManager while the player grabs a ladder with the controllers
bool pmoveIgnoreLadders = false;

// and again we are in include hell and use an extern declaration to escape :S
extern bool VRGlobalIsInstantAccelerateOn();
extern bool VRGlobalIsInstantDecelerateOn();
extern void VRGlobalGetEntityOrigin(int ent, float* entorigin);
extern bool VRGlobalGetNoclipMode();
extern bool VRNotifyStuckEnt(int player, int ent);
extern bool VRGlobalIsPointInsideEnt(const float* point, int ent);
extern void VRGetMaxClimbSpeed(float& updown, float& sideways);
extern void VRGetCVARSpeeds(float& forwardSpeed, float& backSpeed, float& sideSpeed, float& upSpeed);
extern int VRGetLadderMode();
extern int VRGetGrabbedLadder(int player);  // For client or server to use to identify (index into edicts or cl_entities)
inline bool VRIsGrabbingLadder() { return VRGetGrabbedLadder(pmove->player_index) > 0; }
extern bool VRIsPullingOnLedge(int player);
extern bool VRIsAutoDuckingEnabled(int player);
extern float VRGetSmoothStepsSetting();
extern bool VRIsInUpwardsTriggerPush(int player);

// No gravity under water in VR - Max Makes Mods, 2018-01-28
// Moved into separate function with improved logic - Max Makes Mods, 2018-11-18
// Disable gravity after using VR teleporter in water
// and reenable gravity after moving/jumping/crouching using default game inputs.
// (g_vrTeleportInWater is set to true in player.cpp on server dll when teleporting in water.)
bool g_vrTeleportInWater = false;
bool IsInWaterAndIsGravityDisabled()
{
	// If player uses vanilla game movements, reset g_vrTeleportInWater to false.
	if (pmove->cmd.forwardmove || pmove->cmd.sidemove || pmove->cmd.upmove || (pmove->cmd.buttons & (IN_JUMP | IN_DUCK)))
	{
		g_vrTeleportInWater = false;
	}
	return (pmove->waterlevel > 0) && g_vrTeleportInWater;
}
// BSVR end

typedef struct
{
	int planenum;
	short children[2]; // negative numbers are contents
} dclipnode_t;

typedef struct mplane_s
{
	Vector normal; // surface normal
	float dist;	   // closest appoach to origin
	byte type;	   // for texture axis selection and fast side tests
	byte signbits; // signx + signy<<1 + signz<<1
	byte pad[2];
} mplane_t;

typedef struct hull_s
{
	dclipnode_t* clipnodes;
	mplane_t* planes;
	int firstclipnode;
	int lastclipnode;
	Vector clip_mins;
	Vector clip_maxs;
} hull_t;

// Ducking time
#define TIME_TO_DUCK 0.4
#define MAX_CLIMB_SPEED 200
#define STUCK_MOVEUP 1
#define STUCK_MOVEDOWN -1
#define STOP_EPSILON 0.1

#define STEP_CONCRETE 0 // default step sound
#define STEP_METAL 1	// metal floor
#define STEP_DIRT 2		// dirt, sand, rock
#define STEP_VENT 3		// ventillation duct
#define STEP_GRATE 4	// metal grating
#define STEP_TILE 5		// floor tiles
#define STEP_SLOSH 6	// shallow liquid puddle
#define STEP_WADE 7		// wading in liquid
#define STEP_LADDER 8	// climbing ladder

#define PLAYER_FATAL_FALL_SPEED 1024															  // approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED 580															  // approx 20 feet
#define DAMAGE_FOR_FALL_SPEED (float)100 / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED) // damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED 200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

#define PLAYER_LONGJUMP_SPEED 350 // how fast we longjump

#define PLAYER_DUCKING_MULTIPLIER 0.333

// double to float warning
#pragma warning(disable : 4244)
// up / down
#define PITCH 0
// left / right
#define YAW 1
// fall over
#define ROLL 2

#define CONTENTS_CURRENT_0 -9
#define CONTENTS_CURRENT_90 -10
#define CONTENTS_CURRENT_180 -11
#define CONTENTS_CURRENT_270 -12
#define CONTENTS_CURRENT_UP -13
#define CONTENTS_CURRENT_DOWN -14

#define CONTENTS_TRANSLUCENT -15

static Vector rgv3tStuckTable[54];
static int rgStuckLast[MAX_PLAYERS][2];

// Texture names
static int gcTextures = 0;
static char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];
static char grgchTextureType[CTEXTURESMAX];

bool g_onladder = false;

static void PM_InitTrace(trace_t* trace, const Vector& end)
{
	memset(trace, 0, sizeof(*trace));
	VectorCopy(end, trace->endpos);
	trace->allsolid = 1;
	trace->fraction = 1.0f;
}

static void PM_TraceModel(physent_t* pEnt, const Vector& start, const Vector& end, trace_t* trace)
{
	PM_InitTrace(trace, end);
	pmove->PM_TraceModel(pEnt, start, end, trace);
}

void PM_SwapTextures(int i, int j)
{
	char chTemp;
	char szTemp[CBTEXTURENAMEMAX];

	strcpy(szTemp, grgszTextureName[i]);
	chTemp = grgchTextureType[i];

	strcpy(grgszTextureName[i], grgszTextureName[j]);
	grgchTextureType[i] = grgchTextureType[j];

	strcpy(grgszTextureName[j], szTemp);
	grgchTextureType[j] = chTemp;
}

void PM_SortTextures()
{
	// Bubble sort, yuck, but this only occurs at startup and it's only 512 elements...
	//
	int i, j;

	for (i = 0; i < gcTextures; i++)
	{
		for (j = i + 1; j < gcTextures; j++)
		{
			if (stricmp(grgszTextureName[i], grgszTextureName[j]) > 0)
			{
				// Swap
				//
				PM_SwapTextures(i, j);
			}
		}
	}
}

void PM_InitTextureTypes()
{
	char buffer[512];
	int i, j;
	byte* pMemFile;
	int fileSize, filePos;
	static bool bTextureTypeInit = false;

	if (bTextureTypeInit)
		return;

	memset(&(grgszTextureName[0][0]), 0, CTEXTURESMAX * CBTEXTURENAMEMAX);
	memset(grgchTextureType, 0, CTEXTURESMAX);

	gcTextures = 0;
	memset(buffer, 0, 512);

	fileSize = pmove->COM_FileSize("sound/materials.txt");
	pMemFile = pmove->COM_LoadFile("sound/materials.txt", 5, NULL);
	if (!pMemFile)
		return;

	filePos = 0;
	// for each line in the file...
	while (pmove->memfgets(pMemFile, fileSize, &filePos, buffer, 511) != NULL && (gcTextures < CTEXTURESMAX))
	{
		// skip whitespace
		i = 0;
		while ('\0' != buffer[i] && 0 != isspace(buffer[i]))
			i++;

		if ('\0' == buffer[i])
			continue;

		// skip comment lines
		if (buffer[i] == '/' || 0 == isalpha(buffer[i]))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper(buffer[i++]);

		// skip whitespace
		while ('\0' != buffer[i] && 0 != isspace(buffer[i]))
			i++;

		if ('\0' == buffer[i])
			continue;

		// get sentence name
		j = i;
		while ('\0' != buffer[j] && 0 == isspace(buffer[j]))
			j++;

		if ('\0' == buffer[j])
			continue;

		// null-terminate name and save in sentences array
		j = V_min(j, CBTEXTURENAMEMAX - 1 + i);
		buffer[j] = 0;
		strcpy(&(grgszTextureName[gcTextures++][0]), &(buffer[i]));
	}

	// Must use engine to free since we are in a .dll
	pmove->COM_FreeFile(pMemFile);

	PM_SortTextures();

	bTextureTypeInit = true;
}

char PM_FindTextureType(const char* name)
{
	int left, right, pivot;
	int val;

	assert(pm_shared_initialized);

	left = 0;
	right = gcTextures - 1;

	while (left <= right)
	{
		pivot = (left + right) / 2;

		val = strnicmp(name, grgszTextureName[pivot], CBTEXTURENAMEMAX - 1);
		if (val == 0)
		{
			return grgchTextureType[pivot];
		}
		else if (val > 0)
		{
			left = pivot + 1;
		}
		else if (val < 0)
		{
			right = pivot - 1;
		}
	}

	return CHAR_TEX_CONCRETE;
}

void PM_PlayStepSound(int step, float fvol)
{
	static int iSkipStep = 0;
	int irand;
	Vector hvel;

	pmove->iStepLeft = 0 != pmove->iStepLeft ? 0 : 1;

	if (0 == pmove->runfuncs)
	{
		return;
	}

	irand = pmove->RandomLong(0, 1) + (pmove->iStepLeft * 2);

	// FIXME mp_footsteps needs to be a movevar
	if (0 != pmove->multiplayer && 0 == pmove->movevars->footsteps)
		return;

	VectorCopy(pmove->velocity, hvel);
	hvel[2] = 0.0;

	if (0 != pmove->multiplayer && (!g_onladder && Length(hvel) <= 220))
		return;

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	// FIXME, move to player state

	switch (step)
	{
	default:
	case STEP_CONCRETE:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_step4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_METAL:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_metal4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_DIRT:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_dirt4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_VENT:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_duct4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_GRATE:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_grate4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_TILE:
		if (0 == pmove->RandomLong(0, 4))
			irand = 4;
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 4:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_tile5.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_SLOSH:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_slosh4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_WADE:
		if (iSkipStep == 0)
		{
			iSkipStep++;
			break;
		}

		if (iSkipStep++ == 3)
		{
			iSkipStep = 0;
		}

		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	case STEP_LADDER:
		switch (irand)
		{
		// right foot
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder1.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder3.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		// left foot
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder2.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_ladder4.wav", fvol, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
		break;
	}
}

int PM_MapTextureTypeStepType(char chTextureType)
{
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE:
		return STEP_CONCRETE;
	case CHAR_TEX_METAL:
		return STEP_METAL;
	case CHAR_TEX_DIRT:
		return STEP_DIRT;
	case CHAR_TEX_VENT:
		return STEP_VENT;
	case CHAR_TEX_GRATE:
		return STEP_GRATE;
	case CHAR_TEX_TILE:
		return STEP_TILE;
	case CHAR_TEX_SLOSH:
		return STEP_SLOSH;
	}
}

/*
====================
PM_CatagorizeTextureType

Determine texture info for the texture we are standing on.
====================
*/
void PM_CatagorizeTextureType()
{
	Vector start, end;
	const char* pTextureName;

	VectorCopy(pmove->origin, start);
	VectorCopy(pmove->origin, end);

	// Straight down
	end[2] -= 64;

	// Fill in default values, just in case.
	pmove->sztexturename[0] = '\0';
	pmove->chtexturetype = CHAR_TEX_CONCRETE;

	pTextureName = pmove->PM_TraceTexture(pmove->onground, start, end);
	if (!pTextureName)
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if (*pTextureName == '-' || *pTextureName == '+')
		pTextureName += 2;

	if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
		pTextureName++;
	// '}}'

	strcpy(pmove->sztexturename, pTextureName);
	pmove->sztexturename[CBTEXTURENAMEMAX - 1] = 0;

	// get texture type
	pmove->chtexturetype = PM_FindTextureType(pmove->sztexturename);
}

void PM_UpdateStepSound()
{
	float fvol;
	Vector knee;
	Vector feet;
	Vector center;
	float height;
	float speed;
	float velrun;
	float velwalk;
	float flduck;
	int step;

	if (pmove->flTimeStepSound > 0)
		return;

	if ((pmove->flags & FL_FROZEN) != 0)
		return;

	PM_CatagorizeTextureType();

	speed = Length(pmove->velocity);

	// determine if we are on a ladder
	const bool fLadder = (pmove->movetype == MOVETYPE_FLY); // IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!
	if ((pmove->flags & FL_DUCKING) != 0 || fLadder)
	{
		velwalk = 60; // These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;  // UNDONE: Move walking to server
		flduck = 100;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	//  play step sound.  Also, if pmove->flTimeStepSound is zero, get the new
	//  sound right away - we just started moving in new level.
	if ((fLadder || (pmove->onground != -1)) &&
		(Length(pmove->velocity) > 0.0) &&
		(speed >= velwalk || 0 == pmove->flTimeStepSound))
	{
		const bool fWalking = speed < velrun;

		VectorCopy(pmove->origin, center);
		VectorCopy(pmove->origin, knee);
		VectorCopy(pmove->origin, feet);

		height = pmove->player_maxs[pmove->usehull][2] - pmove->player_mins[pmove->usehull][2];

		knee[2] = pmove->origin[2] - 0.3 * height;
		feet[2] = pmove->origin[2] - 0.5 * height;

		// find out what we're stepping in or on...
		if (fLadder)
		{
			step = STEP_LADDER;
			fvol = 0.35;
			pmove->flTimeStepSound = 350;
		}
		else if (pmove->PM_PointContents(knee, NULL) == CONTENTS_WATER)
		{
			step = STEP_WADE;
			fvol = 0.65;
			pmove->flTimeStepSound = 600;
		}
		else if (pmove->PM_PointContents(feet, NULL) == CONTENTS_WATER)
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2 : 0.5;
			pmove->flTimeStepSound = fWalking ? 400 : 300;
		}
		else
		{
			// find texture under player, if different from current texture,
			// get material type
			step = PM_MapTextureTypeStepType(pmove->chtexturetype);

			switch (pmove->chtexturetype)
			{
			default:
			case CHAR_TEX_CONCRETE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_METAL:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_DIRT:
				fvol = fWalking ? 0.25 : 0.55;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_VENT:
				fvol = fWalking ? 0.4 : 0.7;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_TILE:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;

			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2 : 0.5;
				pmove->flTimeStepSound = fWalking ? 400 : 300;
				break;
			}
		}

		pmove->flTimeStepSound += flduck; // slower step time if ducking

		// play the sound
		// 35% volume if ducking
		if ((pmove->flags & FL_DUCKING) != 0)
		{
			fvol *= 0.35;
		}

		PM_PlayStepSound(step, fvol);
	}
}

/*
================
PM_AddToTouched

Add's the trace result to touch list, if contact is not already in list.
================
*/
bool PM_AddToTouched(pmtrace_t tr, Vector impactvelocity)
{
	int i;

	for (i = 0; i < pmove->numtouch; i++)
	{
		if (pmove->touchindex[i].ent == tr.ent)
			break;
	}
	if (i != pmove->numtouch) // Already in list.
		return false;

	VectorCopy(impactvelocity, tr.deltavelocity);

	if (pmove->numtouch >= MAX_PHYSENTS)
		pmove->Con_DPrintf("Too many entities were touched!\n");

	pmove->touchindex[pmove->numtouch++] = tr;
	return true;
}

/*
================
PM_CheckVelocity

See if the player has a bogus velocity value.
================
*/
void PM_CheckVelocity()
{
	int i;

	//
	// bound velocity
	//
	for (i = 0; i < 3; i++)
	{
		// See if it's bogus.
		if (IS_NAN(pmove->velocity[i]))
		{
			pmove->Con_Printf("PM  Got a NaN velocity %i\n", i);
			pmove->velocity[i] = 0;
		}
		if (IS_NAN(pmove->origin[i]))
		{
			pmove->Con_Printf("PM  Got a NaN origin on %i\n", i);
			pmove->origin[i] = 0;
		}

		// Bound it.
		if (pmove->velocity[i] > pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf("PM  Got a velocity too high on %i\n", i);
			pmove->velocity[i] = pmove->movevars->maxvelocity;
		}
		else if (pmove->velocity[i] < -pmove->movevars->maxvelocity)
		{
			pmove->Con_DPrintf("PM  Got a velocity too low on %i\n", i);
			pmove->velocity[i] = -pmove->movevars->maxvelocity;
		}
	}
}

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags:
0x01 == floor
0x02 == step / wall
==================
*/
int PM_ClipVelocity(Vector in, Vector normal, Vector& out, float overbounce)
{
	float backoff;
	float change;
	float angle;
	int i, blocked;

	angle = normal[2];

	blocked = 0x00;		 // Assume unblocked.
	if (angle > 0)		 // If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01; //
	if (0 == angle)		 // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02; //

	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct(in, normal) * overbounce;

	for (i = 0; i < 3; i++)
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		// If out velocity is too small, zero it out.
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	// Return blocking flags.
	return blocked;
}

void PM_AddCorrectGravity()
{
	float ent_gravity;

	if (0 != pmove->waterjumptime)
		return;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * 0.5 * pmove->frametime);
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;

	PM_CheckVelocity();
}


void PM_FixupGravityVelocity()
{
	float ent_gravity;

	if (0 != pmove->waterjumptime)
		return;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5);

	PM_CheckVelocity();
}

// BSVR start
void PM_VR_MoveAwayFromStuckEntityIfPossible(Vector end, pmtrace_t& trace)
{
	// TODO!
	trace.fraction = 1;
	trace.allsolid = false;
	VectorCopy(end, trace.endpos);
}
// BSVR end

/*
============
PM_FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
int PM_FlyMove()
{
	int bumpcount, numbumps;
	Vector dir;
	float d;
	int numplanes;
	Vector planes[MAX_CLIP_PLANES];
	Vector primal_velocity, original_velocity;
	Vector new_velocity;
	int i, j;
	pmtrace_t trace;
	Vector end;
	float time_left, allFraction;
	int blocked;

	numbumps = 4; // Bump up to four times

	blocked = 0;									// Assume not blocked
	numplanes = 0;									//  and not sliding along any planes
	VectorCopy(pmove->velocity, original_velocity); // Store original velocity
	VectorCopy(pmove->velocity, primal_velocity);

	allFraction = 0;
	time_left = pmove->frametime; // Total time for this movement operation.

	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (pmove->velocity == g_vecZero)
			break;

		// Assume we can move all the way from the current origin to the
		//  end point.
		for (i = 0; i < 3; i++)
			end[i] = pmove->origin[i] + time_left * pmove->velocity[i];

		// See if we can make it from origin to end point.
		trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_NORMAL, -1);

		// allFraction += trace.fraction; // BSVR moved down
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.
		if (0 != trace.allsolid)
		{
			// BSVR start
			PM_VR_MoveAwayFromStuckEntityIfPossible(end, trace);
			// BSVR original commented code
			// // entity is trapped in another solid
			// VectorCopy(vec3_origin, pmove->velocity);
			// //Con_DPrintf("Trapped 4\n");
			// return 4;
		}

		allFraction += trace.fraction; // BSVR moved from above

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and
		//  zero the plane counter.
		if (trace.fraction > 0)
		{ // actually covered some distance
			VectorCopy(trace.endpos, pmove->origin);
			VectorCopy(pmove->velocity, original_velocity);
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (trace.fraction == 1)
			break; // moved the entire distance

		//if (!trace.ent)
		//	Sys_Error ("PM_PlayerTrace: !trace.ent");

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		PM_AddToTouched(trace, pmove->velocity);

		// If the plane we hit has a high z component in the normal, then
		//  it's probably a floor
		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1; // floor
		}
		// If the plane has a zero z component in the normal, then it's a
		//  step or wall
		if (0 == trace.plane.normal[2])
		{
			blocked |= 2; // step / wall
						  //Con_DPrintf("Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.fraction;

		// Did we run out of planes to clip against?
		if (numplanes >= MAX_CLIP_PLANES)
		{ // this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, pmove->velocity);
			//Con_DPrintf("Too many planes 4\n");

			break;
		}

		// Set up next clipping plane
		VectorCopy(trace.plane.normal, planes[numplanes]);
		numplanes++;
		//

		// modify original_velocity so it parallels all of the clip planes
		//
		// relfect player velocity
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1 && pmove->movetype == MOVETYPE_WALK && ((pmove->onground == -1) || (pmove->friction != 1)))
		{
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i][2] > 0.7)
				{ // floor or slope
					PM_ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					VectorCopy(new_velocity, original_velocity);
				}
				else
					PM_ClipVelocity(original_velocity, planes[i], new_velocity, 1.0 + pmove->movevars->bounce * (1 - pmove->friction));
			}

			VectorCopy(new_velocity, pmove->velocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				PM_ClipVelocity(
					original_velocity,
					planes[i],
					pmove->velocity,
					1);
				for (j = 0; j < numplanes; j++)
					if (j != i)
					{
						// Are we now moving against this plane?
						if (DotProduct(pmove->velocity, planes[j]) < 0)
							break; // not ok
					}
				if (j == numplanes) // Didn't have to clip, so we're ok
					break;
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{ // go along this plane
				// pmove->velocity is set in clipping call, no need to set again.
				;
			}
			else
			{ // go along the crease
				if (numplanes != 2)
				{
					//Con_Printf ("clip velocity, numplanes == %i\n",numplanes);
					VectorCopy(vec3_origin, pmove->velocity);
					//Con_DPrintf("Trapped 4\n");

					break;
				}
				CrossProduct(planes[0], planes[1], dir);
				d = DotProduct(dir, pmove->velocity);
				VectorScale(dir, d, pmove->velocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			//
			if (DotProduct(pmove->velocity, primal_velocity) <= 0)
			{
				//Con_DPrintf("Back\n");
				VectorCopy(vec3_origin, pmove->velocity);
				break;
			}
		}
	}

	if (allFraction == 0)
	{
		VectorCopy(vec3_origin, pmove->velocity);
		//Con_DPrintf( "Don't stick\n" );
	}

	return blocked;
}

/*
==============
PM_Accelerate
==============
*/
void PM_Accelerate(Vector wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if (0 != pmove->dead)
		return;

	// If waterjumping, don't accelerate
	if (0 != pmove->waterjumptime)
		return;

	// BSVR start
	// If player has enabled instant acceleration in VR,
	// we simply set the velocity to wishdir*wishspeed and are done here.
	if (VRGlobalIsInstantAccelerateOn())
	{
		VectorScale(wishdir, wishspeed, pmove->velocity);
		return;
	}
	// BSVR end

	// See if we are changing direction a bit
	currentspeed = DotProduct(pmove->velocity, wishdir);

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pmove->friction;

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	// Adjust velocity.
	for (i = 0; i < 3; i++)
	{
		pmove->velocity[i] += accelspeed * wishdir[i];
	}
}

/*
=====================
PM_WalkMove

Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
======================
*/
void PM_WalkMove()
{
	int clip;
	int oldonground;
	int i;

	Vector wishvel;
	float spd;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	Vector dest, start;
	Vector original, originalvel;
	Vector down, downvel;
	float downdist, updist;

	pmtrace_t trace;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2] = 0;

	VectorNormalize(pmove->forward); // Normalize remainder of vectors.
	VectorNormalize(pmove->right);	 //

	for (i = 0; i < 2; i++) // Determine x and y parts of velocity
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;

	wishvel[2] = 0; // Zero out z part of velocity

	VectorCopy(wishvel, wishdir); // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	//
	// Clamp to server defined max speed
	//
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale(wishvel, pmove->maxspeed / wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}

	// Set pmove velocity
	pmove->velocity[2] = 0;
	PM_Accelerate(wishdir, wishspeed, pmove->movevars->accelerate);
	pmove->velocity[2] = 0;

	// Add in any base velocity to the current velocity.
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	spd = Length(pmove->velocity);

	if (spd < 1.0f)
	{
		VectorClear(pmove->velocity);
		return;
	}

	// If we are not moving, do nothing
	//if (!pmove->velocity[0] && !pmove->velocity[1] && !pmove->velocity[2])
	//	return;

	oldonground = pmove->onground;

	// first try just moving to the destination
	dest[0] = pmove->origin[0] + pmove->velocity[0] * pmove->frametime;
	dest[1] = pmove->origin[1] + pmove->velocity[1] * pmove->frametime;
	dest[2] = pmove->origin[2];

	// first try moving directly to the next spot
	VectorCopy(dest, start);
	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);
	// If we made it all the way, then copy trace end
	//  as new player position.
	if (trace.fraction == 1)
	{
		VectorCopy(trace.endpos, pmove->origin);
		return;
	}

	if (oldonground == -1 && // Don't walk up stairs if not on ground.
		pmove->waterlevel == 0)
		return;

	if (0 != pmove->waterjumptime) // If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	//  take the move that goes farthest
	VectorCopy(pmove->origin, original);	  // Save out original pos &
	VectorCopy(pmove->velocity, originalvel); //  velocity.

	// Slide move
	clip = PM_FlyMove();

	// Copy the results out
	VectorCopy(pmove->origin, down);
	VectorCopy(pmove->velocity, downvel);

	// Reset original values.
	VectorCopy(original, pmove->origin);

	VectorCopy(originalvel, pmove->velocity);

	// Start out up one stair height
	VectorCopy(pmove->origin, dest);
	dest[2] += pmove->movevars->stepsize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);
	// If we started okay and made it part of the way at least,
	//  copy the results to the movement start position and then
	//  run another move try.
	if (0 == trace.startsolid && 0 == trace.allsolid)
	{
		VectorCopy(trace.endpos, pmove->origin);
	}

	// slide move the rest of the way.
	clip = PM_FlyMove();

	// Now try going back down from the end point
	//  press down the stepheight
	VectorCopy(pmove->origin, dest);
	dest[2] -= pmove->movevars->stepsize;

	trace = pmove->PM_PlayerTrace(pmove->origin, dest, PM_NORMAL, -1);

	// If we are not on the ground any more then
	//  use the original movement attempt
	if (trace.plane.normal[2] < 0.7)
		goto usedown;
	// If the trace ended up in empty space, copy the end
	//  over to the origin.
	if (0 == trace.startsolid && 0 == trace.allsolid)
	{
		VectorCopy(trace.endpos, pmove->origin);
	}
	// Copy this origion to up.
	VectorCopy(pmove->origin, pmove->up);

	// decide which one went farther
	downdist = (down[0] - original[0]) * (down[0] - original[0]) + (down[1] - original[1]) * (down[1] - original[1]);
	updist = (pmove->up[0] - original[0]) * (pmove->up[0] - original[0]) + (pmove->up[1] - original[1]) * (pmove->up[1] - original[1]);

	if (downdist > updist)
	{
	usedown:
		VectorCopy(down, pmove->origin);
		VectorCopy(downvel, pmove->velocity);
	}
	else // copy z value from slide move
		pmove->velocity[2] = downvel[2];
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
void PM_Friction()
{
	float* vel;
	float speed, newspeed, control;
	float friction;
	float drop;
	Vector newvel;

	// If we are in water jump cycle, don't apply friction
	if (0 != pmove->waterjumptime)
		return;

	// BSVR start
	// If player has enabled instant stop in VR,
	// we simply set the velocity to 0 and are done here.
	if (VRGlobalIsInstantDecelerateOn() || VRIsInUpwardsTriggerPush(pmove->player_index))
	{
		VectorClear(pmove->velocity);
		return;
	}
	// BSVR end

	// Get velocity
	vel = pmove->velocity;

	// Calculate speed
	speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]);

	// If too slow, return
	if (speed < 0.1f)
	{
		return;
	}

	drop = 0;

	// apply ground friction
	if (pmove->onground != -1) // On an entity that is the ground
	{
		Vector start, stop;
		pmtrace_t trace;

		start[0] = stop[0] = pmove->origin[0] + vel[0] / speed * 16;
		start[1] = stop[1] = pmove->origin[1] + vel[1] / speed * 16;
		start[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2];
		stop[2] = start[2] - 34;

		trace = pmove->PM_PlayerTrace(start, stop, PM_NORMAL, -1);

		if (trace.fraction == 1.0)
			friction = pmove->movevars->friction * pmove->movevars->edgefriction;
		else
			friction = pmove->movevars->friction;

		// Grab friction value.
		//friction = pmove->movevars->friction;

		friction *= pmove->friction; // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < pmove->movevars->stopspeed) ? pmove->movevars->stopspeed : speed;
		// Add the amount to t'he drop amount.
		drop += control * friction * pmove->frametime;
	}

	// apply water friction
	//	if (pmove->waterlevel)
	//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

	// scale the velocity
	newspeed = speed - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	newvel[0] = vel[0] * newspeed;
	newvel[1] = vel[1] * newspeed;
	newvel[2] = vel[2] * newspeed;

	VectorCopy(newvel, pmove->velocity);
}

void PM_AirAccelerate(Vector wishdir, float wishspeed, float accel)
{
	int i;
	float addspeed, accelspeed, currentspeed, wishspd = wishspeed;

	if (0 != pmove->dead)
		return;
	if (0 != pmove->waterjumptime)
		return;

	// Cap speed
	//wishspd = VectorNormalize (pmove->wishveloc);

	if (wishspd > 30)
		wishspd = 30;
	// Determine veer amount
	currentspeed = DotProduct(pmove->velocity, wishdir);
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if (addspeed <= 0)
		return;
	// Determine acceleration speed after acceleration

	// BSVR start
	// Upstream code moved to else
	if (VRGlobalIsInstantAccelerateOn() || VRIsInUpwardsTriggerPush(pmove->player_index))
	{
		accelspeed = addspeed;
	}
	else
	{
		accelspeed = accel * wishspeed * pmove->frametime * pmove->friction;
		// Cap it
		if (accelspeed > addspeed)
			accelspeed = addspeed;
	}
	// BSVR end

	// Adjust pmove vel.
	for (i = 0; i < 3; i++)
	{
		pmove->velocity[i] += accelspeed * wishdir[i];
	}
}

/*
===================
PM_WaterMove

===================
*/
void PM_WaterMove()
{
	int i;
	Vector wishvel;
	float wishspeed;
	Vector wishdir;
	Vector start, dest;
	Vector temp;
	pmtrace_t trace;

	float speed, newspeed, addspeed, accelspeed;

	//
	// user intentions
	//
	for (i = 0; i < 3; i++)
		wishvel[i] = pmove->forward[i] * pmove->cmd.forwardmove + pmove->right[i] * pmove->cmd.sidemove;

	// Sinking after no other movement occurs
	if (0 == pmove->cmd.forwardmove && 0 == pmove->cmd.sidemove && 0 == pmove->cmd.upmove)
		wishvel[2] -= 60; // drift towards bottom
	else				  // Go straight up by upmove amount.
		wishvel[2] += pmove->cmd.upmove;

	// Copy it over and determine speed
	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Cap speed.
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale(wishvel, pmove->maxspeed / wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}
	// Slow us down a bit.
	wishspeed *= 0.8;

	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
	// Water friction
	VectorCopy(pmove->velocity, temp);
	speed = VectorNormalize(temp);
	if (0 != speed)
	{
		newspeed = speed - pmove->frametime * speed * pmove->movevars->friction * pmove->friction;

		if (newspeed < 0)
			newspeed = 0;
		VectorScale(pmove->velocity, newspeed / speed, pmove->velocity);
	}
	else
		newspeed = 0;

	//
	// water acceleration
	//
	if (wishspeed < 0.1f)
	{
		return;
	}

	addspeed = wishspeed - newspeed;
	if (addspeed > 0)
	{

		VectorNormalize(wishvel);
		accelspeed = pmove->movevars->accelerate * wishspeed * pmove->frametime * pmove->friction;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishvel[i];
	}

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	VectorMA(pmove->origin, pmove->frametime, pmove->velocity, dest);
	VectorCopy(dest, start);
	start[2] += pmove->movevars->stepsize + 1;
	trace = pmove->PM_PlayerTrace(start, dest, PM_NORMAL, -1);
	if (0 == trace.startsolid && 0 == trace.allsolid) // FIXME: check steep slope?
	{												  // walked up the step, so just keep result and exit
		VectorCopy(trace.endpos, pmove->origin);
		return;
	}

	// Try moving straight along out normal path.
	PM_FlyMove();
}


/*
===================
PM_AirMove

===================
*/
void PM_AirMove()
{
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2] = 0;
	// Renormalize
	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);

	// Determine x and y parts of velocity
	for (i = 0; i < 2; i++)
	{
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
	}
	// Zero out z part of velocity
	wishvel[2] = 0;

	// Determine maginitude of speed of move
	VectorCopy(wishvel, wishdir);
	wishspeed = VectorNormalize(wishdir);

	// Clamp to server defined max speed
	if (wishspeed > pmove->maxspeed)
	{
		VectorScale(wishvel, pmove->maxspeed / wishspeed, wishvel);
		wishspeed = pmove->maxspeed;
	}

	PM_AirAccelerate(wishdir, wishspeed, pmove->movevars->airaccelerate);

	// Add in any base velocity to the current velocity.
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_FlyMove();
}

bool PM_InWater()
{
	return (pmove->waterlevel > 1);
}

/*
=============
PM_CheckWater

Sets pmove->waterlevel and pmove->watertype values.
=============
*/
bool PM_CheckWater()
{
	Vector point;
	int cont;
	int truecont;
	float height;
	float heightover2;

	// Pick a spot just above the players feet.
	point[0] = pmove->origin[0] + (pmove->player_mins[pmove->usehull][0] + pmove->player_maxs[pmove->usehull][0]) * 0.5;
	point[1] = pmove->origin[1] + (pmove->player_mins[pmove->usehull][1] + pmove->player_maxs[pmove->usehull][1]) * 0.5;
	point[2] = pmove->origin[2] + pmove->player_mins[pmove->usehull][2] + 1;

	// Assume that we are not in water at all.
	pmove->waterlevel = 0;
	pmove->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = pmove->PM_PointContents(point, &truecont);
	// Are we under water? (not solid and not empty?)
	if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
	{
		// Set water type
		pmove->watertype = cont;

		// We are at least at level one
		pmove->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull][2] + pmove->player_maxs[pmove->usehull][2]);
		heightover2 = height * 0.5;

		// Now check a point that is at the player hull midpoint.
		point[2] = pmove->origin[2] + heightover2;
		cont = pmove->PM_PointContents(point, NULL);
		// If that point is also under water...
		if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
		{
			// Set a higher water level.
			pmove->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = pmove->origin[2] + pmove->view_ofs[2];

			cont = pmove->PM_PointContents(point, NULL);
			if (cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT)
				pmove->waterlevel = 3; // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if ((truecont <= CONTENTS_CURRENT_0) &&
			(truecont >= CONTENTS_CURRENT_DOWN))
		{
			// The deeper we are, the stronger the current.
			static Vector current_table[] =
				{
					{1, 0, 0}, {0, 1, 0}, {-1, 0, 0},
					{0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

			VectorMA(pmove->basevelocity, 50.0 * pmove->waterlevel, current_table[CONTENTS_CURRENT_0 - truecont], pmove->basevelocity);
		}
	}

	return pmove->waterlevel > 1;
}

/*
=============
PM_CatagorizePosition
=============
*/
void PM_CatagorizePosition()
{
	Vector point;
	pmtrace_t tr;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	PM_CheckWater();

	point[0] = pmove->origin[0];
	point[1] = pmove->origin[1];
	point[2] = pmove->origin[2] - 2;

	// BSVR updated to 90
	if (pmove->velocity[2] > 90) // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = -1;
	}
	// BSVR start
	else if ((g_onladder || pmove->movetype == MOVETYPE_FLY) && pmove->velocity[2] > 0.f)  // Moving up on ladder, not on ground
	{
		pmove->onground = -1;
	}
	// BSVR end
	else
	{
		// Try and move down.
		tr = pmove->PM_PlayerTrace(pmove->origin, point, PM_NORMAL, -1);
		// If we hit a steep plane, we are not on ground
		if (tr.plane.normal[2] < 0.7)
			pmove->onground = -1; // too steep
		else
			pmove->onground = tr.ent; // Otherwise, point to index of ent under us.

		// If we are on something...
		if (pmove->onground != -1)
		{
			// Then we are not in water jump sequence
			pmove->waterjumptime = 0;
			// BSVR start
			// This breaks a lot of initial upwards movement in 90fps, so we don't do that anymore. - Max Makes Mods, 2019-07-29
			// If we could make the move, drop us down that 1 pixel
			// if (pmove->waterlevel < 2 && 0 == tr.startsolid && 0 == tr.allsolid)
			// 	VectorCopy(tr.endpos, pmove->origin);
			// BSVR end
		}

		// Standing on an entity other than the world
		if (tr.ent > 0) // So signal that we are touching something.
		{
			PM_AddToTouched(tr, pmove->velocity);
		}
	}
}

/*
=================
PM_GetRandomStuckOffsets

When a player is stuck, it's costly to try and unstick them
Grab a test offset for the player based on a passed in index
=================
*/
int PM_GetRandomStuckOffsets(int nIndex, int server, Vector& offset)
{
	// Last time we did a full
	int idx;
	idx = rgStuckLast[nIndex][server]++;

	VectorCopy(rgv3tStuckTable[idx % 54], offset);

	return (idx % 54);
}

void PM_ResetStuckOffsets(int nIndex, int server)
{
	rgStuckLast[nIndex][server] = 0;
}

/*
=================
NudgePosition

If pmove->origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
#define PM_CHECKSTUCK_MINTIME 0.05 // Don't check again too quickly.

bool PM_TryToUnstuck(Vector base)
{
	float x, y, z;
	float xystep = 8.0;
	float zstep = 18.0;
	float xyminmax = xystep;
	float zminmax = 4 * zstep;
	Vector test;

	for (z = 0; z <= zminmax; z += zstep)
	{
		for (x = -xyminmax; x <= xyminmax; x += xystep)
		{
			for (y = -xyminmax; y <= xyminmax; y += xystep)
			{
				test = base;
				test[0] += x;
				test[1] += y;
				test[2] += z;

				if (pmove->PM_TestPlayerPosition(test, NULL) == -1)
				{
					VectorCopy(test, pmove->origin);
					return false;
				}
			}
		}
	}

	return true;
}

// BSVR start
#define PM_CHECKSTUCK_MINTIME 0.05  // Don't check again too quickly.
// BSVR end

bool PM_CheckStuck()
{
	Vector base;
	Vector offset;
	Vector test;
	int hitent;
	int i;
	pmtrace_t traceresult;
	// BSVR start
	int idx = 0;
	float fTime = 0.f;
	// BSVR end

	static float rgStuckCheckTime[MAX_PLAYERS][2]; // Last time we did a full

	// If position is okay, exit
	hitent = pmove->PM_TestPlayerPosition(pmove->origin, &traceresult);
	if (hitent != 0)  // Only get stuck on world, not on entities - Max Makes Mods, 2018-02-11 - BSVR - was == -1
	{
		PM_ResetStuckOffsets(pmove->player_index, pmove->server);
		return false;
	}

	VectorCopy(pmove->origin, base);

	//
	// Deal with precision error in network and cases where the player can get stuck on level transitions in singleplayer.
	//
	if (0 == pmove->server || 0 == pmove->multiplayer)
	{
		// World or BSP model
		if ((hitent == 0) ||
			(pmove->physents[hitent].model != NULL))
		{
			int nReps = 0;
			PM_ResetStuckOffsets(pmove->player_index, pmove->server);
			do
			{
				i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

				VectorAdd(base, offset, test);
				if (pmove->PM_TestPlayerPosition(test, &traceresult) == -1)
				{
					PM_ResetStuckOffsets(pmove->player_index, pmove->server);

					VectorCopy(test, pmove->origin);
					return false;
				}
				nReps++;
			} while (nReps < 54);
		}
	}

	// Always check if we've just changed levels.
	if (!(pmove->server != 0 && g_CheckForPlayerStuck))
	{
		// TODO: not really necessary to have separate arrays for client and server since the code is separate anyway.
		// BSVR start
		idx = 0 != pmove->server ? 0 : 1;
		// BSVR end

		const float fTime = pmove->Sys_FloatTime();
		// Too soon?
		if (rgStuckCheckTime[pmove->player_index][idx] >=
			(fTime - PM_CHECKSTUCK_MINTIME))
		{
			return true;
		}
		rgStuckCheckTime[pmove->player_index][idx] = fTime;
	}

	// BSVR start
	fTime = pmove->Sys_FloatTime();
	// Too soon?
	if (rgStuckCheckTime[pmove->player_index][idx] >=
		(fTime - PM_CHECKSTUCK_MINTIME))
	{
		return 1;
	}
	rgStuckCheckTime[pmove->player_index][idx] = fTime;
	// BSVR end

	pmove->PM_StuckTouch(hitent, &traceresult);

	i = PM_GetRandomStuckOffsets(pmove->player_index, pmove->server, offset);

	VectorAdd(base, offset, test);
	if ((hitent = pmove->PM_TestPlayerPosition(test, NULL)) == -1)
	{
		//Con_DPrintf("Nudged\n");

		PM_ResetStuckOffsets(pmove->player_index, pmove->server);

		if (i >= 27)
			VectorCopy(test, pmove->origin);

		return false;
	}

	// Try to unstuck the player after a level change.
	// This only works in singleplayer. In multiplayer there it's too unreliable to try, so only the first player gets unstuck.
	if (pmove->server != 0 && g_CheckForPlayerStuck)
	{
		g_CheckForPlayerStuck = false;

		// Are we stuck inside the world?
		if (hitent == 0)
		{
			if (!PM_TryToUnstuck(base))
			{
				return false;
			}
		}
	}

	// If player is flailing while stuck in another player ( should never happen ), then see
	//  if we can't "unstick" them forceably.
	if ((pmove->cmd.buttons & (IN_JUMP | IN_DUCK | IN_ATTACK)) != 0 && (pmove->physents[hitent].player != 0))
	{
		if (!PM_TryToUnstuck(base))
		{
			return false;
		}
	}

	//VectorCopy (base, pmove->origin);

	return true;
}

/*
===============
PM_SpectatorMove
===============
*/
void PM_SpectatorMove()
{
	float speed, drop, friction, control, newspeed;
	//float   accel;
	float currentspeed, addspeed, accelspeed;
	int i;
	Vector wishvel;
	float fmove, smove;
	Vector wishdir;
	float wishspeed;
	// this routine keeps track of the spectators psoition
	// there a two different main move types : track player or moce freely (OBS_ROAMING)
	// doesn't need excate track position, only to generate PVS, so just copy
	// targets position and real view position is calculated on client (saves server CPU)

	if (pmove->iuser1 == OBS_ROAMING)
	{

#ifdef CLIENT_DLL
		// jump only in roaming mode
		if (iJumpSpectator)
		{
			VectorCopy(vJumpOrigin, pmove->origin);
			VectorCopy(vJumpAngles, pmove->angles);
			VectorCopy(vec3_origin, pmove->velocity);
			iJumpSpectator = false;
			return;
		}
#endif
		// Move around in normal spectator method

		speed = Length(pmove->velocity);
		if (speed < 1)
		{
			VectorCopy(vec3_origin, pmove->velocity)
		}
		else
		{
			drop = 0;

			friction = pmove->movevars->friction * 1.5; // extra friction
			control = speed < pmove->movevars->stopspeed ? pmove->movevars->stopspeed : speed;
			drop += control * friction * pmove->frametime;

			// scale the velocity
			newspeed = speed - drop;
			if (newspeed < 0)
				newspeed = 0;
			newspeed /= speed;

			VectorScale(pmove->velocity, newspeed, pmove->velocity);
		}

		// accelerate
		fmove = pmove->cmd.forwardmove;
		smove = pmove->cmd.sidemove;

		VectorNormalize(pmove->forward);
		VectorNormalize(pmove->right);

		for (i = 0; i < 3; i++)
		{
			wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
		}
		wishvel[2] += pmove->cmd.upmove;

		VectorCopy(wishvel, wishdir);
		wishspeed = VectorNormalize(wishdir);

		//
		// clamp to server defined max speed
		//
		if (wishspeed > pmove->movevars->spectatormaxspeed)
		{
			VectorScale(wishvel, pmove->movevars->spectatormaxspeed / wishspeed, wishvel);
			wishspeed = pmove->movevars->spectatormaxspeed;
		}

		currentspeed = DotProduct(pmove->velocity, wishdir);
		addspeed = wishspeed - currentspeed;
		if (addspeed <= 0)
			return;

		accelspeed = pmove->movevars->accelerate * pmove->frametime * wishspeed;
		if (accelspeed > addspeed)
			accelspeed = addspeed;

		for (i = 0; i < 3; i++)
			pmove->velocity[i] += accelspeed * wishdir[i];

		// move
		VectorMA(pmove->origin, pmove->frametime, pmove->velocity, pmove->origin);
	}
	else
	{
		// all other modes just track some kind of target, so spectator PVS = target PVS

		int target;

		// no valid target ?
		if (pmove->iuser2 <= 0)
			return;

		// Find the client this player's targeting
		for (target = 0; target < pmove->numphysent; target++)
		{
			if (pmove->physents[target].info == pmove->iuser2)
				break;
		}

		if (target == pmove->numphysent)
			return;

		// use targets position as own origin for PVS
		VectorCopy(pmove->physents[target].angles, pmove->angles);
		VectorCopy(pmove->physents[target].origin, pmove->origin);

		// no velocity
		VectorCopy(vec3_origin, pmove->velocity);
	}
}

/*
==================
PM_SplineFraction

Use for ease-in, ease-out style interpolation (accel/decel)
Used by ducking code.
==================
*/
float PM_SplineFraction(float value, float scale)
{
	float valueSquared;

	value = scale * value;
	valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

void PM_FixPlayerCrouchStuck(int direction)
{
	int hitent;
	int i;
	Vector test;

	hitent = pmove->PM_TestPlayerPosition(pmove->origin, NULL);
	if (hitent == -1)
		return;

	VectorCopy(pmove->origin, test);
	for (i = 0; i < 36; i++)
	{
		pmove->origin[2] += direction;
		hitent = pmove->PM_TestPlayerPosition(pmove->origin, NULL);
		if (hitent == -1)
			return;
	}

	VectorCopy(test, pmove->origin); // Failed
}

void PM_UnDuck()
{
	int i;
	pmtrace_t trace;
	Vector newOrigin;

	VectorCopy(pmove->origin, newOrigin);

	if (pmove->onground != -1)
	{
		for (i = 0; i < 3; i++)
		{
			newOrigin[i] += (pmove->player_mins[1][i] - pmove->player_mins[0][i]);
		}
	}

	trace = pmove->PM_PlayerTrace(newOrigin, newOrigin, PM_NORMAL, -1);

	if (0 == trace.startsolid)
	{
		pmove->usehull = 0;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		trace = pmove->PM_PlayerTrace(newOrigin, newOrigin, PM_NORMAL, -1);
		if (0 != trace.startsolid)
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			//Con_Printf( "unstick got stuck\n" );
			pmove->usehull = 1;
			return;
		}

		pmove->flags &= ~FL_DUCKING;
		pmove->bInDuck = 0;
		pmove->view_ofs = VEC_VIEW;
		pmove->flDuckTime = 0;

		VectorCopy(newOrigin, pmove->origin);

		// Recatagorize position since ducking can change origin
		PM_CatagorizePosition();
	}
}

void PM_Duck()
{
	int i;
	float time;
	float duckFraction;

	int buttonsChanged = (pmove->oldbuttons ^ pmove->cmd.buttons); // These buttons have changed this frame
	int nButtonPressed = buttonsChanged & pmove->cmd.buttons;	   // The changed ones still down are "pressed"

	bool duckchange = (buttonsChanged & IN_DUCK) != 0;
	bool duckpressed = (nButtonPressed & IN_DUCK) != 0;

	if ((pmove->cmd.buttons & IN_DUCK) != 0)
	{
		pmove->oldbuttons |= IN_DUCK;
	}
	else
	{
		pmove->oldbuttons &= ~IN_DUCK;
	}

	// Prevent ducking if the iuser3 variable is set
	if (0 != pmove->iuser3 || 0 != pmove->dead)
	{
		// Try to unduck
		if ((pmove->flags & FL_DUCKING) != 0)
		{
			PM_UnDuck();
		}
		return;
	}

	if ((pmove->flags & FL_DUCKING) != 0)
	{
		pmove->cmd.forwardmove *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.sidemove *= PLAYER_DUCKING_MULTIPLIER;
		pmove->cmd.upmove *= PLAYER_DUCKING_MULTIPLIER;
	}

	if ((pmove->cmd.buttons & IN_DUCK) != 0 || (pmove->cmd.buttons_ex & X_IN_VRDUCK) != 0 || (0 != pmove->bInDuck) || (pmove->flags & FL_DUCKING) != 0) // BSVR: X_IN_VRDUCK added (physical crouch in VR)
	{
		if ((pmove->cmd.buttons & IN_DUCK) != 0 || (pmove->cmd.buttons_ex & X_IN_VRDUCK) != 0) // BSVR: X_IN_VRDUCK added
		{
			if ((nButtonPressed & IN_DUCK) != 0 && (pmove->flags & FL_DUCKING) == 0)
			{
				// Use 1 second so super long jump will work
				pmove->flDuckTime = 1000;
				pmove->bInDuck = 1;
			}

			time = V_max(0.0, (1.0 - (float)pmove->flDuckTime / 1000.0));

			if (0 != pmove->bInDuck || ((pmove->cmd.buttons_ex & X_IN_VRDUCK) != 0 && (pmove->flags & FL_DUCKING) == 0)) // BSVR: VR crouch ducks immediately (as in Half-Life-VR)
			{
				// Finish ducking immediately if duck time is over or not on ground
				if (((float)pmove->flDuckTime / 1000.0 <= (1.0 - TIME_TO_DUCK)) ||
					(pmove->onground == -1) || ((pmove->cmd.buttons_ex & X_IN_VRDUCK) != 0 && (pmove->flags & FL_DUCKING) == 0)) // BSVR: VR crouch
				{
					pmove->usehull = 1;
					// pmove->view_ofs = VEC_DUCK_VIEW; // BSVR: view_ofs is driven by the HMD (UpdateVRHeadset), removed as in Half-Life-VR
					pmove->flags |= FL_DUCKING;
					pmove->bInDuck = 0;

					// HACKHACK - Fudge for collision bug - no time to fix this properly
					if (pmove->onground != -1)
					{
						for (i = 0; i < 3; i++)
						{
							pmove->origin[i] -= (pmove->player_mins[1][i] - pmove->player_mins[0][i]);
						}
						// See if we are stuck?
						PM_FixPlayerCrouchStuck(STUCK_MOVEUP);

						// Recatagorize position since ducking can change origin
						PM_CatagorizePosition();
					}
				}
				else
				{
					float fMore = (VEC_DUCK_HULL_MIN[2] - VEC_HULL_MIN[2]);

					// Calc parametric time
					duckFraction = PM_SplineFraction(time, (1.0 / TIME_TO_DUCK));
					// pmove->view_ofs[2] = ((VEC_DUCK_VIEW[2] - fMore) * duckFraction) + (VEC_VIEW[2] * (1 - duckFraction)); // BSVR: view_ofs is driven by the HMD, removed as in Half-Life-VR
				}
			}
		}
		else
		{
			// Try to unduck
			PM_UnDuck();
		}
	}
}

void PM_LadderMove(physent_t* pLadder)
{
	Vector ladderCenter;
	trace_t trace;
	Vector floor;
	Vector modelmins, modelmaxs;

	if (pmove->movetype == MOVETYPE_NOCLIP)
		return;


	pmove->PM_GetModelBounds(pLadder->model, modelmins, modelmaxs);

	VectorAdd(modelmins, modelmaxs, ladderCenter);
	VectorScale(ladderCenter, 0.5, ladderCenter);

	pmove->movetype = MOVETYPE_FLY;

	// On ladder, convert movement to be relative to the ladder

	VectorCopy(pmove->origin, floor);
	floor[2] += pmove->player_mins[pmove->usehull][2] - 1;

	const bool onFloor = pmove->PM_PointContents(floor, NULL) == CONTENTS_SOLID;

	pmove->gravity = 0;
	PM_TraceModel(pLadder, pmove->origin, ladderCenter, &trace);
	if (trace.fraction != 1.0)
	{
		float forward = 0, right = 0;
		Vector vpn, v_right;
		float flSpeed = MAX_CLIMB_SPEED;

		// they shouldn't be able to move faster than their maxspeed
		if (flSpeed > pmove->maxspeed)
		{
			flSpeed = pmove->maxspeed;
		}

		AngleVectors(pmove->angles, &vpn, &v_right, NULL);

		if ((pmove->flags & FL_DUCKING) != 0)
		{
			flSpeed *= PLAYER_DUCKING_MULTIPLIER;
		}

		if ((pmove->cmd.buttons & IN_BACK) != 0)
		{
			forward -= flSpeed;
		}
		if ((pmove->cmd.buttons & IN_FORWARD) != 0)
		{
			forward += flSpeed;
		}
		if ((pmove->cmd.buttons & IN_MOVELEFT) != 0)
		{
			right -= flSpeed;
		}
		if ((pmove->cmd.buttons & IN_MOVERIGHT) != 0)
		{
			right += flSpeed;
		}

		if ((pmove->cmd.buttons & IN_JUMP) != 0)
		{
			pmove->movetype = MOVETYPE_WALK;
			VectorScale(trace.plane.normal, 270, pmove->velocity);
		}
		else
		{
			if (forward != 0 || right != 0)
			{
				Vector velocity, perp, cross, lateral, tmp;
				float normal;

				//ALERT(at_console, "pev %.2f %.2f %.2f - ",
				//	pev->velocity.x, pev->velocity.y, pev->velocity.z);
				// Calculate player's intended velocity
				//Vector velocity = (forward * gpGlobals->v_forward) + (right * gpGlobals->v_right);
				VectorScale(vpn, forward, velocity);
				VectorMA(velocity, right, v_right, velocity);


				// Perpendicular in the ladder plane
				//					Vector perp = CrossProduct( Vector(0,0,1), trace.vecPlaneNormal );
				//					perp = perp.Normalize();
				VectorClear(tmp);
				tmp[2] = 1;
				CrossProduct(tmp, trace.plane.normal, perp);
				VectorNormalize(perp);


				// decompose velocity into ladder plane
				normal = DotProduct(velocity, trace.plane.normal);
				// This is the velocity into the face of the ladder
				VectorScale(trace.plane.normal, normal, cross);


				// This is the player's additional velocity
				VectorSubtract(velocity, cross, lateral);

				// This turns the velocity into the face of the ladder into velocity that
				// is roughly vertically perpendicular to the face of the ladder.
				// NOTE: It IS possible to face up and move down or face down and move up
				// because the velocity is a sum of the directional velocity and the converted
				// velocity through the face of the ladder -- by design.
				CrossProduct(trace.plane.normal, perp, tmp);
				VectorMA(lateral, -normal, tmp, pmove->velocity);
				if (onFloor && normal > 0) // On ground moving away from the ladder
				{
					VectorMA(pmove->velocity, MAX_CLIMB_SPEED, trace.plane.normal, pmove->velocity);
				}
				//pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
			}
			else
			{
				VectorClear(pmove->velocity);
			}
		}
	}
}

// BSVR start
physent_t* g_pLastLadder = nullptr;
bool g_didPressLetGoOffLadder = false;

physent_t* PM_Ladder2(void)
{
	if (VRGlobalGetNoclipMode())
		return nullptr;

	int grabbedLaderIndex = VRGetGrabbedLadder(pmove->player_index);

	for (int i = 0; i < pmove->nummoveent; i++)
	{
		physent_t* pe = &pmove->moveents[i];

		if (pe->model && (modtype_t)pmove->PM_GetModelType(pe->model) == mod_brush && pe->skin == CONTENTS_LADDER)
		{
			if (grabbedLaderIndex > 0)
			{
				if (grabbedLaderIndex == pe->info)
				{
					return pe;
				}
			}
			else if (VRGetLadderMode() == VR_LADDER_MODE_LEGACY_ONLY)
			{
				Vector test;
				hull_t* hull = static_cast<hull_t*>(pmove->PM_HullForBsp(pe, test));

				// Offset the test point appropriately for this hull.
				// test = origin - test
				VectorSubtract(pmove->origin, test, test);

				// Test the player's hull for intersection with this model
				if (pmove->PM_HullPointContents(hull, hull->firstclipnode, test) == CONTENTS_EMPTY)
				{
					if (g_pLastLadder == pe)
					{
						// Even the slightest head movements make the player fall off a ladder,
						// so here we will do an additional check if the player is "close enough" to stay on the ladder.

						// Get ladder center in world space
						Vector modelmins{ 0.f, 0.f, 0.f };
						Vector modelmaxs{ 0.f, 0.f, 0.f };
						pmove->PM_GetModelBounds(pe->model, modelmins, modelmaxs);
						Vector ladderCenter{ 0.f, 0.f, 0.f };
						VectorAdd(modelmins, modelmaxs, ladderCenter);
						VectorScale(ladderCenter, 0.5, ladderCenter);
						VectorAdd(ladderCenter, pe->origin, ladderCenter);

						// Get 8 unit direction from player to ladder
						Vector dir;
						VectorSubtract(ladderCenter, pmove->origin, dir);
						VectorNormalize(dir);
						VectorScale(dir, 8.f, dir);

						// Add dir onto the test point and check again
						Vector test2;
						VectorAdd(test, dir, test2);
						if (pmove->PM_HullPointContents(hull, hull->firstclipnode, test2) != CONTENTS_EMPTY)
						{
							return pe;
						}

						// Subtract dir from the test point and check again
						VectorAdd(test, dir, test2);
						if (pmove->PM_HullPointContents(hull, hull->firstclipnode, test2) != CONTENTS_EMPTY)
						{
							return pe;
						}
					}
					continue;
				}
				else
				{
					g_pLastLadder = pe;
					return pe;
				}
			}
		}
	}

	g_pLastLadder = nullptr;
	return nullptr;
}
// BSVR end

physent_t* PM_Ladder()
{
	// BSVR start
	physent_t* lastladder = g_pLastLadder;
	physent_t* ladder = PM_Ladder2();

	if (ladder == nullptr)
	{
		g_didPressLetGoOffLadder = false;
		return nullptr;
	}

	if (ladder == lastladder && g_didPressLetGoOffLadder)
		return nullptr;

	return ladder;
	// int i;
	// physent_t* pe;
	// hull_t* hull;
	// int num;
	// Vector test;

	// for (i = 0; i < pmove->nummoveent; i++)
	// {
	// 	pe = &pmove->moveents[i];

	// 	if (pe->model && (modtype_t)pmove->PM_GetModelType(pe->model) == mod_brush && pe->skin == CONTENTS_LADDER)
	// 	{

	// 		hull = (hull_t*)pmove->PM_HullForBsp(pe, test);
	// 		num = hull->firstclipnode;

	// 		// Offset the test point appropriately for this hull.
	// 		VectorSubtract(pmove->origin, test, test);

	// 		// Test the player's hull for intersection with this model
	// 		if (pmove->PM_HullPointContents(hull, num, test) == CONTENTS_EMPTY)
	// 			continue;

	// 		return pe;
	// 	}
	// }

	// return NULL;
	// BSVR end
}



void PM_WaterJump()
{
	if (pmove->waterjumptime > 10000)
	{
		pmove->waterjumptime = 10000;
	}

	if (0 == pmove->waterjumptime)
		return;

	pmove->waterjumptime -= pmove->cmd.msec;
	if (pmove->waterjumptime < 0 ||
		0 == pmove->waterlevel)
	{
		pmove->waterjumptime = 0;
		pmove->flags &= ~FL_WATERJUMP;
	}

	pmove->velocity[0] = pmove->movedir[0];
	pmove->velocity[1] = pmove->movedir[1];
}

/*
============
PM_AddGravity

============
*/
void PM_AddGravity()
{
	float ent_gravity;

	if (0 != pmove->gravity)
		ent_gravity = pmove->gravity;
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	pmove->velocity[2] -= (ent_gravity * pmove->movevars->gravity * pmove->frametime);
	pmove->velocity[2] += pmove->basevelocity[2] * pmove->frametime;
	pmove->basevelocity[2] = 0;
	PM_CheckVelocity();
}
/*
============
PM_PushEntity

Does not change the entities velocity at all
============
*/
pmtrace_t PM_PushEntity(Vector push)
{
	pmtrace_t trace;
	Vector end;

	VectorAdd(pmove->origin, push, end);

	trace = pmove->PM_PlayerTrace(pmove->origin, end, PM_NORMAL, -1);

	VectorCopy(trace.endpos, pmove->origin);

	// So we can run impact function afterwards.
	if (trace.fraction < 1.0 &&
		0 == trace.allsolid)
	{
		PM_AddToTouched(trace, pmove->velocity);
	}

	return trace;
}

/*
============
PM_Physics_Toss()

Dead player flying through air., e.g.
============
*/
void PM_Physics_Toss()
{
	pmtrace_t trace;
	Vector move;
	float backoff;

	PM_CheckWater();

	if (pmove->velocity[2] > 0)
		pmove->onground = -1;

	// If on ground and not moving, return.
	if (pmove->onground != -1)
	{
		if (VectorCompare(pmove->basevelocity, vec3_origin) &&
			VectorCompare(pmove->velocity, vec3_origin))
			return;
	}

	PM_CheckVelocity();

	// add gravity
	if (pmove->movetype != MOVETYPE_FLY &&
		pmove->movetype != MOVETYPE_BOUNCEMISSILE &&
		pmove->movetype != MOVETYPE_FLYMISSILE &&
		!(pmove->flags & FL_BARNACLED))
		PM_AddGravity();

	// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);

	PM_CheckVelocity();
	VectorScale(pmove->velocity, pmove->frametime, move);
	VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

	trace = PM_PushEntity(move); // Should this clear basevelocity

	PM_CheckVelocity();

	if (0 != trace.allsolid)
	{
		// entity is trapped in another solid
		pmove->onground = trace.ent;
		VectorCopy(vec3_origin, pmove->velocity);
		return;
	}

	if (trace.fraction == 1)
	{
		PM_CheckWater();
		return;
	}


	if (pmove->movetype == MOVETYPE_BOUNCE)
		backoff = 2.0 - pmove->friction;
	else if (pmove->movetype == MOVETYPE_BOUNCEMISSILE)
		backoff = 2.0;
	else
		backoff = 1;

	PM_ClipVelocity(pmove->velocity, trace.plane.normal, pmove->velocity, backoff);

	// stop if on ground
	if (trace.plane.normal[2] > 0.7)
	{
		float vel;
		Vector base;

		VectorClear(base);
		if (pmove->velocity[2] < pmove->movevars->gravity * pmove->frametime)
		{
			// we're rolling on the ground, add static friction.
			pmove->onground = trace.ent;
			pmove->velocity[2] = 0;
		}

		vel = DotProduct(pmove->velocity, pmove->velocity);

		// Con_DPrintf("%f %f: %.0f %.0f %.0f\n", vel, trace.fraction, ent->velocity[0], ent->velocity[1], ent->velocity[2] );

		if (vel < (30 * 30) || (pmove->movetype != MOVETYPE_BOUNCE && pmove->movetype != MOVETYPE_BOUNCEMISSILE))
		{
			pmove->onground = trace.ent;
			VectorCopy(vec3_origin, pmove->velocity);
		}
		else
		{
			VectorScale(pmove->velocity, (1.0 - trace.fraction) * pmove->frametime * 0.9, move);
			trace = PM_PushEntity(move);
		}
		VectorSubtract(pmove->velocity, base, pmove->velocity)
	}

	// check for in water
	PM_CheckWater();
}

/*
====================
PM_NoClip

====================
*/
void PM_NoClip()
{
	int i;
	Vector wishvel;
	float fmove, smove;
	//	float		currentspeed, addspeed, accelspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;

	VectorNormalize(pmove->forward);
	VectorNormalize(pmove->right);

	for (i = 0; i < 3; i++) // Determine x and y parts of velocity
	{
		wishvel[i] = pmove->forward[i] * fmove + pmove->right[i] * smove;
	}
	wishvel[2] += pmove->cmd.upmove;

	VectorMA(pmove->origin, pmove->frametime, wishvel, pmove->origin);

	// Zero out the velocity so that we don't accumulate a huge downward velocity from
	//  gravity, etc.
	VectorClear(pmove->velocity);
}

bool PM_TryUnstuck()
{
	// Trying to determine if a player can move while being stuck (move away from wall) - Max Makes Mods, 2018-04-01
	// Updated and improved - Max Makes Mods, 2020-03-01

	// the maximum distance we try to move a stuck player
	constexpr const float VR_UNSTUCK_DISTANCE = 32.f;

	// Don't unstuck dead players
	if (pmove->dead || pmove->deadflag != DEAD_NO)
		return false;

	// we aren't stuck (possible retry ducking)
	if (pmove->PM_TestPlayerPosition(pmove->origin, nullptr) < 0)
		return true;

	// check if it's just a step
	float oldz = pmove->origin[2];
	pmove->origin[2] += pmove->movevars->stepsize;
	if (pmove->PM_TestPlayerPosition(pmove->origin, nullptr) < 0)
		return true;

	// bruteforce our way out of this!
	pmove->origin[2] = oldz;
	int stuckent = pmove->PM_TestPlayerPosition(pmove->origin, nullptr);
	bool foundend = false;
	bool foundendinsideent = false;
	Vector foundendoffset;
	for (int i = 0; !foundend && i <= VR_UNSTUCK_DISTANCE; i++)
	{
		for (int x = -1; x <= 1; x++)
		{
			for (int y = -1; y <= 1; y++)
			{
				// go positive in z direction first (unstucking upwards is always better than downwards)
				for (int z = 1; z >= -1; z--)
				{
					Vector testend;
					Vector offset;
					offset[0] = x * i;
					offset[1] = y * i;
					offset[2] = z * i;
					VectorAdd(pmove->origin, offset, testend);
					if (pmove->PM_TestPlayerPosition(testend, nullptr) == -1)
					{
						if (stuckent > 0 && VRGlobalIsPointInsideEnt(testend, pmove->physents[stuckent].info) && (!foundendinsideent || Length(offset) < Length(foundendoffset)))
						{
							VectorCopy(offset, foundendoffset);
							foundendinsideent = true;
						}
						else if (!foundendinsideent && (!foundend || Length(offset) < Length(foundendoffset)))
						{
							VectorCopy(offset, foundendoffset);
						}
						foundend = true;
					}
				}
			}
		}
	}

	if (foundend)
	{
		// successfully unstucked
		VectorAdd(pmove->origin, foundendoffset, pmove->origin);
		return true;
	}

	// still stuck and already ducking, can't unstuck :(
	if ((pmove->flags & FL_DUCKING) || pmove->usehull == 1)
		return false;

	// try ducking
	pmove->flags |= FL_DUCKING;
	pmove->usehull = 1;
	for (int i = 0; i < 3; i++)
	{
		pmove->origin[i] -= (pmove->player_mins[1][i] - pmove->player_mins[0][i]);
	}

	// try again ducking
	return PM_TryUnstuck();
}

// Only allow bunny jumping up to 1.7x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.7f

//-----------------------------------------------------------------------------
// Purpose: Corrects bunny jumping ( where player initiates a bunny jump before other
//  movement logic runs, thus making onground == -1 thus making PM_Friction get skipped and
//  running PM_AirMove, which doesn't crop velocity to maxspeed like the ground / other
//  movement logic does.
//-----------------------------------------------------------------------------
void PM_PreventMegaBunnyJumping()
{
	const bool allowBunnyHopping = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "bj")) == 1;

	if (allowBunnyHopping)
	{
		return;
	}

	// Current player speed
	float spd;
	// If we have to crop, apply this cropping fraction to velocity
	float fraction;
	// Speed at which bunny jumping is limited
	float maxscaledspeed;

	maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * pmove->maxspeed;

	// Don't divide by zero
	if (maxscaledspeed <= 0.0f)
		return;

	spd = Length(pmove->velocity);

	if (spd <= maxscaledspeed)
		return;

	fraction = (maxscaledspeed / spd) * 0.65; //Returns the modifier for the velocity

	VectorScale(pmove->velocity, fraction, pmove->velocity); //Crop it down!.
}

/*
=============
PM_Jump
=============
*/
void PM_Jump()
{
	int i;

	if (0 != pmove->dead)
	{
		pmove->oldbuttons |= IN_JUMP; // don't jump again until released
		return;
	}

	const bool tfc = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "tfc")) == 1;

	// Spy that's feigning death cannot jump
	if (tfc &&
		(pmove->deadflag == (DEAD_DISCARDBODY + 1)))
	{
		return;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (0 != pmove->waterjumptime)
	{
		pmove->waterjumptime -= pmove->cmd.msec;
		if (pmove->waterjumptime < 0)
		{
			pmove->waterjumptime = 0;
		}
		return;
	}

	// If we are in the water most of the way...
	if (pmove->waterlevel >= 2 || VRIsInUpwardsTriggerPush(pmove->player_index)) // BSVR added condition
	{ // swimming, not jumping
		pmove->onground = -1;

		if (pmove->watertype == CONTENTS_WATER) // We move up a certain amount
			pmove->velocity[2] = 100;
		else if (pmove->watertype == CONTENTS_SLIME)
			pmove->velocity[2] = 80;
		else // LAVA
			pmove->velocity[2] = 50;

		// play swiming sound
		if (pmove->flSwimTime <= 0 && !VRIsInUpwardsTriggerPush(pmove->player_index)) // BSVR added condition
		{
			// Don't play sound again for 1 second
			pmove->flSwimTime = 1000;
			switch (pmove->RandomLong(0, 3))
			{
			case 0:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 1:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 2:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			case 3:
				pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
				break;
			}
		}

		return;
	}

	// No more effect
	if (pmove->onground == -1)
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game .dll no longer does physics code!!!!
		pmove->oldbuttons |= IN_JUMP; // don't jump again until released
		return;						  // in air, so no effect
	}

	if ((pmove->oldbuttons & IN_JUMP) != 0)
		return; // don't pogo stick

	// In the air now.
	pmove->onground = -1;

	PM_PreventMegaBunnyJumping();

	// Don't play jump sounds while frozen.
	if ((pmove->flags & FL_FROZEN) == 0)
	{
		if (tfc)
		{
			pmove->PM_PlaySound(CHAN_BODY, "player/plyrjmp8.wav", 0.5, ATTN_NORM, 0, PITCH_NORM);
		}
		else
		{
			PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), 1.0);
		}
	}

	// See if user can super long jump?
	const bool cansuperjump = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "slj")) == 1;

	// Acclerate upward
	// If we are ducking...
	if ((0 != pmove->bInDuck) || (pmove->flags & FL_DUCKING) != 0)
	{
		// Adjust for super long jump module
		// UNDONE -- note this should be based on forward angles, not current velocity.
		if (cansuperjump &&
			(pmove->cmd.buttons & IN_DUCK) != 0 &&
			(pmove->flDuckTime > 0) &&
			Length(pmove->velocity) > 50)
		{
			pmove->punchangle[0] = -5;

			for (i = 0; i < 2; i++)
			{
				pmove->velocity[i] = pmove->forward[i] * PLAYER_LONGJUMP_SPEED * 1.6;
			}

			pmove->velocity[2] = sqrt(2 * 800 * 56.0);
		}
		else
		{
			pmove->velocity[2] = sqrt(2 * 800 * 45.0);
		}
	}
	else
	{
		pmove->velocity[2] = sqrt(2 * 800 * 45.0);
	}

	// Decay it for simulation
	PM_FixupGravityVelocity();

	// Flag that we jumped.
	pmove->oldbuttons |= IN_JUMP; // don't jump again until released
}

/*
=============
PM_CheckWaterJump
=============
*/
#define WJ_HEIGHT 8
void PM_CheckWaterJump()
{
	Vector vecStart, vecEnd;
	Vector flatforward;
	Vector flatvelocity;
	float curspeed;
	pmtrace_t tr;
	int savehull;

	// Already water jumping.
	if (0 != pmove->waterjumptime)
		return;

	// Don't hop out if we just jumped in
	if (pmove->velocity[2] < -180)
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity[0] = pmove->velocity[0];
	flatvelocity[1] = pmove->velocity[1];
	flatvelocity[2] = 0;

	// Must be moving
	curspeed = VectorNormalize(flatvelocity);

	// see if near an edge
	flatforward[0] = pmove->forward[0];
	flatforward[1] = pmove->forward[1];
	flatforward[2] = 0;
	VectorNormalize(flatforward);

	// Are we backing into water from steps or something?  If so, don't pop forward
	if (curspeed != 0.0 && (DotProduct(flatvelocity, flatforward) < 0.0))
		return;

	VectorCopy(pmove->origin, vecStart);
	vecStart[2] += WJ_HEIGHT;

	VectorMA(vecStart, 24, flatforward, vecEnd);

	// Trace, this trace should use the point sized collision hull
	savehull = pmove->usehull;
	pmove->usehull = 2;
	tr = pmove->PM_PlayerTrace(vecStart, vecEnd, PM_NORMAL, -1);
	if (tr.fraction < 1.0 && fabs(tr.plane.normal[2]) < 0.1f) // Facing a near vertical wall?
	{
		vecStart[2] += pmove->player_maxs[savehull][2] - WJ_HEIGHT;
		VectorMA(vecStart, 24, flatforward, vecEnd);
		VectorMA(vec3_origin, -50, tr.plane.normal, pmove->movedir);

		tr = pmove->PM_PlayerTrace(vecStart, vecEnd, PM_NORMAL, -1);
		if (tr.fraction == 1.0)
		{
			pmove->waterjumptime = 2000;
			pmove->velocity[2] = 225;
			pmove->oldbuttons |= IN_JUMP;
			pmove->flags |= FL_WATERJUMP;
		}
	}

	// Reset the collision hull
	pmove->usehull = savehull;
}

void PM_CheckFalling()
{
	if (pmove->onground != -1 &&
		0 == pmove->dead &&
		pmove->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD)
	{
		float fvol = 0.5;

		if (pmove->waterlevel > 0)
		{
		}
		else if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED)
		{
			// NOTE:  In the original game dll , there were no breaks after these cases, causing the first one to
			// cascade into the second
			//switch ( RandomLong(0,1) )
			//{
			//case 0:
			//pmove->PM_PlaySound( CHAN_VOICE, "player/pl_fallpain2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			//break;
			//case 1:
			pmove->PM_PlaySound(CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			//	break;
			//}
			fvol = 1.0;
		}
		else if (pmove->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2)
		{
			const bool tfc = atoi(pmove->PM_Info_ValueForKey(pmove->physinfo, "tfc")) == 1;

			if (tfc)
			{
				pmove->PM_PlaySound(CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			}

			fvol = 0.85;
		}
		else if (pmove->flFallVelocity < PLAYER_MIN_BOUNCE_SPEED)
		{
			fvol = 0;
		}

		if (fvol > 0.0)
		{
			// Play landing step right away
			pmove->flTimeStepSound = 0;

			PM_UpdateStepSound();

			// play step sound for current texture
			PM_PlayStepSound(PM_MapTextureTypeStepType(pmove->chtexturetype), fvol);

			// Knock the screen around a little bit, temporary effect
			pmove->punchangle[2] = pmove->flFallVelocity * 0.013; // punch z axis

			if (pmove->punchangle[0] > 8)
			{
				pmove->punchangle[0] = 8;
			}
		}
	}

	if (pmove->onground != -1)
	{
		pmove->flFallVelocity = 0;
	}
}

/*
=================
PM_PlayWaterSounds

=================
*/
void PM_PlayWaterSounds()
{
	// Did we enter or leave water?
	if ((pmove->oldwaterlevel == 0 && pmove->waterlevel != 0) ||
		(pmove->oldwaterlevel != 0 && pmove->waterlevel == 0))
	{
		switch (pmove->RandomLong(0, 3))
		{
		case 0:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 1:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 2:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		case 3:
			pmove->PM_PlaySound(CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, 0, PITCH_NORM);
			break;
		}
	}
}

/*
===============
PM_CalcRoll

===============
*/
float PM_CalcRoll(Vector angles, Vector velocity, float rollangle, float rollspeed)
{
	float sign;
	float side;
	float value;
	Vector forward, right, up;

	AngleVectors(angles, &forward, &right, &up);

	side = DotProduct(velocity, right);

	sign = side < 0 ? -1 : 1;

	side = fabs(side);

	value = rollangle;

	if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
	else
	{
		side = value;
	}

	return side * sign;
}

/*
=============
PM_DropPunchAngle

=============
*/
void PM_DropPunchAngle(Vector& punchangle)
{
	float len;

	len = VectorNormalize(punchangle);
	len -= (10.0 + len * 0.5) * pmove->frametime;
	len = V_max(len, 0.0f);
	VectorScale(punchangle, len, punchangle);
}

/*
==============
PM_CheckParamters

==============
*/
void PM_CheckParamters()
{
	float spd;
	float maxspeed;
	Vector v_angle;

	spd = (pmove->cmd.forwardmove * pmove->cmd.forwardmove) +
		  (pmove->cmd.sidemove * pmove->cmd.sidemove) +
		  (pmove->cmd.upmove * pmove->cmd.upmove);
	spd = sqrt(spd);

	maxspeed = pmove->clientmaxspeed; //atof( pmove->PM_Info_ValueForKey( pmove->physinfo, "maxspd" ) );
	if (maxspeed != 0.0)
	{
		pmove->maxspeed = V_min(maxspeed, pmove->maxspeed);
	}

	// Slow down, I'm pulling it! (a box maybe) but only when I'm standing on ground
	//
	// JoshA: Moved this to CheckParamters rather than working on the velocity,
	// as otherwise it affects every integration step incorrectly.
	if ((pmove->onground != -1) && (pmove->cmd.buttons & IN_USE))
	{
		pmove->maxspeed *= 1.0f / 3.0f;
	}

	if ((spd != 0.0) &&
		(spd > pmove->maxspeed))
	{
		float fRatio = pmove->maxspeed / spd;
		pmove->cmd.forwardmove *= fRatio;
		pmove->cmd.sidemove *= fRatio;
		pmove->cmd.upmove *= fRatio;
	}

	if ((pmove->flags & FL_FROZEN) != 0 ||
		(pmove->flags & FL_STUCK_ONTRAIN) != 0 ||  // BSVR: Renamed from FL_ONTRAIN
		0 != pmove->dead)
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove = 0;
		pmove->cmd.upmove = 0;
	}


	PM_DropPunchAngle(pmove->punchangle);

	// Take angles from command.
	if (0 == pmove->dead)
	{
		VectorCopy(pmove->cmd.viewangles, v_angle);
		VectorAdd(v_angle, pmove->punchangle, v_angle);

		// Set up view angles.
		pmove->angles[ROLL] = PM_CalcRoll(v_angle, pmove->velocity, pmove->movevars->rollangle, pmove->movevars->rollspeed) * 4;
		pmove->angles[PITCH] = v_angle[PITCH];
		pmove->angles[YAW] = v_angle[YAW];
	}
	else
	{
		VectorCopy(pmove->oldangles, pmove->angles);
	}

	// Set dead player view_offset
	if (0 != pmove->dead)
	{
		pmove->view_ofs = VEC_DEAD_VIEW;
	}

	// Adjust client view angles to match values used on server.
	if (pmove->angles[YAW] > 180.0f)
	{
		pmove->angles[YAW] -= 360.0f;
	}
}

void PM_ReduceTimers()
{
	if (pmove->flTimeStepSound > 0)
	{
		pmove->flTimeStepSound -= pmove->cmd.msec;
		if (pmove->flTimeStepSound < 0)
		{
			pmove->flTimeStepSound = 0;
		}
	}
	if (pmove->flDuckTime > 0)
	{
		pmove->flDuckTime -= pmove->cmd.msec;
		if (pmove->flDuckTime < 0)
		{
			pmove->flDuckTime = 0;
		}
	}
	if (pmove->flSwimTime > 0)
	{
		pmove->flSwimTime -= pmove->cmd.msec;
		if (pmove->flSwimTime < 0)
		{
			pmove->flSwimTime = 0;
		}
	}
}

/*
=============
PlayerMove

Returns with origin, angles, and velocity modified in place.

Numtouch and touchindex[] will be set if any of the physents
were contacted during the move.
=============
*/
// BSVR start
/*
====================
PM_YesClip
Moved from PM_Move() switch statement for MOVETYPE_WALK - Max Makes Mods, 2018-04-01
====================
*/
void PM_YesClip(physent_t* pLadder)
{
	// No gravity in water - Max Makes Mods, 2018-02-11
	if (IsInWaterAndIsGravityDisabled() || (pmove->flags & FL_BARNACLED) || VRIsInUpwardsTriggerPush(pmove->player_index))
	{
		pmove->velocity[2] = std::fmax(0.f, pmove->velocity[2]);
	}

	if (!PM_InWater() && !(pmove->flags & FL_BARNACLED) && !VRIsInUpwardsTriggerPush(pmove->player_index))
	{
		PM_AddCorrectGravity();
	}

	// If we are leaping out of the water, just update the counters.
	if (pmove->waterjumptime)
	{
		PM_WaterJump();
		PM_FlyMove();

		// Make sure waterlevel is set correctly
		PM_CheckWater();
		return;
	}

	// If we are swimming in the water, see if we are nudging against a place we can jump up out
	//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
	if (pmove->waterlevel > 1)
	{
		if (pmove->waterlevel == 2)
		{
			PM_CheckWaterJump();
		}

		// If we are falling again, then we must not trying to jump out of water any more.
		if (pmove->velocity[2] < 0 && pmove->waterjumptime)
		{
			pmove->waterjumptime = 0;
		}

		// Perform regular water movement
		PM_WaterMove();

		VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

		// Get a final position
		PM_CatagorizePosition();
	}
	else

		// Not underwater
	{
		// Was jump button pressed?
		if (pmove->cmd.buttons & IN_JUMP)
		{
			if (!pLadder)
			{
				PM_Jump();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor,
		//  we don't slow when standing still, relative to the conveyor.
		if (pmove->onground != -1 && !VRIsInUpwardsTriggerPush(pmove->player_index)) // BSVR: on ground (upstream IsOnGround()) and not in an upwards trigger_push
		{
			pmove->velocity[2] = 0.0;
			PM_Friction();
		}

		// Make sure velocity is valid.
		PM_CheckVelocity();

		// Are we on ground now
		if (pmove->onground != -1 && !VRIsInUpwardsTriggerPush(pmove->player_index)) // BSVR: on ground (upstream IsOnGround()) and not in an upwards trigger_push
		{
			PM_WalkMove();
		}
		else
		{
			PM_AirMove();  // Take into account movement when in air.
		}

		// Set final flags.
		PM_CatagorizePosition();

		// Now pull the base velocity back out.
		// Base velocity is set if you are on a moving object, like
		//  a conveyor (or maybe another monster?)
		VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);

		// Make sure velocity is valid.
		PM_CheckVelocity();

		// Add any remaining gravitational component.
		if (!PM_InWater() && !VRIsInUpwardsTriggerPush(pmove->player_index))
		{
			PM_FixupGravityVelocity();
		}

		// If we are on ground, no downward velocity.
		if (pmove->onground != -1 && !VRIsInUpwardsTriggerPush(pmove->player_index)) // BSVR: on ground (upstream IsOnGround()) and not in an upwards trigger_push
		{
			pmove->velocity[2] = 0;
		}

		// See if we landed on the ground with enough force to play
		//  a landing sound.
		if (!VRIsInUpwardsTriggerPush(pmove->player_index))
		{
			PM_CheckFalling();
		}
	}

	if (IsInWaterAndIsGravityDisabled() || (pmove->flags & FL_BARNACLED) || VRIsInUpwardsTriggerPush(pmove->player_index))
	{
		pmove->velocity[2] = std::fmax(0.f, pmove->velocity[2]);
	}

	// Did we enter or leave the water?
	PM_PlayWaterSounds();
}

void PM_HandleMovement(physent_t* pLadder)
{
	// Handle movement
	switch (pmove->movetype)
	{
	default:
		pmove->Con_DPrintf("Bogus pmove player movetype %i on (%i) 0=cl 1=sv\n", pmove->movetype, pmove->server);
		break;

	case MOVETYPE_NONE:
		break;

	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		PM_Physics_Toss();
		break;

	case MOVETYPE_FLY:

		PM_CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if (pmove->cmd.buttons & IN_JUMP)
		{
			if (!pLadder)
			{
				PM_Jump();
			}
		}
		else
		{
			pmove->oldbuttons &= ~IN_JUMP;
		}

		// Perform the move accounting for any base velocity.
		VectorAdd(pmove->velocity, pmove->basevelocity, pmove->velocity);
		PM_FlyMove();
		VectorSubtract(pmove->velocity, pmove->basevelocity, pmove->velocity);
		break;

	case MOVETYPE_NOCLIP:  // In VR player movetype is always NOCLIP so doors, elevators etc don't crush us
	case MOVETYPE_WALK:
		if (VRGlobalGetNoclipMode())
		{
			PM_NoClip();
		}
		else
		{
			PM_YesClip(pLadder);  // Moved all the walking code in this switch-statement to its own method
		}
		break;
	}
}
// BSVR end

void PM_PlayerMove(qboolean server)
{
	physent_t* pLadder = NULL;

	// Are we running server code?
	pmove->server = server;

	// Adjust speeds etc.
	PM_CheckParamters();

	// Assume we don't touch anything
	pmove->numtouch = 0;

	// # of msec to apply movement
	pmove->frametime = pmove->cmd.msec * 0.001;

	PM_ReduceTimers();

	// Convert view angles to vectors
	AngleVectors(pmove->angles, &pmove->forward, &pmove->right, &pmove->up);

	// PM_ShowClipBox();

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
	if (0 != pmove->spectator || pmove->iuser1 > 0)
	{
		PM_SpectatorMove();
		PM_CatagorizePosition();
		return;
	}

	// Always try and unstick us unless we are in NOCLIP mode
	// BSVR start
	// if (pmove->movetype != MOVETYPE_NOCLIP && pmove->movetype != MOVETYPE_NONE)
	// {
	// 	if (PM_CheckStuck())
	// 	{
	// 		// Let the user try to duck to get unstuck
	// 		PM_Duck();

	// 		if (PM_CheckStuck())
	// 		{
	// 			return; // Can't move, we're stuck
	// 		}
	// 	}
	// }
	if (!VRGlobalGetNoclipMode() && pmove->movetype != MOVETYPE_NONE)
	{
		bool dontUnstuck = false;
		int stuckent = pmove->PM_TestPlayerPosition(pmove->origin, nullptr);
		if (stuckent > 0)
		{
			// Tell server dll that we got stuck in an entity - server dll can then determine if this entity should kill us or not
			dontUnstuck = VRNotifyStuckEnt(pmove->player_index, pmove->physents[stuckent].info);
		}
		if (PM_CheckStuck() || stuckent >= 0)
		{
			// When we're stuck, move away if our move direction is going away from whatever we're stuck on
			// (Only unstuck us on server, to not interfere with dontUnstuck, which is only set by server)
			if ((server || pmove->server) && !dontUnstuck)
			{
				if (!PM_TryUnstuck())
				{
					return;
				}
			}
			else
			{
				return;
			}
		}
	}
	//BSVR end

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	PM_CatagorizePosition();

	// Store off the starting water level
	pmove->oldwaterlevel = pmove->waterlevel;

	// If we are not on ground, store off how fast we are moving down
	if (pmove->onground == -1)
	{
		pmove->flFallVelocity = -pmove->velocity[2];
	}

	// BSVR start
	// if set, PM_Ladder will ignore the ladder we currently touch (will be reset when no ladder is touched)
	g_didPressLetGoOffLadder = g_didPressLetGoOffLadder || (pmove->cmd.buttons_ex & X_IN_LETLADDERGO);
	// BSVR end

	g_onladder = false;
	// Don't run ladder code if dead or on a train
	if (0 == pmove->dead && (pmove->flags & FL_STUCK_ONTRAIN) == 0 && !pmoveIgnoreLadders) // BSVR: added pmoveIgnoreLadders check // BSVR: Renamed from FL_ONTRAIN
	{
		pLadder = PM_Ladder();
		if (pLadder)
		{
			g_onladder = true;
		}
	}

	PM_UpdateStepSound();

	PM_Duck();

	// Don't run ladder code if dead or on a train
	if (0 == pmove->dead && (pmove->flags & FL_STUCK_ONTRAIN) == 0) // BSVR: Renamed from FL_ONTRAIN
	{
		// BSVR start
		// Old code
		// if (pLadder)
		// {
		// 	PM_LadderMove(pLadder);
		// }
		// else if (pmove->movetype != MOVETYPE_WALK &&
		// 		 pmove->movetype != MOVETYPE_NOCLIP)
		// {
		// 	// Clear ladder stuff unless player is noclipping
		// 	//  it will be set immediately again next frame if necessary
		// 	pmove->movetype = MOVETYPE_WALK;
		// }
		// New code
		if (pLadder && !VRGlobalGetNoclipMode())
		{
			pmove->gravity = 0;
			pmove->movetype = MOVETYPE_FLY;
			PM_LadderMove(pLadder);
		}
		else
		{
			pmove->movetype = MOVETYPE_NOCLIP;
		}
		// BSVR end
	}

	// BSVR start
	// Slow down, I'm pulling it! (a box maybe) but only when I'm standing on ground
	if (pmove->onground != -1 && (pmove->cmd.buttons & IN_USE))
	{
		VectorScale(pmove->velocity, 0.3, pmove->velocity);
	}
	// Always calculate movement (on land) as if ducking, then afterwards unduck if possible or stay ducked if not possible (this essentially makes players autoduck if necessary)
	// In water we don't do this, as it messes with waterjump
	bool tryAutoDuck = VRIsAutoDuckingEnabled(pmove->player_index) && pmove->usehull == 0 && !(pmove->flags & FL_DUCKING) && !pLadder && pmove->waterlevel == 0;

	if (tryAutoDuck && pmove->onground != -1)
	{
		// same logic as in PM_CategorizePosition()
		Vector point;
		VectorCopy(pmove->origin, point);
		point[2] -= 2.f;
		auto tr = pmove->PM_PlayerTrace(pmove->origin, point, PM_NORMAL, -1);
		// don't auto crouch if surface isn't horizontal
		if (tr.fraction >= 1.f || tr.plane.normal[2] < 1.f)
		{
			tryAutoDuck = false;
		}
	}
	if (tryAutoDuck)
	{
		// Backup original origin and velocity (velocity gets changed to add in gravity!)
		Vector original_origin;
		Vector original_velocity;
		VectorCopy(pmove->origin, original_origin);
		VectorCopy(pmove->velocity, original_velocity);

		// Move standing up
		PM_HandleMovement(pLadder);

		// Backup standmove origin
		Vector standmove_origin;
		Vector standmove_velocity;
		VectorCopy(pmove->origin, standmove_origin);
		VectorCopy(pmove->velocity, standmove_velocity);

		// Restore original origin and velocity
		VectorCopy(original_origin, pmove->origin);
		VectorCopy(original_velocity, pmove->velocity);
		PM_CatagorizePosition();

		// Set crouching
		pmove->usehull = 1;
		pmove->flags |= FL_DUCKING;
		pmove->origin[2] = pmove->origin[2] + pmove->player_mins[0][2] - pmove->player_mins[1][2];

		// Make sure we stay onground when switching to crouching
		if (pmove->onground != -1) // upstream: IsOnGround()
		{
			PM_CatagorizePosition();
			int tries = 0;
			while (pmove->onground == -1 && tries < 20) // upstream: !IsOnGround()
			{
				pmove->origin[2] -= 0.1f;
				PM_CatagorizePosition();
				tries++;
			}
			// if we can't get properly back on ground, smth is wrong, and we cannot reliably do auto-crouch (steep ground or whatever), cancel and use standing movement
			if (pmove->onground == -1) // upstream: !IsOnGround()
			{
				VectorCopy(standmove_origin, pmove->origin);
				VectorCopy(standmove_velocity, pmove->velocity);
				pmove->usehull = 0;
				pmove->flags &= ~FL_DUCKING;
				PM_CatagorizePosition();
				return;
			}
		}

		// Move crouching
		PM_HandleMovement(pLadder);

		// Backup duckmove origin
		Vector duckmove_origin;
		VectorCopy(pmove->origin, duckmove_origin);

		// Get distance moved ducking and standing
		Vector standmove_dist;
		Vector duckmove_dist;
		VectorSubtract(standmove_origin, original_origin, standmove_dist);
		VectorSubtract(duckmove_origin, original_origin, duckmove_dist);

		// Restore standing
		pmove->usehull = 0;
		pmove->flags &= ~FL_DUCKING;

		// Use ducking if we walked 50% further ducking than standing (happens on entering something where we must duck)
		if (duckmove_dist.Length2D() > standmove_dist.Length2D() * 1.5f)
		{
			// check if we can actually stand at the ducking origin!
			Vector standing_ducking_origin;
			VectorCopy(duckmove_origin, standing_ducking_origin);
			standing_ducking_origin[2] = standing_ducking_origin[2] + pmove->player_mins[1][2] - pmove->player_mins[0][2];
			if (pmove->PM_TestPlayerPosition(standing_ducking_origin, nullptr) < 0)
			{
				// use standing origin, if we can actually stand at the ducking origin!
				VectorCopy(standmove_origin, pmove->origin);
				VectorCopy(standmove_velocity, pmove->velocity);
			}
			else
			{
				// ducking origin is further away, and we can't stand here, really use ducking origin and stay ducked
				pmove->usehull = 1;
				pmove->flags |= FL_DUCKING;
				VectorCopy(duckmove_origin, pmove->origin);
			}
		}
		else  // Use standing otherwise
		{
			VectorCopy(standmove_origin, pmove->origin);
			VectorCopy(standmove_velocity, pmove->velocity);
		}
	}
	else
	{
		PM_HandleMovement(pLadder);
	}
	// BSVR end

	PM_CatagorizePosition();
}

void PM_CreateStuckTable()
{
	float x, y, z;
	int idx;
	int i;
	float zi[3];

	memset(rgv3tStuckTable, 0, 54 * sizeof(Vector));

	idx = 0;
	// Little Moves.
	x = y = 0;
	// Z moves
	for (z = -0.125; z <= 0.125; z += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	x = z = 0;
	// Y moves
	for (y = -0.125; y <= 0.125; y += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -0.125; x <= 0.125; x += 0.125)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (x = -0.125; x <= 0.125; x += 0.250)
	{
		for (y = -0.125; y <= 0.125; y += 0.250)
		{
			for (z = -0.125; z <= 0.125; z += 0.250)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}

	// Big Moves.
	x = y = 0;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for (i = 0; i < 3; i++)
	{
		// Z moves
		z = zi[i];
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	x = z = 0;

	// Y moves
	for (y = -2.0f; y <= 2.0f; y += 2.0)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}
	y = z = 0;
	// X moves
	for (x = -2.0f; x <= 2.0f; x += 2.0f)
	{
		rgv3tStuckTable[idx][0] = x;
		rgv3tStuckTable[idx][1] = y;
		rgv3tStuckTable[idx][2] = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for (i = 0; i < 3; i++)
	{
		z = zi[i];

		for (x = -2.0f; x <= 2.0f; x += 2.0f)
		{
			for (y = -2.0f; y <= 2.0f; y += 2.0)
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}
}



/*
This modume implements the shared player physics code between any particular game and
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
*/

void PM_Move(struct playermove_s* ppmove, qboolean server)
{
	// BSVR start
	if (VRIsPullingOnLedge(ppmove->player_index))
		return;

	if (VRGetLadderMode() == VR_LADDER_MODE_IMMERSIVE_ONLY
		&& VRIsGrabbingLadder())
	{
		return;
	}
	// BSVR end

	assert(pm_shared_initialized);

	pmove = ppmove;

	PM_PlayerMove(server);

	if (pmove->onground != -1)
	{
		pmove->flags |= FL_ONGROUND;
	}
	else
	{
		pmove->flags &= ~FL_ONGROUND;
	}

	// Reset friction after each movement so FrictionModifier Triggers work still.
	if (pmove->movetype == MOVETYPE_WALK)
	{
		pmove->friction = 1.0f;
	}
}

int PM_GetVisEntInfo(int ent)
{
	if (ent >= 0 && ent <= pmove->numvisent)
	{
		return pmove->visents[ent].info;
	}
	return -1;
}

int PM_GetPhysEntInfo(int ent)
{
	if (ent >= 0 && ent <= pmove->numphysent)
	{
		return pmove->physents[ent].info;
	}
	return -1;
}

void PM_Init(struct playermove_s* ppmove)
{
	assert(!pm_shared_initialized);

	pmove = ppmove;

	PM_CreateStuckTable();
	PM_InitTextureTypes();

	//The engine copies the hull sizes initialized by PM_GetHullBounds *before* PM_GetHullBounds is actually called, so manually initialize these.
	for (int i = 0; i < NUM_HULLS; ++i)
	{
		if (!PM_GetHullBounds(i, pmove->player_mins[i], pmove->player_maxs[i]))
		{
			//Matches the engine's behavior in ignoring the remaining hull sizes if any hull isn't provided.
			break;
		}
	}

	pm_shared_initialized = true;
}

bool PM_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	switch (hullnumber)
	{
	case 0: // Normal player
		VEC_HULL_MIN.CopyToArray(mins);
		VEC_HULL_MAX.CopyToArray(maxs);
		return true;
	case 1: // Crouched player
		VEC_DUCK_HULL_MIN.CopyToArray(mins);
		VEC_DUCK_HULL_MAX.CopyToArray(maxs);
		return true;
	case 2: // Point based hull
		g_vecZero.CopyToArray(mins);
		g_vecZero.CopyToArray(maxs);
		return true;
	}

	return false;
}

// BSVR start
// Added, so server.dll can access bsp models (see UTIL_GetBSPModel in util.cpp) - Max Makes Mods, 2018-01-21
struct playermove_s* PM_GetPlayerMove(void)
{
	return pmove;
}
// BSVR end
