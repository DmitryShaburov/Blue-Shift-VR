// Engine message wrappers that drop messages with a bogus type instead of crashing, moved from enginecallback.h

#pragma once

// Included from the middle of enginecallback.h; these two make the header parse on its own too (both are already included there)
#include "extdll.h"
#include "enginecallback.h"

namespace
{
bool _isBogusMessage = false;
}

inline void MESSAGE_BEGIN_IMPL(const char* msg_args, int msg_dest, int msg_type, const float* pOrigin = nullptr, edict_t* ed = nullptr)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Last message was bogus and not finished before new message!\n");
		_isBogusMessage = false;
	}

	// Very very very rarely for some reason the game might crash with
	// "Tried to create a message with a bogus message type ( 0 )".
	// We catch that here and log it instead of crashing.
	if (msg_type == 0)
	{
		_isBogusMessage = true;
		g_engfuncs.pfnAlertMessage(at_error, "Tried to send a bogus message, intercepted: %s\n", msg_args);
		if (pOrigin)
		{
			g_engfuncs.pfnAlertMessage(at_error, "Bogus message pOrigin: %f %f %f\n", pOrigin[0], pOrigin[1], pOrigin[2]);
		}
		if (ed)
		{
			g_engfuncs.pfnAlertMessage(at_error, "Bogus message ed\n");
		}
		return;
	}

	(*g_engfuncs.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ed);
}

inline void MESSAGE_BEGIN_IMPL(const char* msg_args, int msg_dest, int msg_type, const float* pOrigin, entvars_t* pev)
{
	MESSAGE_BEGIN_IMPL(msg_args, msg_dest, msg_type, pOrigin, pev->pContainingEntity);
}

#define MESSAGE_BEGIN(...) MESSAGE_BEGIN_IMPL(#__VA_ARGS__, __VA_ARGS__)

inline void MESSAGE_END()
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message end.\n");
		_isBogusMessage = false;
		return;
	}
	(*g_engfuncs.pfnMessageEnd)();
}
inline void WRITE_BYTE(int iValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message byte: %i\n", iValue);
		return;
	}
	(*g_engfuncs.pfnWriteByte)(iValue);
}
inline void WRITE_CHAR(int iValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message char: %i\n", iValue);
		return;
	}
	(*g_engfuncs.pfnWriteChar)(iValue);
}
inline void WRITE_SHORT(int iValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message short: %i\n", iValue);
		return;
	}
	(*g_engfuncs.pfnWriteShort)(iValue);
}
inline void WRITE_LONG(int iValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message long: %i\n", iValue);
		return;
	}
	(*g_engfuncs.pfnWriteLong)(iValue);
}
inline void WRITE_ANGLE(float flValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message angle: %f\n", flValue);
		return;
	}
	(*g_engfuncs.pfnWriteAngle)(flValue);
}
inline void WRITE_COORD(float flValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message coord: %f\n", flValue);
		return;
	}
	(*g_engfuncs.pfnWriteCoord)(flValue);
}
inline void WRITE_STRING(const char* sz)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message string: %s\n", sz);
		return;
	}
	(*g_engfuncs.pfnWriteString)(sz);
}
inline void WRITE_ENTITY(int iValue)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message entity: %i\n", iValue);
		return;
	}
	(*g_engfuncs.pfnWriteEntity)(iValue);
}
inline void WRITE_COORDS(const float* v)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message coords: %f %f %f\n", v[0], v[1], v[2]);
		return;
	}
	WRITE_COORD(v[0]);
	WRITE_COORD(v[1]);
	WRITE_COORD(v[2]);
}
inline void WRITE_FLOAT(float f)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message float: %f\n", f);
		return;
	}
	unsigned char floatbytes[sizeof(float)];
	memcpy(floatbytes, &f, sizeof(float));
	for (size_t i = 0; i < sizeof(float); i++)
	{
		WRITE_BYTE(floatbytes[i]);
	}
}
inline void WRITE_PRECISE_VECTOR(const float* v)
{
	if (_isBogusMessage)
	{
		g_engfuncs.pfnAlertMessage(at_error, "Bogus message precise vector: %f %f %f\n", v[0], v[1], v[2]);
		return;
	}
	WRITE_FLOAT(v[0]);
	WRITE_FLOAT(v[1]);
	WRITE_FLOAT(v[2]);
}
