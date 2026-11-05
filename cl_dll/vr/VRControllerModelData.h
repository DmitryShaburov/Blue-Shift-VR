// Hand/weapon model data sent by the server for rendering the VR controllers, moved from hud.h

#pragma once

#include "cl_dll.h"

// For precise position when rendering
struct ControllerModelData
{
	struct ModelData
	{
		int body{0};
		int skin{0};
		float scale{1.f};
		int sequence{0};
		float frame{0.f};
		float framerate{0.f};
		float animtime{0.f};
		int effects{0};
		int rendermode{0};
		int renderamt{0};
		int renderfx{0};
		color24 rendercolor{0, 0, 0};
		char modelname[1024];
	};

	ModelData controller;

	bool hasDraggedEnt{false};
	int draggedEntIndex{0};
	ModelData draggedEnt;
};
