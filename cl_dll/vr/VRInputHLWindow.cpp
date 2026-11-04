
#include <iostream>
#include <filesystem>
#include <string>
#include <functional>
#include <fstream>
#include <cctype>
#include <regex>

#include "hud.h"
#include "cl_util.h"
#include "VRInput.h"
#include "eiface.h"
#include "VRCommon.h"
#include "VRRenderer.h"

#include <algorithm>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

// TODO: Maybe add support for other systems as well?
#ifdef _WIN32
#define HSPRITE WINDOWS_HSPRITE
#include <windows.h>
#undef HSPRITE
#else
#include <SDL2/SDL_messagebox.h>
#endif

#ifdef _WIN32
namespace
{
	HWND hlWindow{ nullptr };

	BOOL CALLBACK VRFindHalflifeWindow(_In_ HWND hwnd, _In_ LPARAM lParam)
	{
		if (IsWindowVisible(hwnd))
		{
			DWORD procId = GetCurrentProcessId();
			DWORD windowProcId;
			GetWindowThreadProcessId(hwnd, &windowProcId);
			if (procId == windowProcId)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				if (rect.right > rect.left&& rect.bottom > rect.top)
				{
					hlWindow = hwnd;
					return FALSE;
				}
			}
		}
		return TRUE;
	}

	void VRFindHLWindow()
	{
		if (!IsWindow(hlWindow) || !IsWindowVisible(hlWindow))
		{
			EnumWindows(VRFindHalflifeWindow, 0);
		}
	}

	// BSVR: VRGetAbsoluteHLWindowCoords, VRSendMouseMessageToHLWindow and VRSendMouseClickToHLWindow removed,
	// the mouse is sent through SDL2 below

	void VRForceWindowToForeground()
	{
		VRFindHLWindow();
		if (hlWindow != GetFocus())
		{
			SwitchToThisWindow(hlWindow, TRUE);
		}
	}

	void VRDisplayErrorPopup(const char* errorMessage)
	{
		VRFindHLWindow();
		MessageBoxA(hlWindow, errorMessage, "Error starting Half-Life: VR", MB_OK);
	}
}  // namespace
#endif

// The mouse is sent through SDL2, which the engine uses for its window and input, so this works on every platform
// and doesn't need the game window in the foreground. x and y go from 0 to 1 across the window.
namespace
{
	// The engine has one SDL window, SDL window IDs start at 1.
	// (The SDL2 the client links against is too old for SDL_GL_GetCurrentWindow.)
	SDL_Window* VRGetSDLGameWindow()
	{
		if (SDL_Window* focused = SDL_GetKeyboardFocus())
			return focused;
		for (Uint32 id = 1; id < 16; id++)
		{
			if (SDL_Window* window = SDL_GetWindowFromID(id))
				return window;
		}
		return nullptr;
	}

	bool VRGetSDLWindowCoords(float x, float y, SDL_Window** window, int* ix, int* iy)
	{
		*window = VRGetSDLGameWindow();
		if (*window == nullptr)
			return false;
		int width = 0, height = 0;
		SDL_GetWindowSize(*window, &width, &height);
		if (width <= 0 || height <= 0)
			return false;
		*ix = std::clamp(int(x * width), 0, width - 1);
		*iy = std::clamp(int(y * height), 0, height - 1);
		return true;
	}

	void VRSendSDLMouseButton(float x, float y, bool down)
	{
		SDL_Window* window;
		int ix, iy;
		if (!VRGetSDLWindowCoords(x, y, &window, &ix, &iy))
			return;

		SDL_Event event{};
		event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		event.button.timestamp = SDL_GetTicks();
		event.button.windowID = SDL_GetWindowID(window);
		event.button.which = 0;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.button.clicks = 1;
		event.button.x = ix;
		event.button.y = iy;
		SDL_PushEvent(&event);
	}
}  // namespace

void VRInput::SendMousePosToHLWindow(float x, float y)
{
	SDL_Window* window;
	int ix, iy;
	if (!VRGetSDLWindowCoords(x, y, &window, &ix, &iy))
		return;

	// Warping also moves SDL's own cursor position (which the engine reads) and sends a motion event.
	// Only warp when the position changed, so a real mouse still works while the beam rests.
	static int lastX = -1;
	static int lastY = -1;
	if (ix == lastX && iy == lastY)
		return;
	lastX = ix;
	lastY = iy;
	SDL_WarpMouseInWindow(window, ix, iy);
}

void VRInput::SendMouseButtonClickToHLWindow(float x, float y)
{
	VRSendSDLMouseButton(x, y, true);
}

void VRInput::SendMouseButtonUpToHLWindow(float x, float y)
{
	VRSendSDLMouseButton(x, y, false);
}

void VRInput::DisplayErrorPopup(const char* errorMessage)
{
	#ifdef _WIN32
	VRDisplayErrorPopup(errorMessage);
	#else
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error starting Half-Life: VR", errorMessage, nullptr);
	#endif
}

void VRInput::ForceWindowToForeground()
{
	#ifdef _WIN32
	VRForceWindowToForeground();
	#endif
}
