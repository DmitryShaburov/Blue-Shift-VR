#include <cmath>
#include <map>
#include <regex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include "ammohistory.h"
#include "com_model.h"

#include "VRWeaponWheel.h"
#include "VRRenderer.h"
#include "VRHelper.h"
#include "VRInput.h"
#include "VRTextureHelper.h"
#include "vr_gl.h"

namespace
{
	// Weapon categories, same as the original HL weapon slots
	constexpr int NUM_CATEGORIES = 5;

	// All sizes are in HL units
	constexpr float WHEEL_INNER_RADIUS = 3.f;
	constexpr float WHEEL_OUTER_RADIUS = 9.f;
	constexpr float WHEEL_SEGMENT_GAP_DEGREES = 3.f;
	constexpr int WHEEL_SEGMENT_STEPS = 12;
	constexpr float WHEEL_ALPHA = 0.7f;
	constexpr float WHEEL_DEAD_ZONE = 2.5f;  // hand offset below this hovers nothing
	constexpr float WHEEL_HOVER_HAPTIC_INTENSITY = 0.3f;

	// Segment brightness relative to the HUD color
	constexpr float WHEEL_COLOR_SCALE = 0.25f;
	constexpr float WHEEL_HOVER_COLOR_SCALE = 0.6f;
	constexpr float WHEEL_EMPTY_COLOR_SCALE = 0.1f;

	// Weapon icons are 170x45 sprite rects, this is how many HL units one sprite pixel covers
	constexpr float ICON_UNITS_PER_PIXEL = 6.f / 170.f;
	constexpr float ICON_CENTER_RADIUS = (WHEEL_INNER_RADIUS + WHEEL_OUTER_RADIUS) / 2.f;
	constexpr int ICON_INACTIVE_BRIGHTNESS = 192;  // same as the HL weapon list

	// Slot number label, outside of the ring
	constexpr float LABEL_RADIUS = WHEEL_OUTER_RADIUS + 1.5f;
	constexpr float LABEL_UNITS_PER_PIXEL = 0.08f;

	// Pixels below this brightness count as empty when looking for the visible part of an icon
	constexpr int ICON_VISIBLE_THRESHOLD = 40;

	constexpr float PI = 3.14159265358979f;

	bool g_isOpen = false;
	int g_lastHovered = -1;

	// Fixed when the wheel is opened
	Vector g_center;
	Vector g_right;
	Vector g_up;

	// HL coordinates of the HUD sprites are in the size of the original sprite sheet
	struct SheetSize
	{
		int width;
		int height;
	};
	const std::unordered_map<std::string, SheetSize> g_sheetSizes = {
		{ "640hud1", { 256, 256 } },
		{ "640hud2", { 256, 256 } },
		{ "640hud3", { 256, 256 } },
		{ "640hud4", { 256, 256 } },
		{ "640hud5", { 256, 256 } },
		{ "640hud6", { 256, 256 } },
		{ "640hud7", { 256, 128 } },
		{ "640hud8", { 256, 64 } },
		{ "640hud9", { 256, 64 } },
		{ "320hud1", { 256, 256 } },
		{ "320hud2", { 128, 128 } },
		{ "320hud3", { 256, 64 } },
		{ "320hud4", { 256, 32 } },
	};

	struct VisibleBounds
	{
		float left, right, top, bottom;  // in sprite pixels, relative to the sheet
	};

