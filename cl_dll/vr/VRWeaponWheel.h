#pragma once

struct WEAPON;

namespace VR
{
	namespace WeaponWheel
	{
		struct Selection
		{
			bool holster = false;
			WEAPON* weapon = nullptr;
		};


		// Opens the wheel around the weapon hand, facing the head
		void Open();
		// Closes without selecting anything
		void Close();
		// Closes and returns what the weapon hand points at: a weapon, the empty hand (holster), or nothing
		Selection CloseAndGetSelection();
		bool IsOpen();

		// Draws the wheel (call while rendering the HUD)
		void Render();
	}  // namespace WeaponWheel
}  // namespace VR
