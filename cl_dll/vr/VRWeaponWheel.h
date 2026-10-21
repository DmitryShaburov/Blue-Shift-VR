#pragma once

struct WEAPON;

namespace VR
{
	namespace WeaponWheel
	{

		// Opens the wheel around the weapon hand, facing the head
		void Open();
		// Closes without selecting anything
		void Close();
		// Closes and returns the weapon of the category the weapon hand points at, null if it points at none
		WEAPON* CloseAndGetSelection();
		bool IsOpen();

		// Draws the wheel (call while rendering the HUD)
		void Render();
	}  // namespace WeaponWheel
}  // namespace VR