	// Returns the part of the rect that actually has something drawn, so icons can be centered
	// (the weapon sprites are right aligned in their rects)
	VisibleBounds GetVisibleBounds(unsigned int texture, int textureWidth, int textureHeight, const SheetSize& sheet, const Rect& rc)
	{
		static std::map<std::tuple<unsigned int, int, int, int, int>, VisibleBounds> cache;

		const auto key = std::make_tuple(texture, rc.left, rc.top, rc.right, rc.bottom);
		auto it = cache.find(key);
		if (it != cache.end())
			return it->second;

		VisibleBounds bounds{ static_cast<float>(rc.left), static_cast<float>(rc.right), static_cast<float>(rc.top), static_cast<float>(rc.bottom) };

		std::vector<unsigned char> pixels(static_cast<size_t>(textureWidth) * textureHeight * 4);
		glBindTexture(GL_TEXTURE_2D, texture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		const float scaleX = static_cast<float>(textureWidth) / sheet.width;
		const float scaleY = static_cast<float>(textureHeight) / sheet.height;
		int minX = textureWidth, maxX = -1, minY = textureHeight, maxY = -1;
		for (int y = static_cast<int>(rc.top * scaleY); y < static_cast<int>(rc.bottom * scaleY) && y < textureHeight; y++)
		{
			for (int x = static_cast<int>(rc.left * scaleX); x < static_cast<int>(rc.right * scaleX) && x < textureWidth; x++)
			{
				const unsigned char* p = &pixels[(static_cast<size_t>(y) * textureWidth + x) * 4];
				const int brightness = (std::max)({ p[0], p[1], p[2] }) * p[3] / 255;
				if (brightness > ICON_VISIBLE_THRESHOLD)
				{
					minX = (std::min)(minX, x);
					maxX = (std::max)(maxX, x);
					minY = (std::min)(minY, y);
					maxY = (std::max)(maxY, y);
				}
			}
		}
		if (maxX >= 0)
		{
			bounds = { minX / scaleX, (maxX + 1) / scaleX, minY / scaleY, (maxY + 1) / scaleY };
		}

		cache[key] = bounds;
		return bounds;
	}

	// Draws a HUD sprite rect as a quad in the wheel plane, centered at the given point
	void DrawSprite(HSPRITE sprite, const Rect& rc, const Vector& position, float unitsPerPixel, int r, int g, int b, bool centerOnVisible)
	{
		const model_s* model = gEngfuncs.GetSpritePointer(sprite);
		if (model == nullptr)
			return;

		// "sprites/640hud7.spr" -> "640hud7"
		const std::string sheetName = std::regex_replace(model->name, std::regex{ "^sprites/([a-z0-9_]+)\\.spr$" }, "$1");
		const auto sheetIt = g_sheetSizes.find(sheetName);
		if (sheetIt == g_sheetSizes.end())
			return;
		const SheetSize& sheet = sheetIt->second;

		unsigned int textureWidth = 0, textureHeight = 0;
		const unsigned int texture = VRTextureHelper::Instance().GetTexture("/hud/" + sheetName + ".png", textureWidth, textureHeight);
		if (texture == 0)
			return;

		VisibleBounds visible{ static_cast<float>(rc.left), static_cast<float>(rc.right), static_cast<float>(rc.top), static_cast<float>(rc.bottom) };
		if (centerOnVisible)
			visible = GetVisibleBounds(texture, textureWidth, textureHeight, sheet, rc);

		// Offsets from the point to the rect edges, so the center of the visible part lands on it
		const float centerX = (visible.left + visible.right) / 2.f;
		const float centerY = (visible.top + visible.bottom) / 2.f;
		const float left = (rc.left - centerX) * unitsPerPixel;
		const float right = (rc.right - centerX) * unitsPerPixel;
		const float top = -(rc.top - centerY) * unitsPerPixel;
		const float bottom = -(rc.bottom - centerY) * unitsPerPixel;

		const float u0 = static_cast<float>(rc.left) / sheet.width;
		const float u1 = static_cast<float>(rc.right) / sheet.width;
		const float v0 = static_cast<float>(rc.top) / sheet.height;
		const float v1 = static_cast<float>(rc.bottom) / sheet.height;

		auto vertex = [&](float u, float v, float x, float y)
		{
			const Vector pos = position + g_right * x + g_up * y;
			glTexCoord2f(u, v);
			glVertex3f(pos.x, pos.y, pos.z);
		};

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor4f(r / 255.f, g / 255.f, b / 255.f, 1.f);
		glBegin(GL_QUADS);
		vertex(u0, v0, left, top);
		vertex(u1, v0, right, top);
		vertex(u1, v1, right, bottom);
		vertex(u0, v1, left, bottom);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
	}

	// Center of the category's segment, as a direction in the wheel plane. Category 0 is at the top, going clockwise.
	Vector GetCategoryDirection(int category)
	{
		const float angle = PI / 2.f - category * (2.f * PI / NUM_CATEGORIES);
		return g_right * cosf(angle) + g_up * sinf(angle);
	}

	// The weapon shown for a category: the current weapon if it belongs there, otherwise the first one with ammo
	WEAPON* GetCategoryWeapon(int category)
	{
		WEAPON* current = gHUD.m_Ammo.m_pWeapon;
		if (current != nullptr && current->iSlot == category && gWR.HasAmmo(current))
			return current;

		return gWR.GetFirstPos(category);
	}

	// Returns the category the weapon hand points at, or -1 inside the dead zone
	int GetHoveredCategory()
	{
		VRHelper* helper = gVRRenderer.GetHelper();
		if (helper == nullptr)
			return -1;

		const Vector offset = helper->GetWeaponPosition() - g_center;
		const float x = DotProduct(offset, g_right);
		const float y = DotProduct(offset, g_up);
		if (x * x + y * y < WHEEL_DEAD_ZONE * WHEEL_DEAD_ZONE)
			return -1;

		// Clockwise angle from the top, same layout as the drawn segments
		float angle = atan2f(x, y);
		if (angle < 0.f)
			angle += 2.f * PI;

		const float segmentAngle = 2.f * PI / NUM_CATEGORIES;
		return static_cast<int>(angle / segmentAngle + 0.5f) % NUM_CATEGORIES;
	}
}  // namespace

namespace VR
{
	namespace WeaponWheel
	{
		void Open()
		{
			VRHelper* helper = gVRRenderer.GetHelper();
			if (helper == nullptr || !helper->HasValidWeaponController())
			{
				gEngfuncs.Con_DPrintf("WeaponWheel: Open failed, no valid weapon controller\n");
				return;
			}

			Vector forward;
			helper->GetViewVectors(forward, g_right, g_up);
			g_center = helper->GetWeaponPosition();
			g_isOpen = true;
			g_lastHovered = -1;
		}

