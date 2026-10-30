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
	// The wheel is a honeycomb of flat-top hexagons, like in the HL2 VR mod. The empty hand (holster) is in the
	// center. Six spokes of weapons go out from it in the directions of 12, 2, 4, 6, 8 and 10 o'clock, in a
	// straight line of hexagons, the first weapon of a spoke is next to the center.
	constexpr int NUM_SPOKES = 6;
	constexpr int SPOKE_LENGTH = 3;
	constexpr int MAX_CELLS = 1 + NUM_SPOKES * SPOKE_LENGTH;

	// Weapon ids, the same as WEAPON_* in dlls/weapons.h (not included, it needs the server headers).
	// Ids work for any alias of the weapon entity, the weapon name the client gets is the classname the map used.
	constexpr int ID_CROWBAR = 1;
	constexpr int ID_GLOCK = 2;
	constexpr int ID_PYTHON = 3;
	constexpr int ID_MP5 = 4;
	constexpr int ID_CROSSBOW = 6;
	constexpr int ID_SHOTGUN = 7;
	constexpr int ID_RPG = 8;
	constexpr int ID_GAUSS = 9;
	constexpr int ID_EGON = 10;
	constexpr int ID_HORNETGUN = 11;
	constexpr int ID_HANDGRENADE = 12;
	constexpr int ID_TRIPMINE = 13;
	constexpr int ID_SATCHEL = 14;
	constexpr int ID_SNARK = 15;

	// Weapons of each spoke from the center outwards, 0 is unused. Only the weapons the player has are drawn,
	// one after the other without gaps, so the spoke grows as weapons are picked up. Weapons that don't exist
	// in the game (like the crossbow in Blue Shift) never show up.
	constexpr int SPOKES[NUM_SPOKES][SPOKE_LENGTH] = {
		{ ID_GLOCK, ID_PYTHON, 0 },                    // 12 o'clock: handguns
		{ ID_MP5, ID_SHOTGUN, ID_CROSSBOW },           // 2 o'clock: long guns
		{ ID_RPG, ID_GAUSS, ID_EGON },                 // 4 o'clock: heavy
		{ ID_CROWBAR, 0, 0 },                          // 6 o'clock: melee
		{ ID_SNARK, ID_HORNETGUN, 0 },                 // 8 o'clock: alien
		{ ID_HANDGRENADE, ID_SATCHEL, ID_TRIPMINE },   // 10 o'clock: explosives
	};

	// All sizes are in HL units (37.5 units per meter). Scales the whole wheel: hexagons, icons, outlines and
	// the distance at which the hand points at a hexagon.
	constexpr float WHEEL_SCALE = 1.f;

	constexpr float HEX_PITCH = 3.f * WHEEL_SCALE;  // distance between the centers of neighbouring hexagons, about 8 cm
	constexpr float HEX_GAP = 0.2f * WHEEL_SCALE;   // space between the outlines of neighbours
	constexpr float HEX_INRADIUS = HEX_PITCH / 2.f - HEX_GAP / 2.f;
	constexpr float HEX_RADIUS = HEX_INRADIUS * 2.f / 1.7320508f;  // corner distance, flat-top hexagon
	constexpr float HEX_POINT_RADIUS = HEX_PITCH * 0.55f;           // how close the hand has to be to a hexagon's center to point at it

	constexpr float WHEEL_DISTANCE = 3.f;  // how far the wheel plane is in front of the hand, about 8 cm at the default world scale
	constexpr float WHEEL_HOVER_HAPTIC_INTENSITY = 0.3f;

	// Hexagon fill in the HUD color: a barely visible tint, and for the hovered one a gradient from the center to the edges
	constexpr float TILE_TINT_ALPHA = 0.12f;
	constexpr float HOVER_CENTER_ALPHA = 0.6f;
	constexpr float HOVER_EDGE_ALPHA = 0.1f;

	// Hexagon outline
	constexpr float OUTLINE_THICKNESS = 0.1f * WHEEL_SCALE;
	constexpr float OUTLINE_HOVER_THICKNESS = 0.15f * WHEEL_SCALE;
	constexpr float OUTLINE_COLOR_SCALE = 0.8f;
	constexpr float OUTLINE_HOVER_COLOR_SCALE = 1.f;

	// Weapon icons are 170x45 sprite rects, this is how many HL units one sprite pixel covers
	constexpr float ICON_UNITS_PER_PIXEL = 2.8f / 170.f * WHEEL_SCALE;
	constexpr int ICON_INACTIVE_BRIGHTNESS = 192;  // same as the HL weapon list
	constexpr int EMPTY_BRIGHTNESS = 128;          // weapons without ammo, same as the HL weapon list

	// Pixels below this brightness count as empty when looking for the visible part of an icon
	constexpr int ICON_VISIBLE_THRESHOLD = 40;

	// The HUD sheets have no alpha, their background is dark grey (about 21-29) with scanlines.
	// Brightness up to this value is treated as transparent, the rest is scaled to 0..1 alpha.
	constexpr int ICON_BACKGROUND_FLOOR = 30;

	constexpr float PI = 3.14159265358979f;

	// One hexagon of the wheel
	struct Cell
	{
		float x, y;       // offset from the center of the wheel, in the wheel plane
		WEAPON* weapon;   // null for the empty hand in the center
		bool empty;       // the player has the weapon but no ammo for it, it can't be selected
	};

	// Hover key of the cell the hand points at: the weapon id, 0 for the empty hand, -1 for none
	constexpr int HOVER_NONE = -1;
	constexpr int HOVER_HAND = 0;

	bool g_isOpen = false;
	int g_lastHover = HOVER_NONE;
	bool g_leftCenter = false;  // the hand has been somewhere else than the center since the wheel opened

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
	void DrawSprite(HSPRITE sprite, const Rect& rc, const Vector& position, float unitsPerPixel, int r, int g, int b)
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

		const VisibleBounds visible = GetVisibleBounds(texture, textureWidth, textureHeight, sheet, rc);

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
			DrawSprite(sprite, *rect, position, ICON_UNITS_PER_PIXEL, r, g, b);
	}

	// The weapon with this id if the player has it
	WEAPON* FindWeapon(int id)
	{
		WEAPON* weapon = gWR.GetWeapon(id);
		if (weapon->iId != 0 && gHUD.HasWeapon(weapon->iId))
			return weapon;

		return nullptr;
	}

	// The hexagons that are there right now: the empty hand in the center (always first), then the
	// weapons the player has, spoke by spoke, including the ones without ammo. Returns how many there are.
	int BuildCells(Cell cells[MAX_CELLS])
	{
		int count = 0;
		cells[count++] = Cell{ 0.f, 0.f, nullptr, false };

		for (int spoke = 0; spoke < NUM_SPOKES; spoke++)
		{
			// Spoke 0 points up, going clockwise
			const float angle = PI / 2.f - spoke * (PI / 3.f);
			int placed = 0;
			for (int i = 0; i < SPOKE_LENGTH; i++)
			{
				if (SPOKES[spoke][i] == 0)
					continue;

				WEAPON* weapon = FindWeapon(SPOKES[spoke][i]);
				if (weapon == nullptr)
					continue;

				const float distance = ++placed * HEX_PITCH;
				cells[count++] = Cell{ cosf(angle) * distance, sinf(angle) * distance, weapon, !gWR.HasAmmo(weapon) };
			}
		}

		return count;
	}

	Vector GetCellPosition(const Cell& cell)
	{
		return g_center + g_right * cell.x + g_up * cell.y;
	}

	// The selectable cell closest to the weapon hand, -1 if the hand is not on any
	int GetHoveredCell(const Cell* cells, int count)
	{
		VRHelper* helper = gVRRenderer.GetHelper();
		if (helper == nullptr)
			return -1;

		const Vector offset = helper->GetWeaponPosition() - g_center;
		const float x = DotProduct(offset, g_right);
		const float y = DotProduct(offset, g_up);

		int hovered = -1;
		float closest = HEX_POINT_RADIUS * HEX_POINT_RADIUS;
		for (int i = 0; i < count; i++)
		{
			if (cells[i].empty)
				continue;

			const float dx = x - cells[i].x;
			const float dy = y - cells[i].y;
			const float distanceSquared = dx * dx + dy * dy;
			if (distanceSquared < closest)
			{
				closest = distanceSquared;
				hovered = i;
			}
		}

		return hovered;
	}

	int GetHoverKey(const Cell* cells, int hovered)
	{
		if (hovered == -1)
			return HOVER_NONE;

		return cells[hovered].weapon != nullptr ? cells[hovered].weapon->iId : HOVER_HAND;
	}

	// Corner of a flat-top hexagon
	Vector GetHexCorner(const Vector& center, int corner)
	{
		const float angle = corner * (PI / 3.f);
		return center + g_right * (cosf(angle) * HEX_RADIUS) + g_up * (sinf(angle) * HEX_RADIUS);
	}

	// Draws a hexagon, with the alpha going from centerAlpha in the middle to edgeAlpha at the corners
	void DrawHexFill(const Vector& center, int r, int g, int b, float centerAlpha, float edgeAlpha)
	{
		glBegin(GL_TRIANGLE_FAN);
		glColor4f(r / 255.f, g / 255.f, b / 255.f, centerAlpha);
		glVertex3f(center.x, center.y, center.z);
		glColor4f(r / 255.f, g / 255.f, b / 255.f, edgeAlpha);
		for (int corner = 0; corner <= 6; corner++)
		{
			const Vector point = GetHexCorner(center, corner % 6);
			glVertex3f(point.x, point.y, point.z);
		}
		glEnd();
	}

	// Draws the outline of a hexagon inside its edges, in the current color
	void DrawHexOutline(const Vector& center, float thickness)
	{
		glBegin(GL_QUADS);
		for (int corner = 0; corner < 6; corner++)
		{
			const Vector from = GetHexCorner(center, corner);
			const Vector to = GetHexCorner(center, (corner + 1) % 6);
			const Vector inward = ((center - (from + to) * 0.5f).Normalize()) * thickness;
			const Vector points[4] = { from, to, to + inward, from + inward };
			for (const Vector& point : points)
				glVertex3f(point.x, point.y, point.z);
		}
		glEnd();
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
			// Away from the head, so the hand is in front of the wheel
			g_center = helper->GetWeaponPosition() + forward * WHEEL_DISTANCE;
			g_isOpen = true;
			g_lastHover = HOVER_HAND;  // the hand starts on the center hexagon
			g_leftCenter = false;
		}

		void Close()
		{
			g_isOpen = false;
		}

		Selection CloseAndGetSelection()
		{
			Selection selection;
			if (g_isOpen)
			{
				Cell cells[MAX_CELLS];
				const int count = BuildCells(cells);
				const int hovered = GetHoveredCell(cells, count);
				if (hovered != -1)
				{
					if (cells[hovered].weapon != nullptr)
					{
						selection.weapon = cells[hovered].weapon;
					}
					else if (g_leftCenter)
					{
						// Releasing on the center without having moved is just closing the wheel, not holstering
						selection.holster = true;
					}
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
			// so also make the test always pass and don't write depth. Otherwise the coplanar sprites z-fight with the hexagons.
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

			Cell cells[MAX_CELLS];
			const int count = BuildCells(cells);
			const int hovered = GetHoveredCell(cells, count);

			const int hoverKey = GetHoverKey(cells, hovered);
			if (hoverKey != g_lastHover)
			{
				// Render runs once per eye, the second call sees no change
				g_lastHover = hoverKey;
				if (hovered != -1)
				{
					// Not PlaySound() from cl_util.h: windows.h (via vr_gl.h) turns that name into PlaySoundA
					gEngfuncs.pfnPlaySoundByName("common/wpn_moveselect.wav", 1);
					g_vrInput.recoilintensity = WHEEL_HOVER_HAPTIC_INTENSITY;
				}
			}
			if (hovered != 0)
				g_leftCenter = true;

			// Weapons without ammo are dim red, like in the HL weapon list
			int emptyR, emptyG, emptyB;
			UnpackRGB(emptyR, emptyG, emptyB, RGB_REDISH);
			ScaleColors(emptyR, emptyG, emptyB, EMPTY_BRIGHTNESS);

			// Translucent fill
			for (int i = 0; i < count; i++)
			{
				const int r = cells[i].empty ? emptyR : hudR;
				const int g = cells[i].empty ? emptyG : hudG;
				const int b = cells[i].empty ? emptyB : hudB;
				if (i == hovered)
					DrawHexFill(GetCellPosition(cells[i]), r, g, b, HOVER_CENTER_ALPHA, HOVER_EDGE_ALPHA);
				else
					DrawHexFill(GetCellPosition(cells[i]), r, g, b, TILE_TINT_ALPHA, TILE_TINT_ALPHA);
			}

			// Outlines on top, opaque so the corners where the lines overlap don't get brighter
			for (int i = 0; i < count; i++)
			{
				const bool isHovered = i == hovered;
				const float scale = isHovered ? OUTLINE_HOVER_COLOR_SCALE : OUTLINE_COLOR_SCALE;
				const int r = cells[i].empty ? emptyR : hudR;
				const int g = cells[i].empty ? emptyG : hudG;
				const int b = cells[i].empty ? emptyB : hudB;
				glColor4f(r / 255.f * scale, g / 255.f * scale, b / 255.f * scale, 1.f);
				DrawHexOutline(GetCellPosition(cells[i]), isHovered ? OUTLINE_HOVER_THICKNESS : OUTLINE_THICKNESS);
			}

			// Sprites are additive, like everything else on the HL HUD. The empty hand has no icon, only its hexagon.
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			for (int i = 0; i < count; i++)
			{
				if (cells[i].weapon == nullptr)
					continue;

				if (cells[i].empty)
					DrawWeaponIcon(cells[i].weapon, GetCellPosition(cells[i]), false, emptyR, emptyG, emptyB);
				else
					DrawWeaponIcon(cells[i].weapon, GetCellPosition(cells[i]), i == hovered, hudR, hudG, hudB);
			}

			glPopAttrib();
		}
	}  // namespace WeaponWheel
}  // namespace VR
