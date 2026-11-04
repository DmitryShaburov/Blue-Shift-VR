
#include <algorithm>

#include "hud.h"
#include "const.h"

#include "screenfade.h"
#include "shake.h"

#include "r_studioint.h"

#include "VRRenderer.h"
#include "VRHelper.h"
#include "VROpenGLInterceptor.h"
#include "VRInput.h"

#include "vr_gl.h"


extern engine_studio_api_t IEngineStudio;


void VRRenderer::DrawScreenFade()
{
	screenfade_t screenfade{0};
	gEngfuncs.pfnGetScreenFade(&screenfade);

	if (screenfade.fadeReset == 0.f && screenfade.fadeEnd == 0.f)
		return;

	if ((m_hudRedrawTime > screenfade.fadeReset) && (m_hudRedrawTime > screenfade.fadeEnd))
		return;

	glPushAttrib(GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if ((screenfade.fadeFlags & FFADE_MODULATE) != 0)
	{
		IEngineStudio.GL_SetRenderMode(kRenderTransAdd);
	}
	else
	{
		IEngineStudio.GL_SetRenderMode(kRenderTransTexture);
	}

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	float fadealpha = 0.f;
	if ((screenfade.fadeFlags & FFADE_STAYOUT) != 0)
	{
		fadealpha = screenfade.fadealpha;
	}
	else
	{
		if ((screenfade.fadeFlags & FFADE_OUT) != 0)
		{
			fadealpha = screenfade.fadealpha - fabs(screenfade.fadeSpeed * (screenfade.fadeEnd - m_hudRedrawTime));
		}
		else
		{
			fadealpha = fabs(screenfade.fadeSpeed * (screenfade.fadeEnd - m_hudRedrawTime));
		}
		fadealpha = std::clamp(fadealpha, 0.f, float(screenfade.fadealpha));
	}

	glColor4f(screenfade.fader / 255.f, screenfade.fadeg / 255.f, screenfade.fadeb / 255.f, fadealpha / 255.f);

	glBegin(GL_QUADS);
	glVertex2f(-1.f, -1.f);
	glVertex2f(1.f, -1.f);
	glVertex2f(1.f, 1.f);
	glVertex2f(-1.f, 1.f);
	glEnd();

	glColor4f(0.f, 0.f, 0.f, 0.f);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();
}

// BSVR start
// Covers the current eye's view with black at the given opacity (0..1)
void VRRenderer::DarkenScreen(float opacity)
{
	HLVR_UnlockGLMatrices();

	glPushAttrib(GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT | GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glColor4f(0.f, 0.f, 0.f, opacity);

	glBegin(GL_QUADS);
	glVertex2f(-1.f, -1.f);
	glVertex2f(1.f, -1.f);
	glVertex2f(1.f, 1.f);
	glVertex2f(-1.f, 1.f);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glPopAttrib();

	HLVR_LockGLMatrices();
}
// BSVR end

void VRRenderer::RenderScreenOverlays()
{
	//hlvrUnlockGLMatrices();
	HLVR_UnlockGLMatrices();

	DrawScreenFade();

	glPushAttrib(GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT | GL_TEXTURE_BIT);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// reset everytime
	g_vrInput.damageintensity = 0.f;

	// draw damage unless when being healed
	// damage is drawn as pulsating red gradient overlay using a sine wave (well actually negative cosine so we start at 0, but who cares)
	bool hasDamage = gHUD.m_Health.m_fAttackFront > 0 || gHUD.m_Health.m_fAttackRear > 0 || gHUD.m_Health.m_fAttackLeft > 0 || gHUD.m_Health.m_fAttackRight > 0 || gHUD.m_Health.m_iHealth <= 15 || 0 != gHUD.m_Health.m_bitsDamage || (0 != gHUD.m_Health.m_fFade && gHUD.m_Health.m_healthLost);

	bool isBeingHealed = 0 != gHUD.m_Health.m_fFade && gHUD.m_Health.m_healthGained;

	if ((hasDamage || m_isDrawingDamage) && !isBeingHealed)
	{
		float brightness = 0.f;

		if (!m_isDrawingDamage)
		{
			m_flDrawingDamageTime = gHUD.m_flTime;
			m_isDrawingDamage = true;
			brightness = 0.f;
		}
		else
		{
			constexpr const float speed = 2.f;
			brightness = (-cos((gHUD.m_flTime - m_flDrawingDamageTime) * speed) + 1.f) * 0.5f;
		}

		// recalculate damage color everytime we fade out
		if (brightness < 0.01f)
		{
			if ((gHUD.m_Health.m_bitsDamage & DMG_DROWN) != 0)
			{
				// dark blue
				m_damageColor = Vector{0.2f, 0.3f, 0.7f};
			}
			else if ((gHUD.m_Health.m_bitsDamage & (DMG_FREEZE | DMG_SLOWFREEZE)) != 0)
			{
				// light blue
				m_damageColor = Vector{0.4f, 0.7f, 1.f};
			}
			else if ((gHUD.m_Health.m_bitsDamage & (DMG_POISON | DMG_ACID | DMG_NERVEGAS | DMG_RADIATION)) != 0)
			{
				// green
				m_damageColor = Vector{0.4f, 0.8f, 0.f};
			}
			else
			{
				// any other damage is red
				m_damageColor = Vector{1.f, 0.f, 0.f};
			}
		}

		if (brightness < 0.01f && !hasDamage)
		{
			// we were just drawing the last "fade out"
			m_isDrawingDamage = false;
		}
		else
		{
			Vector viewOrg;
			Vector viewAngles;
			vrHelper->GetViewOrg(viewOrg);
			vrHelper->GetViewAngles(vr::EVREye::Eye_Left, viewAngles);

			Vector topleftfront{8.f, -32.f, 32.f};
			Vector toprightfront{8.f, 32.f, 32.f};
			Vector bottomleftfront{8.f, -32.f, -32.f};
			Vector bottomrightfront{8.f, 32.f, -32.f};
			Vector centerfront{8.f, 0.f, 0.f};
			Vector topleftback{-8.f, topleftfront.y * 100.f, topleftfront.z * 100.f};
			Vector toprightback{-8.f, toprightfront.y * 100.f, toprightfront.z * 100.f};
			Vector bottomleftback{-8.f, bottomleftfront.y * 100.f, bottomleftfront.z * 100.f};
			Vector bottomrightback{-8.f, bottomrightfront.y * 100.f, bottomrightfront.z * 100.f};

			RotateVector(topleftfront, viewAngles);
			RotateVector(toprightfront, viewAngles);
			RotateVector(bottomleftfront, viewAngles);
			RotateVector(bottomrightfront, viewAngles);
			RotateVector(centerfront, viewAngles);
			RotateVector(topleftback, viewAngles);
			RotateVector(toprightback, viewAngles);
			RotateVector(bottomleftback, viewAngles);
			RotateVector(bottomrightback, viewAngles);

			float color[4];
			m_damageColor.CopyToArray(color);
			color[3] = brightness;

			// good old legacy opengl xD

			// gradient triangles forming a rectangular overlay in front of player that goes from solid red on the border to transparent in the center
			glBegin(GL_TRIANGLES);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + toprightfront);
			glColor4f(0.f, 0.f, 0.f, 0.f);
			glVertex3fv(viewOrg + centerfront);

			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftfront);
			glColor4f(0.f, 0.f, 0.f, 0.f);
			glVertex3fv(viewOrg + centerfront);

			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftfront);
			glColor4f(0.f, 0.f, 0.f, 0.f);
			glVertex3fv(viewOrg + centerfront);

			glColor4fv(color);
			glVertex3fv(viewOrg + toprightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightfront);
			glColor4f(0.f, 0.f, 0.f, 0.f);
			glVertex3fv(viewOrg + centerfront);
			glEnd();

			// solid quads that go behind the vieworg to "close" any holes in ultra-wide fov headsets of the future or whatever
			glBegin(GL_QUADS);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + toprightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + toprightback);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftback);

			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + topleftback);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftback);

			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomleftback);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightback);

			glColor4fv(color);
			glVertex3fv(viewOrg + toprightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightfront);
			glColor4fv(color);
			glVertex3fv(viewOrg + bottomrightback);
			glColor4fv(color);
			glVertex3fv(viewOrg + toprightback);
			glEnd();
		}

		if (m_isDrawingDamage)
		{
			g_vrInput.damageintensity = brightness;
		}
	}
	else
	{
		m_isDrawingDamage = false;
	}

	glPopAttrib();

	//hlvrLockGLMatrices();
	HLVR_LockGLMatrices();
}
