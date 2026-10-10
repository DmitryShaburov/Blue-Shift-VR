#pragma once

// BSVR start
// Engine-side BSP structures as laid out by the Half-Life 25th anniversary engine
// in hardware (OpenGL) mode. The SDK's com_model.h describes the legacy software
// renderer layout, which differs in msurface_t and texture_t. This mod only runs on
// the 25th anniversary OpenGL engine, so VR code accesses engine memory through the
// types below instead of the SDK types. Field order follows Quake's gl_model.h with the
// anniversary engine's trailing display list block (same as yapb's msurface_hw_hl25_t).
//
// Self-contained: pulls in the SDK headers that com_model.h itself expects to be included first.

#include <cstddef>
#include "Platform.h"
#include "mathlib.h"
#include "const.h"
#include "com_model.h"

#define SURF_PLANEBACK 2 // plane should be negated

#define VERTEXSIZE 7

typedef struct glpoly_s
{
	struct glpoly_s* next;
	struct glpoly_s* chain;
	int numverts;
	int flags;					// for SURF_UNDERWATER
	float verts[4][VERTEXSIZE]; // variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct mdisplaylist_s
{
	unsigned int gl_displaylist;
	int rendermode;
	float scrolloffset;
	int renderDetailTexture;
} mdisplaylist_t;

typedef struct msurface_hl25_s
{
	int visframe; // should be drawn when node is crossed

	mplane_t* plane; // pointer to shared plane
	int flags;		 // see SURF_ #defines

	int firstedge; // look up in model->surfedges[], negative numbers
	int numedges;  // are backwards edges

	short texturemins[2]; // smallest s/t position on the surface.
	short extents[2];	  // ?? s/t texture size, 1..256 for all non-sky surfaces

	int light_s, light_t; // gl lightmap coordinates

	glpoly_t* polys; // multiple if warped
	struct msurface_hl25_s* texturechain;

	mtexinfo_t* texinfo;

	// lighting info
	int dlightframe;
	int dlightbits;

	int lightmaptexturenum;
	byte styles[MAXLIGHTMAPS];
	int cached_light[MAXLIGHTMAPS]; // values currently used in lightmap
	qboolean cached_dlight;			// true if dynamic light in cache

	color24* samples; // [numstyles*surfsize]

	decal_t* pdecals;

	mdisplaylist_t displaylist; // 25th anniversary engine only
} msurface_hl25_t;

typedef struct texture_hl25_s
{
	char name[16];
	unsigned width, height;
	int gl_texturenum;
	struct msurface_hl25_s* texturechain;	// for gl_texsort drawing
	int anim_total;							// total tenths in sequence ( 0 = no)
	int anim_min, anim_max;					// time for this frame min <=time< max
	struct texture_hl25_s* anim_next;		// in the animation sequence
	struct texture_hl25_s* alternate_anims; // bmodels in frame 1 use these
	unsigned offsets[MIPLEVELS];			// four mip maps stored
	unsigned paloffset;
} texture_hl25_t;

// Both DLLs are built 32-bit for the 32-bit engine; these sizes are what the engine uses.
static_assert(sizeof(void*) == 4, "VRComModelHL25.h layouts assume a 32-bit build");
static_assert(sizeof(msurface_hl25_t) == 108, "msurface_hl25_t must match the 25th anniversary OpenGL engine layout");
static_assert(offsetof(texture_hl25_t, gl_texturenum) == 24, "texture_hl25_t must match the OpenGL engine layout");

// Accessors that reinterpret the SDK-typed model_t pointers as the live engine layout.
inline const msurface_hl25_t* VRGetSurface(const model_t* model, int index)
{
	return reinterpret_cast<const msurface_hl25_t*>(model->surfaces) + index;
}

inline const msurface_hl25_t* VRGetMarkSurface(const model_t* model, int index)
{
	return reinterpret_cast<const msurface_hl25_t*>(model->marksurfaces[index]);
}

inline texture_hl25_t* VRGetTexture(const model_t* model, int index)
{
	return reinterpret_cast<texture_hl25_t*>(model->textures[index]);
}

// Runtime sanity check that the engine really uses the msurface_hl25_t stride: the plane and
// texinfo pointers of a loaded model's first surfaces must point into that model's own arrays.
// If a future engine update changes the layout again, this fails instead of reading garbage.
inline bool VRIsModelLayoutValid(const model_t* model)
{
	if (model == nullptr || model->needload != 0 || model->surfaces == nullptr || model->nummodelsurfaces <= 0)
		return false;

	const int count = (model->nummodelsurfaces < 16) ? model->nummodelsurfaces : 16;
	for (int i = 0; i < count; ++i)
	{
		const msurface_hl25_t* surface = VRGetSurface(model, model->firstmodelsurface + i);
		if (surface->plane < model->planes || surface->plane >= model->planes + model->numplanes)
			return false;
		if (surface->texinfo < model->texinfo || surface->texinfo >= model->texinfo + model->numtexinfo)
			return false;
	}
	return true;
}
// BSVR end
