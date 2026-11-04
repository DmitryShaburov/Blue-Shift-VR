

// #include <string>
// #include <iostream>

#ifdef _WIN32
#include "easyhook.h"
#endif

#include "hud.h"
#include "cl_util.h"
#include "vr_gl.h"
#include "VROpenGLInterceptor.h"

namespace
{
	#ifdef _WIN32
	std::vector<std::shared_ptr<HOOK_TRACE_INFO>> hooks;
	#endif

	bool hlvr_GLMatricesLocked = false;
	int hlvr_PushCount = 0;
}

void HLVR_LockGLMatrices(bool resetpushcount)
{
	hlvr_GLMatricesLocked = true;
	if (resetpushcount)
	{
		hlvr_PushCount = 0;
	}
}

void HLVR_UnlockGLMatrices(bool resetpushcount)
{
	hlvr_GLMatricesLocked = false;
	if (resetpushcount)
	{
		hlvr_PushCount = 0;
	}
}

#ifdef _WIN32
extern "C" void __stdcall MyGLEnable(GLenum cap)
{
	//OutputDebugStringA("MyGLEnable\n");
	if (!(hlvr_GLMatricesLocked && cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE5))
	{
		glEnable(cap);
	}
}

extern "C" void __stdcall MyGLDisable(GLenum cap)
{
	//OutputDebugStringA("MyGLDisable\n");
	if (!(hlvr_GLMatricesLocked && cap == GL_DEPTH_TEST))
	{
		glDisable(cap);
	}
}

extern "C" void __stdcall MyGLViewport(GLint x, GLint y, GLint width, GLsizei height)
{
	//OutputDebugStringA("MyGLViewport\n");
	if (!hlvr_GLMatricesLocked)
	{
		glViewport(x, y, width, height);
	}
}

extern "C" void __stdcall MyGLPushMatrix(void)
{
	//OutputDebugStringA("MyGLPushMatrix\n");
	glPushMatrix();
	hlvr_PushCount++;
}

extern "C" void __stdcall MyGLPopMatrix(void)
{
	//OutputDebugStringA("MyGLPopMatrix\n");
	glPopMatrix();
	hlvr_PushCount--;
}

extern "C" void __stdcall MyGLLoadIdentity(void)
{
	//OutputDebugStringA("MyGLLoadIdentity\n");
	if (!hlvr_GLMatricesLocked)
	{
		glLoadIdentity();
	}
}

extern "C" void __stdcall MyGLMatrixMode(GLenum mode)
{
	//OutputDebugStringA("MyGLMatrixMode\n");
	if (!hlvr_GLMatricesLocked)
	{
		glMatrixMode(mode);
	}
}

extern "C" void __stdcall MyGLLoadMatrixd(const GLdouble * m)
{
	//OutputDebugStringA("MyGLLoadMatrixd\n");
	if (!hlvr_GLMatricesLocked)
	{
		glLoadMatrixd(m);
	}
}

extern "C" void __stdcall MyGLLoadMatrixf(const GLfloat * m)
{
	//OutputDebugStringA("MyGLLoadMatrixf\n");
	if (!hlvr_GLMatricesLocked)
	{
		glLoadMatrixf(m);
	}
}

extern "C" void __stdcall MyGLMultMatrixd(const GLdouble * m)
{
	//OutputDebugStringA("MyGLMultMatrixd\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glMultMatrixd(m);
	}
}

extern "C" void __stdcall MyGLMultMatrixf(const GLfloat * m)
{
	//OutputDebugStringA("MyGLMultMatrixf\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glMultMatrixf(m);
	}
}

extern "C" void __stdcall MyGLScaled(GLdouble x, GLdouble y, GLdouble z)
{
	//OutputDebugStringA("MyGLScaled\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glScaled(x, y, z);
	}
}

extern "C" void __stdcall MyGLScalef(GLfloat x, GLfloat y, GLfloat z)
{
	//OutputDebugStringA("MyGLScalef\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glScalef(x, y, z);
	}
}

