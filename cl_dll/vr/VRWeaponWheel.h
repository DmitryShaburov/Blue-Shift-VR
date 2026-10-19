#pragma once

namespace VR
{
	namespace WeaponWheel
	{
		// Opens the wheel around the weapon hand, facing the head
		void Open();
		void Close();
		bool IsOpen();

		// Draws the wheel (call while rendering the HUD)
		void Render();
	}  // namespace WeaponWheel
}  // namespace VR
