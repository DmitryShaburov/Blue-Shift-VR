#pragma once

// Bunch of simple classes, structs and other stuff - Max Makes Mods - 2019-04-07

#include "vector.h"
#include "Platform.h"

class GlobalXenMounds
{
public:
	void Add(const Vector& position, const string_t multi_manager);
	bool Trigger(CBasePlayer* pPlayer, const Vector& position);
	bool Has(const Vector& position);
	void Clear() { m_xen_mounds.clear(); }

private:
	std::map<const Vector, const string_t> m_xen_mounds;
};

class VRLevelChangeData
{
public:
	Vector lastHMDOffset{};
	Vector clientOriginOffset{}; // Must be Vector instead of Vector2D, so save/load works (only has FIELD_VECTOR, which expects 3 floats)
	bool hasData{false};
	float prevYaw{0.f};
	float currentYaw{0.f};
};
