#pragma once

class VRDebugBBoxDrawer
{
public:

	~VRDebugBBoxDrawer();

	void DrawBBoxes(EHandleT<CBaseEntity> hEntity, bool mirrored = false);
	void DrawPoint(EHandleT<CBaseEntity> hEntity, const Vector& point);
	void DrawLine(EHandleT<CBaseEntity> hEntity, const Vector& start, const Vector& end);
	void Clear(EHandleT<CBaseEntity> hEntity);
	void ClearBoxes(EHandleT<CBaseEntity> hEntity);
	void ClearPoint(EHandleT<CBaseEntity> hEntity);
	void ClearLine(EHandleT<CBaseEntity> hEntity);
	void ClearAllBut(EHandleT<CBaseEntity> hEntity);
	void ClearAll();

	inline void SetColor(int r, int g, int b) { m_r = r; m_g = g; m_b = b; }

private:

	int m_r = 255;
	int m_g = 0;
	int m_b = 0;

	std::unordered_map<EHandleT<CBaseEntity>, std::vector<EHandleT<CBaseEntity>>, EHandleT<CBaseEntity>::Hash, EHandleT<CBaseEntity>::Equal> m_bboxes;
	std::unordered_map<EHandleT<CBaseEntity>, EHandleT<CBaseEntity>, EHandleT<CBaseEntity>::Hash, EHandleT<CBaseEntity>::Equal> m_points;
	std::unordered_map<EHandleT<CBaseEntity>, EHandleT<CBaseEntity>, EHandleT<CBaseEntity>::Hash, EHandleT<CBaseEntity>::Equal> m_lines;
};

