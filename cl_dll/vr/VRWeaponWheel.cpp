#include <algorithm>
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

	// The weapons of the hovered category fan out in an arc beyond the ring
	constexpr float ARC_INNER_RADIUS = WHEEL_OUTER_RADIUS + 0.5f;
	constexpr float ARC_OUTER_RADIUS = ARC_INNER_RADIUS + 5.5f;
	constexpr float ARC_CENTER_RADIUS = (ARC_INNER_RADIUS + ARC_OUTER_RADIUS) / 2.f;
	constexpr float ARC_ENTER_DISTANCE = 8.f;  // hand offset from the center from which the arc items are hovered

	// The melee category has the empty hand (holster) after its weapons
	constexpr int MELEE_CATEGORY = 0;
	constexpr int MAX_ARC_ITEMS = MAX_WEAPON_POSITIONS + 1;

	// Pixels below this brightness count as empty when looking for the visible part of an icon
	constexpr int ICON_VISIBLE_THRESHOLD = 40;

	// The HUD sheets have no alpha, their background is dark grey (about 21-29) with scanlines.
	// Brightness up to this value is treated as transparent, the rest is scaled to 0..1 alpha.
	constexpr int ICON_BACKGROUND_FLOOR = 30;

	constexpr float PI = 3.14159265358979f;

	constexpr float SEGMENT_ANGLE = 2.f * PI / NUM_CATEGORIES;

	struct Hover
	{
		int category = -1;
		int item = -1;  // index in the category's arc, -1 while the hand is still inside the ring
	};

	bool g_isOpen = false;
	Hover g_lastHover;

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

	// Returns a copy of the sheet texture that is white with alpha from the pixel brightness, so the
	// dark background doesn't show up as a lighter rectangle. The sprite color is applied when drawing.
	unsigned int GetMaskedTexture(unsigned int texture, int textureWidth, int textureHeight)
	{
		static std::unordered_map<unsigned int, unsigned int> cache;

		auto it = cache.find(texture);
		if (it != cache.end())
			return it->second;

		std::vector<unsigned char> pixels(static_cast<size_t>(textureWidth) * textureHeight * 4);
		glBindTexture(GL_TEXTURE_2D, texture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		for (size_t i = 0; i < pixels.size(); i += 4)
		{
			const int brightness = (std::max)({ pixels[i], pixels[i + 1], pixels[i + 2] });
			const int alpha = (std::max)(0, brightness - ICON_BACKGROUND_FLOOR) * 255 / (255 - ICON_BACKGROUND_FLOOR);
			pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
			pixels[i + 3] = static_cast<unsigned char>(alpha);
		}

		unsigned int masked = 0;
		glGenTextures(1, &masked);
		glBindTexture(GL_TEXTURE_2D, masked);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		cache[texture] = masked;
		return masked;
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

		// Sprites are packed tightly on the sheet, so sampling right on the rect edge blends in a column of the
		// neighbouring sprite. Stay half a texel inside.
		const float scaleX = static_cast<float>(textureWidth) / sheet.width;
		const float scaleY = static_cast<float>(textureHeight) / sheet.height;
		const float u0 = (rc.left * scaleX + 0.5f) / textureWidth;
		const float u1 = (rc.right * scaleX - 0.5f) / textureWidth;
		const float v0 = (rc.top * scaleY + 0.5f) / textureHeight;
		const float v1 = (rc.bottom * scaleY - 0.5f) / textureHeight;

		auto vertex = [&](float u, float v, float x, float y)
		{
			const Vector pos = position + g_right * x + g_up * y;
			glTexCoord2f(u, v);
			glVertex3f(pos.x, pos.y, pos.z);
		};

		const unsigned int maskedTexture = GetMaskedTexture(texture, textureWidth, textureHeight);
		if (maskedTexture == 0)
			return;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, maskedTexture);
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
		const float angle = PI / 2.f - category * SEGMENT_ANGLE;
		return g_right * cosf(angle) + g_up * sinf(angle);
	}

	// Draws a ring sector between two angles (counter-clockwise from the wheel's right) in the current color
	void DrawSector(float innerRadius, float outerRadius, float start, float end)
	{
		glBegin(GL_QUAD_STRIP);
		for (int step = 0; step <= WHEEL_SEGMENT_STEPS; step++)
		{
			const float angle = start + (end - start) * step / WHEEL_SEGMENT_STEPS;
			const Vector dir = g_right * cosf(angle) + g_up * sinf(angle);
			const Vector inner = g_center + dir * innerRadius;
			const Vector outer = g_center + dir * outerRadius;
			glVertex3f(inner.x, inner.y, inner.z);
			glVertex3f(outer.x, outer.y, outer.z);
		}
		glEnd();
	}

	// Simple open hand drawn with quads in the wheel plane (the HUD sheets have no hand icon)
	void DrawHandIcon(const Vector& position, int r, int g, int b)
	{
		auto quad = [&](float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
		{
			const float xs[4] = { x0, x1, x2, x3 };
			const float ys[4] = { y0, y1, y2, y3 };
			for (int i = 0; i < 4; i++)
			{
				const Vector pos = position + g_right * xs[i] + g_up * ys[i];
				glVertex3f(pos.x, pos.y, pos.z);
			}
		};

		glColor4f(r / 255.f, g / 255.f, b / 255.f, 1.f);
		glBegin(GL_QUADS);
		// Palm
		quad(-1.0f, -1.4f, 1.0f, -1.4f, 1.0f, 0.4f, -1.0f, 0.4f);
		// Fingers
		const float fingerHeights[4] = { 1.4f, 1.8f, 1.6f, 1.2f };
		for (int i = 0; i < 4; i++)
		{
			const float x = -1.0f + i * 0.517f;
			quad(x, 0.5f, x + 0.45f, 0.5f, x + 0.45f, 0.4f + fingerHeights[i], x, 0.4f + fingerHeights[i]);
		}
		// Thumb
		quad(-1.0f, -0.8f, -1.0f, 0.0f, -1.7f, 0.7f, -2.0f, 0.3f);
		glEnd();
	}

	// The weapon shown for a category: the current weapon if it belongs there, otherwise the first one with ammo
	WEAPON* GetCategoryWeapon(int category)
	{
		WEAPON* current = gHUD.m_Ammo.m_pWeapon;
		if (current != nullptr && current->iSlot == category && gWR.HasAmmo(current))
			return current;

		return gWR.GetFirstPos(category);
	}

	// The entries of a category's arc in slot order: the weapons the player can select, and for the
	// melee category the empty hand (a null entry) after them. Returns how many there are.
	int GetCategoryItems(int category, WEAPON* items[MAX_ARC_ITEMS])
	{
		int count = 0;
		for (int pos = 0; pos < MAX_WEAPON_POSITIONS; pos++)
		{
			WEAPON* weapon = gWR.GetWeaponSlot(category, pos);
			if (weapon != nullptr && weapon->iId != 0 && gWR.HasAmmo(weapon))
				items[count++] = weapon;
		}
		if (category == MELEE_CATEGORY)
			items[count++] = nullptr;

		return count;
	}

	void DrawWeaponIcon(WEAPON* weapon, const Vector& position, bool active, int hudR, int hudG, int hudB)
	{
		int r = hudR, g = hudG, b = hudB;
		HSPRITE sprite = weapon->hInactive;
		const Rect* rect = &weapon->rcInactive;
		if (active && weapon->hActive != 0)
		{
			sprite = weapon->hActive;
			rect = &weapon->rcActive;
		}
		else
		{
			ScaleColors(r, g, b, ICON_INACTIVE_BRIGHTNESS);
		}

		if (sprite != 0)
			DrawSprite(sprite, *rect, position, ICON_UNITS_PER_PIXEL, r, g, b, true);
	}

	// What the weapon hand points at. The category comes from the direction, the arc item from the direction
	// as well once the hand is far enough from the center.
	Hover GetHover(const int itemCounts[NUM_CATEGORIES])
	{
		Hover hover;

		VRHelper* helper = gVRRenderer.GetHelper();
		if (helper == nullptr)
			return hover;

		const Vector offset = helper->GetWeaponPosition() - g_center;
		const float x = DotProduct(offset, g_right);
		const float y = DotProduct(offset, g_up);
		const float distanceSquared = x * x + y * y;
		if (distanceSquared < WHEEL_DEAD_ZONE * WHEEL_DEAD_ZONE)
			return hover;

		// Clockwise angle from the top, same layout as the drawn segments
		float angle = atan2f(x, y);
		if (angle < 0.f)
			angle += 2.f * PI;

		hover.category = static_cast<int>(angle / SEGMENT_ANGLE + 0.5f) % NUM_CATEGORIES;

		const int count = itemCounts[hover.category];
		if (count > 0 && distanceSquared >= ARC_ENTER_DISTANCE * ARC_ENTER_DISTANCE)
		{
			// Angle from the category's clockwise-first edge
			float delta = angle - hover.category * SEGMENT_ANGLE;
			if (delta > PI)
				delta -= 2.f * PI;
			const float fromEdge = delta + SEGMENT_ANGLE / 2.f;

			hover.item = (std::max)(0, (std::min)(count - 1, static_cast<int>(fromEdge / (SEGMENT_ANGLE / count))));
		}

		return hover;
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
			g_lastHover = Hover{};
		}

		void Close()
		{
			g_isOpen = false;
		}

		Selection CloseAndGetSelection()
		{
			Selection selection;
			if (g_isOpen && g_lastHover.category != -1)
			{
				WEAPON* items[MAX_ARC_ITEMS];
				const int count = GetCategoryItems(g_lastHover.category, items);
				if (g_lastHover.item != -1 && g_lastHover.item < count)
				{
					selection.weapon = items[g_lastHover.item];
					selection.holster = selection.weapon == nullptr;
				}
				else
				{
					// Released inside the ring: the weapon shown for the category
					selection.weapon = GetCategoryWeapon(g_lastHover.category);
				}
			}

			g_isOpen = false;
			return selection;
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
			// On Windows the glDisable hook ignores GL_DEPTH_TEST while the VR scene is drawn (VROpenGLInterceptor.cpp),
			// so also make the test always pass and don't write depth. Otherwise the coplanar sprites z-fight with the segments.
			glDepthFunc(GL_ALWAYS);
			glDepthMask(GL_FALSE);
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
			WEAPON* items[NUM_CATEGORIES][MAX_ARC_ITEMS];
			int itemCounts[NUM_CATEGORIES];
			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				categoryWeapons[i] = GetCategoryWeapon(i);
				itemCounts[i] = GetCategoryItems(i, items[i]);
			}

			// Categories without a weapon can't be selected
			Hover hover = GetHover(itemCounts);
			if (hover.category != -1 && categoryWeapons[hover.category] == nullptr)
				hover = Hover{};

			if (hover.category != g_lastHover.category || hover.item != g_lastHover.item)
			{
				// Render runs once per eye, the second call sees no change
				g_lastHover = hover;
				if (hover.category != -1)
				{
					// Not PlaySound() from cl_util.h: windows.h (via vr_gl.h) turns that name into PlaySoundA
					gEngfuncs.pfnPlaySoundByName("common/wpn_moveselect.wav", 1);
					g_vrInput.recoilintensity = WHEEL_HOVER_HAPTIC_INTENSITY;
				}
			}

			const float gap = WHEEL_SEGMENT_GAP_DEGREES * PI / 180.f;

			auto setColor = [&](float colorScale)
			{
				glColor4f(hudR / 255.f * colorScale, hudG / 255.f * colorScale, hudB / 255.f * colorScale, WHEEL_ALPHA);
			};

			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				// Category 0 is centered at the top, going clockwise
				const float centerAngle = PI / 2.f - i * SEGMENT_ANGLE;

				float colorScale = WHEEL_COLOR_SCALE;
				if (categoryWeapons[i] == nullptr)
					colorScale = WHEEL_EMPTY_COLOR_SCALE;
				else if (i == hover.category)
					colorScale = WHEEL_HOVER_COLOR_SCALE;
				setColor(colorScale);

				DrawSector(WHEEL_INNER_RADIUS, WHEEL_OUTER_RADIUS, centerAngle - SEGMENT_ANGLE / 2.f + gap / 2.f, centerAngle + SEGMENT_ANGLE / 2.f - gap / 2.f);
			}

			// The arc of the hovered category. Its items split the category's span evenly, the first one on the clockwise-first side.
			const int arcCount = hover.category != -1 ? itemCounts[hover.category] : 0;
			const float itemAngle = arcCount > 0 ? SEGMENT_ANGLE / arcCount : 0.f;
			const float arcEdge = hover.category != -1 ? PI / 2.f - hover.category * SEGMENT_ANGLE + SEGMENT_ANGLE / 2.f : 0.f;

			for (int k = 0; k < arcCount; k++)
			{
				setColor(k == hover.item ? WHEEL_HOVER_COLOR_SCALE : WHEEL_COLOR_SCALE);
				DrawSector(ARC_INNER_RADIUS, ARC_OUTER_RADIUS, arcEdge - (k + 1) * itemAngle + gap / 2.f, arcEdge - k * itemAngle - gap / 2.f);
			}

			// Sprites are additive, like everything else on the HL HUD
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				const Vector direction = GetCategoryDirection(i);
				WEAPON* weapon = categoryWeapons[i];

				if (weapon != nullptr)
					DrawWeaponIcon(weapon, g_center + direction * ICON_CENTER_RADIUS, i == hover.category, hudR, hudG, hudB);

				// Slot number
				const int digit = gHUD.m_HUD_number_0 + i + 1;
				int r = hudR, g = hudG, b = hudB;
				if (weapon == nullptr)
					ScaleColors(r, g, b, 96);
				DrawSprite(gHUD.GetSprite(digit), gHUD.GetSpriteRect(digit), g_center + direction * LABEL_RADIUS, LABEL_UNITS_PER_PIXEL, r, g, b, false);
			}

			for (int k = 0; k < arcCount; k++)
			{
				const float angle = arcEdge - (k + 0.5f) * itemAngle;
				const Vector position = g_center + (g_right * cosf(angle) + g_up * sinf(angle)) * ARC_CENTER_RADIUS;
				WEAPON* weapon = items[hover.category][k];

				if (weapon != nullptr)
				{
					DrawWeaponIcon(weapon, position, k == hover.item, hudR, hudG, hudB);
				}
				else
				{
					int r = hudR, g = hudG, b = hudB;
					if (k != hover.item)
						ScaleColors(r, g, b, ICON_INACTIVE_BRIGHTNESS);
					DrawHandIcon(position, r, g, b);
				}
			}

			glPopAttrib();
		}
	}  // namespace WeaponWheel
}  // namespace VR