extern "C" void __stdcall MyGLTranslated(GLdouble x, GLdouble y, GLdouble z)
{
	//OutputDebugStringA("MyGLTranslated\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glTranslated(x, y, z);
	}
}

extern "C" void __stdcall MyGLTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	//OutputDebugStringA("MyGLTranslatef\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glTranslatef(x, y, z);
	}
}

extern "C" void __stdcall MyGLRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	//OutputDebugStringA("MyGLRotated\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glRotated(angle, x, y, z);
	}
}

extern "C" void __stdcall MyGLRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	//OutputDebugStringA("MyGLRotatef\n");
	if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
	{
		glRotatef(angle, x, y, z);
	}
}

extern "C" void __stdcall MyGLFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
	//OutputDebugStringA("MyGLFrustum\n");
	if (!hlvr_GLMatricesLocked)
	{
		glFrustum(left, right, bottom, top, zNear, zFar);
	}
}

bool InterceptOpenGLCall(const char* name, void* hookmethod)
{
	std::shared_ptr<HOOK_TRACE_INFO> hook = std::make_shared<HOOK_TRACE_INFO>(HOOK_TRACE_INFO{ nullptr });
	hooks.push_back(hook);

	extern void* GetOpenGLFuncAddress(const char* name);

	NTSTATUS result = LhInstallHook(
		GetOpenGLFuncAddress(name),
		hookmethod,
		nullptr,
		hook.get());

	if (FAILED(result))
	{
		gEngfuncs.Con_DPrintf("Warning: Failed to install hook for OpenGL method %s: %s\n", name, RtlGetLastErrorString());
		return false;
	}
	else
	{
		ULONG ACLEntries[1] = { 0 };
		LhSetInclusiveACL(ACLEntries, 1, hook.get());
		//gEngfuncs.Con_DPrintf("Successfully installed hook!\n");
		return true;
	}
}

bool InterceptOpenGLCalls()
{
	return
		InterceptOpenGLCall("glEnable", MyGLEnable) &&
		InterceptOpenGLCall("glDisable", MyGLDisable) &&
		InterceptOpenGLCall("glViewport", MyGLViewport) &&
		InterceptOpenGLCall("glPushMatrix", MyGLPushMatrix) &&
		InterceptOpenGLCall("glPopMatrix", MyGLPopMatrix) &&
		InterceptOpenGLCall("glLoadIdentity", MyGLLoadIdentity) &&
		InterceptOpenGLCall("glMatrixMode", MyGLMatrixMode) &&
		InterceptOpenGLCall("glLoadMatrixd", MyGLLoadMatrixd) &&
		InterceptOpenGLCall("glLoadMatrixf", MyGLLoadMatrixf) &&
		InterceptOpenGLCall("glMultMatrixd", MyGLMultMatrixd) &&
		InterceptOpenGLCall("glMultMatrixf", MyGLMultMatrixf) &&
		InterceptOpenGLCall("glScaled", MyGLScaled) &&
		InterceptOpenGLCall("glScalef", MyGLScalef) &&
		InterceptOpenGLCall("glTranslated", MyGLTranslated) &&
		InterceptOpenGLCall("glTranslatef", MyGLTranslatef) &&
		InterceptOpenGLCall("glRotated", MyGLRotated) &&
		InterceptOpenGLCall("glRotatef", MyGLRotatef) &&
		InterceptOpenGLCall("glFrustum", MyGLFrustum);
}

// BSVR start
// Remove the OpenGL hooks while opengl32.dll is guaranteed to still be loaded (called from HUD_Shutdown),
// instead of leaving it to EasyHook32.dll's own unload during process teardown, when the hooked DLLs
// may already be gone. LhWaitForPendingRemovals blocks until no thread is still inside a hook.
void UninstallOpenGLInterceptor()
{
	LhUninstallAllHooks();
	LhWaitForPendingRemovals();
	hooks.clear();
}
// BSVR end