		void Close()
		{
			g_isOpen = false;
		}

		bool IsOpen()
		{
			return g_isOpen;
		}

		void Render()
		{
			if (!g_isOpen)
				return;

			glPushAttrib(GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);

			// The engine may leave any of this on; plain colored geometry needs it all off
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_LIGHTING);
			glDisable(GL_FOG);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_SCISSOR_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// Blue Shift's HUD color (named RGB_YELLOWISH in the HL code)
			int hudR, hudG, hudB;
			UnpackRGB(hudR, hudG, hudB, RGB_YELLOWISH);

			WEAPON* categoryWeapons[NUM_CATEGORIES];
			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				categoryWeapons[i] = GetCategoryWeapon(i);
			}

			// Categories without a weapon can't be selected
			int hovered = GetHoveredCategory();
			if (hovered != -1 && categoryWeapons[hovered] == nullptr)
				hovered = -1;

			if (hovered != g_lastHovered)
			{
				// Render runs once per eye, the second call sees no change
				g_lastHovered = hovered;
				if (hovered != -1)
				{
					PlaySound("common/wpn_moveselect.wav", 1);
					g_vrInput.recoilintensity = WHEEL_HOVER_HAPTIC_INTENSITY;
				}
			}

			const float segmentAngle = 2.f * PI / NUM_CATEGORIES;
			const float gap = WHEEL_SEGMENT_GAP_DEGREES * PI / 180.f;

			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				// Category 0 is centered at the top, going clockwise
				const float centerAngle = PI / 2.f - i * segmentAngle;
				const float start = centerAngle - segmentAngle / 2.f + gap / 2.f;
				const float end = centerAngle + segmentAngle / 2.f - gap / 2.f;

				float colorScale = WHEEL_COLOR_SCALE;
				if (categoryWeapons[i] == nullptr)
					colorScale = WHEEL_EMPTY_COLOR_SCALE;
				else if (i == hovered)
					colorScale = WHEEL_HOVER_COLOR_SCALE;
				glColor4f(hudR / 255.f * colorScale, hudG / 255.f * colorScale, hudB / 255.f * colorScale, WHEEL_ALPHA);

				glBegin(GL_QUAD_STRIP);
				for (int step = 0; step <= WHEEL_SEGMENT_STEPS; step++)
				{
					const float angle = start + (end - start) * step / WHEEL_SEGMENT_STEPS;
					const Vector dir = g_right * cosf(angle) + g_up * sinf(angle);
					const Vector inner = g_center + dir * WHEEL_INNER_RADIUS;
					const Vector outer = g_center + dir * WHEEL_OUTER_RADIUS;
					glVertex3f(inner.x, inner.y, inner.z);
					glVertex3f(outer.x, outer.y, outer.z);
				}
				glEnd();
			}

			// Sprites are additive, like everything else on the HL HUD
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				const Vector direction = GetCategoryDirection(i);
				WEAPON* weapon = categoryWeapons[i];

				if (weapon != nullptr)
				{
					int r = hudR, g = hudG, b = hudB;
					HSPRITE sprite = weapon->hInactive;
					const Rect* rect = &weapon->rcInactive;
					if (i == hovered && weapon->hActive != 0)
					{
						sprite = weapon->hActive;
						rect = &weapon->rcActive;
					}
					else
					{
						ScaleColors(r, g, b, ICON_INACTIVE_BRIGHTNESS);
					}

					if (sprite != 0)
						DrawSprite(sprite, *rect, g_center + direction * ICON_CENTER_RADIUS, ICON_UNITS_PER_PIXEL, r, g, b, true);
				}

				// Slot number
				const int digit = gHUD.m_HUD_number_0 + i + 1;
				int r = hudR, g = hudG, b = hudB;
				if (weapon == nullptr)
					ScaleColors(r, g, b, 96);
				DrawSprite(gHUD.GetSprite(digit), gHUD.GetSpriteRect(digit), g_center + direction * LABEL_RADIUS, LABEL_UNITS_PER_PIXEL, r, g, b, false);
			}

			glPopAttrib();
		}
	}  // namespace WeaponWheel
}  // namespace VR
