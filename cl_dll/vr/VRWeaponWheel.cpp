#include <algorithm>
#include <cmath>
#include <map>
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

extern cl_entity_t* SaveGetLocalPlayer();

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

	constexpr float SQRT3 = 1.7320508f;

	constexpr float HEX_PITCH = 3.f * WHEEL_SCALE;  // distance between the centers of neighbouring hexagons, about 8 cm
	constexpr float HEX_GAP = 0.2f * WHEEL_SCALE;   // space between the outlines of neighbours
	constexpr float HEX_INRADIUS = HEX_PITCH / 2.f - HEX_GAP / 2.f;
	constexpr float HEX_RADIUS = HEX_INRADIUS * 2.f / SQRT3;  // corner distance, flat-top hexagon
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

	// No cell is hovered
	constexpr int CELL_NONE = -1;

	// Hover key of the cell the hand points at: the weapon id, HOVER_HAND for the empty hand, HOVER_NONE for none.
	// Weapon ids start at 1, so they don't clash with HOVER_HAND.
	constexpr int HOVER_NONE = -1;
	constexpr int HOVER_HAND = 0;

	struct WheelState
	{
		bool isOpen = false;
		int lastHover = HOVER_NONE;
		bool leftCenter = false;  // the hand has been somewhere else than the center since the wheel opened

		// The hexagons and the one the hand points at, refreshed once per frame
		Cell cells[MAX_CELLS];
		int cellCount = 0;
		int hovered = CELL_NONE;

		// Fixed when the wheel is opened: relative to the player's feet and without the world yaw (turning, trains),
		// the same room space the hands are tracked in, so the wheel moves with the player on elevators and trains
		Vector roomCenter;
		Vector roomRight;
		Vector roomUp;

		// The above in the world, refreshed once per frame
		Vector center;
		Vector right;
		Vector up;
	};
	WheelState g_wheel;

	// HL coordinates of the HUD sprites are in the size of the original sprite sheet.
	// Keyed by the sprite model name. Weapon sprites are always the 640 sheets, the 320 ones are only used
	// when the screen is narrower than 640.
	struct SheetInfo
	{
		int width;
		int height;
		const char* texture;
	};
	const std::unordered_map<std::string, SheetInfo> g_sheets = {
		{ "sprites/640hud1.spr", { 256, 256, "/hud/640hud1.png" } },
		{ "sprites/640hud2.spr", { 256, 256, "/hud/640hud2.png" } },
		{ "sprites/640hud3.spr", { 256, 256, "/hud/640hud3.png" } },
		{ "sprites/640hud4.spr", { 256, 256, "/hud/640hud4.png" } },
		{ "sprites/640hud5.spr", { 256, 256, "/hud/640hud5.png" } },
		{ "sprites/640hud6.spr", { 256, 256, "/hud/640hud6.png" } },
		{ "sprites/640hud7.spr", { 256, 128, "/hud/640hud7.png" } },
		{ "sprites/640hud8.spr", { 256, 64, "/hud/640hud8.png" } },
		{ "sprites/640hud9.spr", { 256, 64, "/hud/640hud9.png" } },
	};

	struct VisibleBounds
	{
		float left, right, top, bottom;  // in sprite pixels, relative to the sheet
	};

	// What is kept of a sprite sheet texture. It is read back from the GPU once, when the sheet is first used.
	struct SheetData
	{
		// Copy of the sheet that is white with alpha from the pixel brightness, so the dark background
		// (with scanlines) doesn't show up as a lighter rectangle. The sprite color is applied when drawing.
		unsigned int maskedTexture = 0;
		std::vector<unsigned char> brightness;  // per pixel, to find the visible part of an icon
	};

	const SheetData& GetSheetData(unsigned int texture, int textureWidth, int textureHeight)
	{
		static std::unordered_map<unsigned int, SheetData> cache;

		auto it = cache.find(texture);
		if (it != cache.end())
			return it->second;

		std::vector<unsigned char> pixels(static_cast<size_t>(textureWidth) * textureHeight * 4);
		glBindTexture(GL_TEXTURE_2D, texture);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		SheetData& data = cache[texture];
		data.brightness.resize(static_cast<size_t>(textureWidth) * textureHeight);
		for (size_t i = 0; i < data.brightness.size(); i++)
		{
			unsigned char* p = &pixels[i * 4];
			const int brightness = (std::max)({ p[0], p[1], p[2] });
			const int alpha = (std::max)(0, brightness - ICON_BACKGROUND_FLOOR) * 255 / (255 - ICON_BACKGROUND_FLOOR);
			data.brightness[i] = static_cast<unsigned char>(brightness);
			p[0] = p[1] = p[2] = 255;
			p[3] = static_cast<unsigned char>(alpha);
		}

		glGenTextures(1, &data.maskedTexture);
		glBindTexture(GL_TEXTURE_2D, data.maskedTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		return data;
	}

	// Returns the part of the rect that actually has something drawn, so icons can be centered
	// (the weapon sprites are right aligned in their rects)
	VisibleBounds GetVisibleBounds(const SheetData& data, int textureWidth, int textureHeight, const SheetInfo& sheet, const Rect& rc)
	{
		static std::map<std::tuple<unsigned int, int, int, int, int>, VisibleBounds> cache;

		const auto key = std::make_tuple(data.maskedTexture, rc.left, rc.top, rc.right, rc.bottom);
		auto it = cache.find(key);
		if (it != cache.end())
			return it->second;

		VisibleBounds bounds{ static_cast<float>(rc.left), static_cast<float>(rc.right), static_cast<float>(rc.top), static_cast<float>(rc.bottom) };

		const float scaleX = static_cast<float>(textureWidth) / sheet.width;
		const float scaleY = static_cast<float>(textureHeight) / sheet.height;
		int minX = textureWidth, maxX = -1, minY = textureHeight, maxY = -1;
		for (int y = static_cast<int>(rc.top * scaleY); y < static_cast<int>(rc.bottom * scaleY) && y < textureHeight; y++)
		{
			for (int x = static_cast<int>(rc.left * scaleX); x < static_cast<int>(rc.right * scaleX) && x < textureWidth; x++)
			{
				if (data.brightness[static_cast<size_t>(y) * textureWidth + x] > ICON_VISIBLE_THRESHOLD)
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
	void DrawSprite(HSPRITE sprite, const Rect& rc, const Vector& position, float unitsPerPixel, int r, int g, int b)
	{
		const model_s* model = gEngfuncs.GetSpritePointer(sprite);
		if (model == nullptr)
			return;

		const auto sheetIt = g_sheets.find(model->name);
		if (sheetIt == g_sheets.end())
			return;
		const SheetInfo& sheet = sheetIt->second;

		unsigned int textureWidth = 0, textureHeight = 0;
		const unsigned int texture = VRTextureHelper::Instance().GetTexture(sheet.texture, textureWidth, textureHeight);
		if (texture == 0)
			return;

		const SheetData& sheetData = GetSheetData(texture, textureWidth, textureHeight);
		const VisibleBounds visible = GetVisibleBounds(sheetData, textureWidth, textureHeight, sheet, rc);

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
			const Vector pos = position + g_wheel.right * x + g_wheel.up * y;
			glTexCoord2f(u, v);
			glVertex3f(pos.x, pos.y, pos.z);
		};

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, sheetData.maskedTexture);
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
		return g_wheel.center + g_wheel.right * cell.x + g_wheel.up * cell.y;
	}

	// The selectable cell closest to the weapon hand, CELL_NONE if the hand is not on any
	int GetHoveredCell(const Cell* cells, int count)
	{
		VRHelper* helper = gVRRenderer.GetHelper();
		if (helper == nullptr)
			return CELL_NONE;

		const Vector offset = helper->GetWeaponPosition() - g_wheel.center;
		const float x = DotProduct(offset, g_wheel.right);
		const float y = DotProduct(offset, g_wheel.up);

		int hovered = CELL_NONE;
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
		if (hovered == CELL_NONE)
			return HOVER_NONE;

		return cells[hovered].weapon != nullptr ? cells[hovered].weapon->iId : HOVER_HAND;
	}

	// The player's feet, the point the hands are tracked from (GetPositionInHLSpaceFromAbsoluteTrackingMatrix).
	// Not the origin: it moves when the hull changes (crouching), the hands and the ground don't.
	Vector GetPlayerGround()
	{
		cl_entity_t* localPlayer = SaveGetLocalPlayer();
		if (localPlayer == nullptr)
			return Vector{};

		Vector ground = localPlayer->curstate.origin;
		ground.z += localPlayer->curstate.mins.z;
		return ground;
	}

	// Rotates around the vertical axis, like the world yaw rotates the tracked positions
	Vector RotateYaw(const Vector& v, float degrees)
	{
		const float c = cosf(degrees * PI / 180.f);
		const float s = sinf(degrees * PI / 180.f);
		return Vector{ v.x * c - v.y * s, v.x * s + v.y * c, v.z };
	}

	void UpdateWorldPosition()
	{
		VRHelper* helper = gVRRenderer.GetHelper();
		if (helper == nullptr)
			return;

		const float yaw = helper->GetWorldYaw();
		g_wheel.center = GetPlayerGround() + RotateYaw(g_wheel.roomCenter, yaw);
		g_wheel.right = RotateYaw(g_wheel.roomRight, yaw);
		g_wheel.up = RotateYaw(g_wheel.roomUp, yaw);
	}

	void Refresh()
	{
		UpdateWorldPosition();
		g_wheel.cellCount = BuildCells(g_wheel.cells);
		g_wheel.hovered = GetHoveredCell(g_wheel.cells, g_wheel.cellCount);
	}

	// Corner of a flat-top hexagon
	Vector GetHexCorner(const Vector& center, int corner)
	{
		const float angle = corner * (PI / 3.f);
		return center + g_wheel.right * (cosf(angle) * HEX_RADIUS) + g_wheel.up * (sinf(angle) * HEX_RADIUS);
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

	// Adds the outline of a hexagon inside its edges, in the current color. Call between glBegin(GL_QUADS) and glEnd().
	void AddHexOutline(const Vector& center, float thickness)
	{
		for (int corner = 0; corner < 6; corner++)
		{
			const Vector from = GetHexCorner(center, corner);
			const Vector to = GetHexCorner(center, (corner + 1) % 6);
			const Vector inward = ((center - (from + to) * 0.5f).Normalize()) * thickness;
			const Vector points[4] = { from, to, to + inward, from + inward };
			for (const Vector& point : points)
				glVertex3f(point.x, point.y, point.z);
		}
	}
	struct Color
	{
		int r, g, b;
	};

	struct HudColors
	{
		Color normal;
		Color empty;  // weapons without ammo
	};

	HudColors GetHudColors()
	{
		HudColors colors;
		// Blue Shift's HUD color (named RGB_YELLOWISH in the HL code)
		UnpackRGB(colors.normal.r, colors.normal.g, colors.normal.b, RGB_YELLOWISH);

		// Weapons without ammo are dim red, like in the HL weapon list
		UnpackRGB(colors.empty.r, colors.empty.g, colors.empty.b, RGB_REDISH);
		ScaleColors(colors.empty.r, colors.empty.g, colors.empty.b, EMPTY_BRIGHTNESS);
		return colors;
	}

	const Color& GetCellColor(const Cell& cell, const HudColors& colors)
	{
		return cell.empty ? colors.empty : colors.normal;
	}

	void SetRenderState()
	{
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
	}

	// Translucent fill
	void DrawFills(const HudColors& colors)
	{
		for (int i = 0; i < g_wheel.cellCount; i++)
		{
			const Cell& cell = g_wheel.cells[i];
			const Color& color = GetCellColor(cell, colors);
			if (i == g_wheel.hovered)
				DrawHexFill(GetCellPosition(cell), color.r, color.g, color.b, HOVER_CENTER_ALPHA, HOVER_EDGE_ALPHA);
			else
				DrawHexFill(GetCellPosition(cell), color.r, color.g, color.b, TILE_TINT_ALPHA, TILE_TINT_ALPHA);
		}
	}

	// Outlines on top, opaque so the corners where the lines overlap don't get brighter
	void DrawOutlines(const HudColors& colors)
	{
		glBegin(GL_QUADS);
		for (int i = 0; i < g_wheel.cellCount; i++)
		{
			const Cell& cell = g_wheel.cells[i];
			const bool isHovered = i == g_wheel.hovered;
			const float scale = isHovered ? OUTLINE_HOVER_COLOR_SCALE : OUTLINE_COLOR_SCALE;
			const Color& color = GetCellColor(cell, colors);
			glColor4f(color.r / 255.f * scale, color.g / 255.f * scale, color.b / 255.f * scale, 1.f);
			AddHexOutline(GetCellPosition(cell), isHovered ? OUTLINE_HOVER_THICKNESS : OUTLINE_THICKNESS);
		}
		glEnd();
	}

	// Sprites are additive, like everything else on the HL HUD. The empty hand has no icon, only its hexagon.
	void DrawIcons(const HudColors& colors)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for (int i = 0; i < g_wheel.cellCount; i++)
		{
			const Cell& cell = g_wheel.cells[i];
			if (cell.weapon == nullptr)
				continue;

			const Color& color = GetCellColor(cell, colors);
			DrawWeaponIcon(cell.weapon, GetCellPosition(cell), !cell.empty && i == g_wheel.hovered, color.r, color.g, color.b);
		}
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

			Vector forward, right, up;
			helper->GetViewVectors(forward, right, up);
			// Away from the head, so the hand is in front of the wheel
			const Vector center = helper->GetWeaponPosition() + forward * WHEEL_DISTANCE;

			const float yaw = helper->GetWorldYaw();
			g_wheel.roomCenter = RotateYaw(center - GetPlayerGround(), -yaw);
			g_wheel.roomRight = RotateYaw(right, -yaw);
			g_wheel.roomUp = RotateYaw(up, -yaw);
			g_wheel.isOpen = true;
			Refresh();
			g_wheel.lastHover = HOVER_HAND;  // the hand starts on the center hexagon
			g_wheel.leftCenter = false;
		}

		void Close()
		{
			g_wheel.isOpen = false;
		}

		Selection CloseAndGetSelection()
		{
			Selection selection;
			if (g_wheel.isOpen)
			{
				// The release comes before this frame's Update()
				Refresh();
				if (g_wheel.hovered != CELL_NONE)
				{
					if (g_wheel.cells[g_wheel.hovered].weapon != nullptr)
					{
						selection.weapon = g_wheel.cells[g_wheel.hovered].weapon;
					}
					else if (g_wheel.leftCenter)
					{
						// Releasing on the center without having moved is just closing the wheel, not holstering
						selection.holster = true;
					}
				}
			}

			g_wheel.isOpen = false;
			return selection;
		}

		void Update()
		{
			if (!g_wheel.isOpen)
				return;

			Refresh();

			const int hoverKey = GetHoverKey(g_wheel.cells, g_wheel.hovered);
			if (hoverKey != g_wheel.lastHover)
			{
				g_wheel.lastHover = hoverKey;
				if (g_wheel.hovered != CELL_NONE)
				{
					// Not PlaySound() from cl_util.h: windows.h (via vr_gl.h) turns that name into PlaySoundA
					gEngfuncs.pfnPlaySoundByName("common/wpn_moveselect.wav", 1);
					g_vrInput.recoilintensity = WHEEL_HOVER_HAPTIC_INTENSITY;
				}
			}
			// Any movement off the center counts, including pointing at nothing
			if (g_wheel.hovered != 0)
				g_wheel.leftCenter = true;
		}

		void Render()
		{
			if (!g_wheel.isOpen)
				return;

			glPushAttrib(GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);
			SetRenderState();

			const HudColors colors = GetHudColors();
			DrawFills(colors);
			DrawOutlines(colors);
			DrawIcons(colors);

			glPopAttrib();
		}
	}  // namespace WeaponWheel
}  // namespace VR