#else // Linux

// The Linux engine (hw.so) calls OpenGL through its own function pointers (qglViewport, ...), filled once at startup.
// Pointing them at the wrappers below intercepts only the engine's calls; the mod's own calls still go to libGL.
// hw.so ships with its symbol table, which gives the address of each pointer.

#include "VREngineInterceptor.h"

#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace
{
	void MyGLEnable(GLenum cap)
	{
		if (!(hlvr_GLMatricesLocked && cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE5))
			glEnable(cap);
	}

	void MyGLDisable(GLenum cap)
	{
		if (!(hlvr_GLMatricesLocked && cap == GL_DEPTH_TEST))
			glDisable(cap);
	}

	void MyGLViewport(GLint x, GLint y, GLsizei width, GLsizei height)
	{
		if (!hlvr_GLMatricesLocked)
			glViewport(x, y, width, height);
	}

	void MyGLPushMatrix()
	{
		glPushMatrix();
		hlvr_PushCount++;
	}

	void MyGLPopMatrix()
	{
		glPopMatrix();
		hlvr_PushCount--;
	}

	void MyGLLoadIdentity()
	{
		if (!hlvr_GLMatricesLocked)
			glLoadIdentity();
	}

	void MyGLMatrixMode(GLenum mode)
	{
		if (!hlvr_GLMatricesLocked)
			glMatrixMode(mode);
	}

	void MyGLLoadMatrixd(const GLdouble* m)
	{
		if (!hlvr_GLMatricesLocked)
			glLoadMatrixd(m);
	}

	void MyGLLoadMatrixf(const GLfloat* m)
	{
		if (!hlvr_GLMatricesLocked)
			glLoadMatrixf(m);
	}

	void MyGLMultMatrixd(const GLdouble* m)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glMultMatrixd(m);
	}

	void MyGLMultMatrixf(const GLfloat* m)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glMultMatrixf(m);
	}

	void MyGLScaled(GLdouble x, GLdouble y, GLdouble z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glScaled(x, y, z);
	}

	void MyGLScalef(GLfloat x, GLfloat y, GLfloat z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glScalef(x, y, z);
	}

	void MyGLTranslated(GLdouble x, GLdouble y, GLdouble z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glTranslated(x, y, z);
	}

	void MyGLTranslatef(GLfloat x, GLfloat y, GLfloat z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glTranslatef(x, y, z);
	}

	void MyGLRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glRotated(angle, x, y, z);
	}

	void MyGLRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
	{
		if (!hlvr_GLMatricesLocked || hlvr_PushCount > 0)
			glRotatef(angle, x, y, z);
	}

	void MyGLFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
	{
		if (!hlvr_GLMatricesLocked)
			glFrustum(left, right, bottom, top, zNear, zFar);
	}

	struct EngineGLHook
	{
		const char* pointerName;
		void* wrapper;
		void** pointer = nullptr;
		void* original = nullptr;
	};

	EngineGLHook engineHooks[] = {
		{ "qglEnable", (void*)MyGLEnable },
		{ "qglDisable", (void*)MyGLDisable },
		{ "qglViewport", (void*)MyGLViewport },
		{ "qglPushMatrix", (void*)MyGLPushMatrix },
		{ "qglPopMatrix", (void*)MyGLPopMatrix },
		{ "qglLoadIdentity", (void*)MyGLLoadIdentity },
		{ "qglMatrixMode", (void*)MyGLMatrixMode },
		{ "qglLoadMatrixd", (void*)MyGLLoadMatrixd },
		{ "qglLoadMatrixf", (void*)MyGLLoadMatrixf },
		{ "qglMultMatrixd", (void*)MyGLMultMatrixd },
		{ "qglMultMatrixf", (void*)MyGLMultMatrixf },
		{ "qglScaled", (void*)MyGLScaled },
		{ "qglScalef", (void*)MyGLScalef },
		{ "qglTranslated", (void*)MyGLTranslated },
		{ "qglTranslatef", (void*)MyGLTranslatef },
		{ "qglRotated", (void*)MyGLRotated },
		{ "qglRotatef", (void*)MyGLRotatef },
		{ "qglFrustum", (void*)MyGLFrustum },
	};

	struct LoadedLibrary
	{
		std::string path;
		ElfW(Addr) base = 0;
	};

	int FindEngineLibrary(dl_phdr_info* info, size_t, void* data)
	{
		const char* name = info->dlpi_name;
		const size_t length = std::strlen(name);
		if (length >= 6 && std::strcmp(name + length - 6, "/hw.so") == 0)
		{
			auto* engine = static_cast<LoadedLibrary*>(data);
			engine->path = name;
			engine->base = info->dlpi_addr;
			return 1;
		}
		return 0;
	}

	// Sets engineHooks[].pointer from hw.so's symbol table (.symtab, the local symbols dlsym can't see)
	bool FindEnginePointers(const LoadedLibrary& engine)
	{
		std::ifstream file(engine.path, std::ios::binary);
		const std::vector<char> elf{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
		if (elf.size() < sizeof(Elf32_Ehdr))
			return false;

		const auto* header = reinterpret_cast<const Elf32_Ehdr*>(elf.data());
		if (header->e_shoff + header->e_shnum * sizeof(Elf32_Shdr) > elf.size())
			return false;
		const auto* sections = reinterpret_cast<const Elf32_Shdr*>(elf.data() + header->e_shoff);

		for (int i = 0; i < header->e_shnum; i++)
		{
			if (sections[i].sh_type != SHT_SYMTAB || sections[i].sh_link >= header->e_shnum)
				continue;

			const Elf32_Shdr& strings = sections[sections[i].sh_link];
			if (sections[i].sh_offset + sections[i].sh_size > elf.size() || strings.sh_offset + strings.sh_size > elf.size())
				return false;

			const auto* symbols = reinterpret_cast<const Elf32_Sym*>(elf.data() + sections[i].sh_offset);
			const size_t count = sections[i].sh_size / sizeof(Elf32_Sym);
			for (size_t s = 0; s < count; s++)
			{
				if (symbols[s].st_name >= strings.sh_size)
					continue;
				const char* name = elf.data() + strings.sh_offset + symbols[s].st_name;
				for (auto& hook : engineHooks)
				{
					if (std::strcmp(name, hook.pointerName) == 0)
						hook.pointer = reinterpret_cast<void**>(engine.base + symbols[s].st_value);
				}
			}
		}

		bool allFound = true;
		for (const auto& hook : engineHooks)
		{
			if (hook.pointer == nullptr || *hook.pointer == nullptr)
			{
				gEngfuncs.Con_DPrintf("Warning: Failed to find engine OpenGL pointer %s in %s\n", hook.pointerName, engine.path.c_str());
				allFound = false;
			}
		}
		return allFound;
	}
}  // namespace

bool InterceptOpenGLCalls()
{
	LoadedLibrary engine;
	if (!dl_iterate_phdr(FindEngineLibrary, &engine))
	{
		gEngfuncs.Con_DPrintf("Warning: Failed to find the engine library hw.so\n");
		return false;
	}
	if (!FindEnginePointers(engine))
		return false;

	for (auto& hook : engineHooks)
	{
		hook.original = *hook.pointer;
		*hook.pointer = hook.wrapper;
	}
	return true;
}

// Called from HUD_Shutdown: the engine must not call into the wrappers after client.so is unloaded.
// Like LhUninstallAllHooks on Windows, this also removes the engine function patches (VREngineInterceptor.cpp).
void UninstallOpenGLInterceptor()
{
	VRUnpatchEngineFunctions();

	for (auto& hook : engineHooks)
	{
		if (hook.pointer != nullptr && hook.original != nullptr)
			*hook.pointer = hook.original;
		hook.pointer = nullptr;
		hook.original = nullptr;
	}
}

#endif
