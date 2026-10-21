#include <cmath>

#include "hud.h"
#include "cl_util.h"

#include "VRWeaponWheel.h"
#include "VRRenderer.h"
#include "VRHelper.h"
#include "vr_gl.h"

namespace
{
	// Weapon categories, same as the original HL weapon slots
	constexpr int NUM_CATEGORIES = 5;

	// All sizes are in HL units
	constexpr float WHEEL_INNER_RADIUS = 3.f;
	constexpr float WHEEL_OUTER_RADIUS = 7.f;
	constexpr float WHEEL_SEGMENT_GAP_DEGREES = 3.f;
	constexpr int WHEEL_SEGMENT_STEPS = 12;
	constexpr float WHEEL_ALPHA = 0.6f;

	constexpr float PI = 3.14159265358979f;

	bool g_isOpen = false;
	bool g_renderPrinted = false; // TEMP

	// Fixed when the wheel is opened
	Vector g_center;
	Vector g_right;
	Vector g_up;
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
				gEngfuncs.Con_Printf("WeaponWheel: Open failed, no valid weapon controller\n"); // TEMP
				return;
			}

			if (g_isOpen)
				gEngfuncs.Con_Printf("WeaponWheel: Open called while already open (missed release?)\n"); // TEMP

			Vector forward;
			helper->GetViewVectors(forward, g_right, g_up);
			g_center = helper->GetWeaponPosition();
			g_isOpen = true;
			g_renderPrinted = false; // TEMP
			gEngfuncs.Con_Printf("WeaponWheel: opened at %.1f %.1f %.1f, right %.2f %.2f %.2f (len %.2f), up %.2f %.2f %.2f (len %.2f)\n", // TEMP
				g_center.x, g_center.y, g_center.z,
				g_right.x, g_right.y, g_right.z, g_right.Length(),
				g_up.x, g_up.y, g_up.z, g_up.Length());
		}

		void Close()
		{
			gEngfuncs.Con_Printf("WeaponWheel: Close (was open=%d)\n", (int)g_isOpen); // TEMP
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

			if (!g_renderPrinted) // TEMP
			{
				g_renderPrinted = true;
				gEngfuncs.Con_Printf("WeaponWheel: Render reached\n");
			}

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

			glColor4f(0.9f, 0.9f, 0.9f, WHEEL_ALPHA); // TEMP bright

			const float segmentAngle = 2.f * PI / NUM_CATEGORIES;
			const float gap = WHEEL_SEGMENT_GAP_DEGREES * PI / 180.f;

			for (int i = 0; i < NUM_CATEGORIES; i++)
			{
				// Category 0 is centered at the top, going clockwise
				const float centerAngle = PI / 2.f - i * segmentAngle;
				const float start = centerAngle - segmentAngle / 2.f + gap / 2.f;
				const float end = centerAngle + segmentAngle / 2.f - gap / 2.f;

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

			glPopAttrib();
		}
	}  // namespace WeaponWheel
}  // namespace VR
