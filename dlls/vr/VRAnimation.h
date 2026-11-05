// Server-side bone setup for hitboxes and attachments, moved from animation.h

#pragma once

#include "extdll.h"

struct StudioHitBox
{
	Vector abscenter;
	Vector origin;
	Vector angles;
	Vector mins;
	Vector maxs;
	int hitgroup = 0;
};
struct StudioAttachment
{
	Vector pos;
};
int GetNumSequences(void* pmodel);
int GetNumHitboxes(void* pmodel);
int GetNumBodies(void* pmodel);
int GetNumAttachments(void* pmodel);
int GetHitboxesAndAttachments(entvars_t* pev, void* pmodel, int sequence, float frame, StudioHitBox* hitboxes, StudioAttachment* attachments, bool mirrored = false);
