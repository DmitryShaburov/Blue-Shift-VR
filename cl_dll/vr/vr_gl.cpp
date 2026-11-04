
#include "vr_gl.h"

#include "hud.h"
#include "VRInput.h"

#ifndef _WIN32
#include <SDL2/SDL_video.h>
#endif

#ifndef GL_INVALID_FRAMEBUFFER_OPERATION
#define GL_INVALID_FRAMEBUFFER_OPERATION 0x0506
#endif

PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = nullptr;

PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = nullptr;

PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = nullptr;
#ifdef _WIN32
PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
#endif
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = nullptr;

bool IsValidAddressPointer(void* p)
{
	if (p == nullptr)
		return false;

	int64_t i = reinterpret_cast<int>(p);
	if (i == -1 || (i >= 0 && i <= 3))
		return false;

	return true;
}

void* GetOpenGLFuncAddress(const char* name)
{
#ifdef _WIN32
	void* p = wglGetProcAddress(name);
	if (!IsValidAddressPointer(p))
	{
		HMODULE module = LoadLibraryA("opengl32.dll");
		if (module)
		{
			p = GetProcAddress(module, name);
		}
	}
	return p;
#else
	// SDL resolves through whichever backend the engine window uses (GLX on X11, EGL on Wayland)
	return SDL_GL_GetProcAddress(name);
#endif
}

inline std::string GLErrorString(GLenum error)
{
	switch (error)
	{
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
	case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
	default: return std::to_string(error);
	}
}

void ClearGLErrors()
{
	while (glGetError() != GL_NO_ERROR)
		;
}

void TryTryGLCall(std::function<void()> call, const std::string& name, const std::string& args)
{
	call();
	auto error = glGetError();
	if (error != GL_NO_ERROR)
	{
		std::stringstream errormsg;
		errormsg << "OpenGL call ";
		errormsg << name;
		errormsg << "(";
		errormsg << args;
		errormsg << ")";
		errormsg << " failed with error: ";
		errormsg << GLErrorString(error);
		throw OGLErrorException{errormsg.str()};
	}
}

bool CreateGLTexture(unsigned int* texture, int width, int height)
{
	ClearGLErrors();

	try
	{
		TryGLCall(glEnable, GL_TEXTURE_2D);
		TryGLCall(glActiveTexture, GL_TEXTURE0);
		if (0 != *texture)
		{
			TryGLCall(glDeleteTextures, 1, texture);
		}
		TryGLCall(glGenTextures, 1, texture);
		TryGLCall(glBindTexture, GL_TEXTURE_2D, *texture);
		TryGLCall(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		TryGLCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		TryGLCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		TryGLCall(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		TryGLCall(glBindTexture, GL_TEXTURE_2D, 0);
	}
	catch (const OGLErrorException& e)
	{
		gEngfuncs.Con_DPrintf("%s\n", e.what());
		std::cerr << e.what() << std::endl
				  << std::flush;
		g_vrInput.DisplayErrorPopup(e.what());
		*texture = 0;
	}

	ClearGLErrors();

	return *texture != 0;
}

bool CreateGLFrameBuffer(unsigned int* framebuffer, unsigned int texture, int width, int height)
{
	unsigned int rbo = 0;

	ClearGLErrors();

	try
	{
		TryGLCall(glGenFramebuffers, 1, framebuffer);
		TryGLCall(glBindFramebuffer, GL_FRAMEBUFFER, *framebuffer);

		TryGLCall(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

		TryGLCall(glGenRenderbuffers, 1, &rbo);
		TryGLCall(glBindRenderbuffer, GL_RENDERBUFFER, rbo);
		TryGLCall(glRenderbufferStorage, GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		TryGLCall(glFramebufferRenderbuffer, GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
		TryGLCall(glBindRenderbuffer, GL_RENDERBUFFER, 0);

		TryGLCall(glBindFramebuffer, GL_FRAMEBUFFER, 0);
	}
	catch (const OGLErrorException& e)
	{
		gEngfuncs.Con_DPrintf("%s\n", e.what());
		std::cerr << e.what() << std::endl
				  << std::flush;
		g_vrInput.DisplayErrorPopup(e.what());
		*framebuffer = 0;
		rbo = 0;
	}

	ClearGLErrors();

	return *framebuffer != 0 && rbo != 0;
}

bool InitAdditionalGLFunctions()
{
	glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)GetOpenGLFuncAddress("glGenFramebuffers");
	glGenRenderbuffers = (PFNGLGENFRAMEBUFFERSPROC)GetOpenGLFuncAddress("glGenRenderbuffers");

	glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)GetOpenGLFuncAddress("glBindFramebuffer");
	glBindRenderbuffer = (PFNGLBINDFRAMEBUFFERPROC)GetOpenGLFuncAddress("glBindRenderbuffer");

	glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)GetOpenGLFuncAddress("glFramebufferTexture2D");
	glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)GetOpenGLFuncAddress("glRenderbufferStorage");
	glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)GetOpenGLFuncAddress("glFramebufferRenderbuffer");

#ifdef _WIN32
	glActiveTexture = (PFNGLACTIVETEXTUREPROC)GetOpenGLFuncAddress("glActiveTexture");
#endif
	glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)GetOpenGLFuncAddress("glGenerateMipmap");

	return glGenFramebuffers != nullptr &&
		   glGenRenderbuffers != nullptr &&
		   glBindFramebuffer != nullptr &&
		   glBindRenderbuffer != nullptr &&
		   glFramebufferTexture2D != nullptr &&
		   glRenderbufferStorage != nullptr &&
		   glFramebufferRenderbuffer != nullptr &&
		   glGenerateMipmap != nullptr;
}

void CaptureCurrentScreenToTexture(GLuint texture)
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, viewport[2], viewport[3]);
	glBindTexture(GL_TEXTURE_2D, 0);
}

namespace
{
// GLSL functions, loaded on first use. Local names so they don't clash with glext.h.
using CreateShaderFn = GLuint(APIENTRY*)(GLenum type);
using ShaderSourceFn = void(APIENTRY*)(GLuint shader, GLsizei count, const char* const* source, const GLint* length);
using CompileShaderFn = void(APIENTRY*)(GLuint shader);
using GetShaderivFn = void(APIENTRY*)(GLuint shader, GLenum pname, GLint* params);
using CreateProgramFn = GLuint(APIENTRY*)();
using AttachShaderFn = void(APIENTRY*)(GLuint program, GLuint shader);
using LinkProgramFn = void(APIENTRY*)(GLuint program);
using GetProgramivFn = void(APIENTRY*)(GLuint program, GLenum pname, GLint* params);
using UseProgramFn = void(APIENTRY*)(GLuint program);
using GetUniformLocationFn = GLint(APIENTRY*)(GLuint program, const char* name);
using Uniform1iFn = void(APIENTRY*)(GLint location, GLint v0);
using DeleteShaderFn = void(APIENTRY*)(GLuint shader);
using DeleteProgramFn = void(APIENTRY*)(GLuint program);

CreateShaderFn vrglCreateShader = nullptr;
ShaderSourceFn vrglShaderSource = nullptr;
CompileShaderFn vrglCompileShader = nullptr;
GetShaderivFn vrglGetShaderiv = nullptr;
CreateProgramFn vrglCreateProgram = nullptr;
AttachShaderFn vrglAttachShader = nullptr;
LinkProgramFn vrglLinkProgram = nullptr;
GetProgramivFn vrglGetProgramiv = nullptr;
UseProgramFn vrglUseProgram = nullptr;
GetUniformLocationFn vrglGetUniformLocation = nullptr;
Uniform1iFn vrglUniform1i = nullptr;
DeleteShaderFn vrglDeleteShader = nullptr;
DeleteProgramFn vrglDeleteProgram = nullptr;

constexpr GLenum VRGL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum VRGL_VERTEX_SHADER = 0x8B31;
constexpr GLenum VRGL_COMPILE_STATUS = 0x8B81;
constexpr GLenum VRGL_LINK_STATUS = 0x8B82;
constexpr GLenum VRGL_CURRENT_PROGRAM = 0x8B8D;

// Black (and anything darker than BlackLevel) becomes transparent, with a soft edge: alpha rises with the brightest
// channel. Over black, an anti-aliased pixel is its color times its coverage, so dividing by alpha restores the color.
const char* BlackToTransparentVertexShader =
	"void main() { gl_TexCoord[0] = gl_MultiTexCoord0; gl_Position = gl_Vertex; }";
const char* BlackToTransparentFragmentShader =
	"uniform sampler2D source;\n"
	"const float BlackLevel = 0.05;\n"
	"void main() {\n"
	"	vec3 color = texture2D(source, gl_TexCoord[0].st).rgb;\n"
	"	float alpha = clamp(max(color.r, max(color.g, color.b)) / BlackLevel, 0.0, 1.0);\n"
	"	gl_FragColor = vec4(alpha > 0.0 ? color / alpha : vec3(0.0), alpha);\n"
	"}\n";

GLuint CompileShader(GLenum type, const char* source)
{
	GLuint shader = vrglCreateShader(type);
	vrglShaderSource(shader, 1, &source, nullptr);
	vrglCompileShader(shader);
	GLint compiled = 0;
	vrglGetShaderiv(shader, VRGL_COMPILE_STATUS, &compiled);
	if (0 == compiled)
	{
		vrglDeleteShader(shader);
		return 0;
	}
	return shader;
}

// 0 if GLSL isn't available or the shaders don't compile, then it isn't tried again
GLuint GetBlackToTransparentProgram()
{
	static bool tried = false;
	static GLuint program = 0;
	if (tried)
		return program;
	tried = true;

	vrglCreateShader = (CreateShaderFn)GetOpenGLFuncAddress("glCreateShader");
	vrglShaderSource = (ShaderSourceFn)GetOpenGLFuncAddress("glShaderSource");
	vrglCompileShader = (CompileShaderFn)GetOpenGLFuncAddress("glCompileShader");
	vrglGetShaderiv = (GetShaderivFn)GetOpenGLFuncAddress("glGetShaderiv");
	vrglCreateProgram = (CreateProgramFn)GetOpenGLFuncAddress("glCreateProgram");
	vrglAttachShader = (AttachShaderFn)GetOpenGLFuncAddress("glAttachShader");
	vrglLinkProgram = (LinkProgramFn)GetOpenGLFuncAddress("glLinkProgram");
	vrglGetProgramiv = (GetProgramivFn)GetOpenGLFuncAddress("glGetProgramiv");
	vrglUseProgram = (UseProgramFn)GetOpenGLFuncAddress("glUseProgram");
	vrglGetUniformLocation = (GetUniformLocationFn)GetOpenGLFuncAddress("glGetUniformLocation");
	vrglUniform1i = (Uniform1iFn)GetOpenGLFuncAddress("glUniform1i");
	vrglDeleteShader = (DeleteShaderFn)GetOpenGLFuncAddress("glDeleteShader");
	vrglDeleteProgram = (DeleteProgramFn)GetOpenGLFuncAddress("glDeleteProgram");
	void* functions[] = {(void*)vrglCreateShader, (void*)vrglShaderSource, (void*)vrglCompileShader, (void*)vrglGetShaderiv,
		(void*)vrglCreateProgram, (void*)vrglAttachShader, (void*)vrglLinkProgram, (void*)vrglGetProgramiv, (void*)vrglUseProgram,
		(void*)vrglGetUniformLocation, (void*)vrglUniform1i, (void*)vrglDeleteShader, (void*)vrglDeleteProgram};
	for (void* function : functions)
	{
		if (!IsValidAddressPointer(function))
		{
			gEngfuncs.Con_DPrintf("GLSL is not available, the menu is shown with a black background.\n");
			return 0;
		}
	}

	GLuint vertexShader = CompileShader(VRGL_VERTEX_SHADER, BlackToTransparentVertexShader);
	GLuint fragmentShader = CompileShader(VRGL_FRAGMENT_SHADER, BlackToTransparentFragmentShader);
	if (vertexShader == 0 || fragmentShader == 0)
	{
		vrglDeleteShader(vertexShader); // ignores 0
		vrglDeleteShader(fragmentShader);
		gEngfuncs.Con_DPrintf("Could not compile the menu transparency shader, the menu is shown with a black background.\n");
		return 0;
	}
	GLuint newProgram = vrglCreateProgram();
	vrglAttachShader(newProgram, vertexShader);
	vrglAttachShader(newProgram, fragmentShader);
	vrglLinkProgram(newProgram);
	// the program keeps the attached shaders, they are freed with it
	vrglDeleteShader(vertexShader);
	vrglDeleteShader(fragmentShader);
	GLint linked = 0;
	vrglGetProgramiv(newProgram, VRGL_LINK_STATUS, &linked);
	if (0 == linked)
	{
		vrglDeleteProgram(newProgram);
		gEngfuncs.Con_DPrintf("Could not link the menu transparency shader, the menu is shown with a black background.\n");
		return 0;
	}
	program = newProgram;
	return program;
}
}

bool CopyTextureWithBlackTransparent(GLuint source, GLuint targetFramebuffer, int width, int height)
{
	GLuint program = GetBlackToTransparentProgram();
	if (program == 0)
		return false;

	GLint previousFramebuffer = 0;
	GLint previousProgram = 0;
	glGetIntegerv(VRGL_FRAMEBUFFER_BINDING, &previousFramebuffer);
	glGetIntegerv(VRGL_CURRENT_PROGRAM, &previousProgram);
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, source);

	vrglUseProgram(program);
	vrglUniform1i(vrglGetUniformLocation(program, "source"), 0);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0.f, 0.f);
	glVertex2f(-1.f, -1.f);
	glTexCoord2f(1.f, 0.f);
	glVertex2f(1.f, -1.f);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(1.f, 1.f);
	glTexCoord2f(0.f, 1.f);
	glVertex2f(-1.f, 1.f);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	vrglUseProgram(previousProgram);
	glBindTexture(GL_TEXTURE_2D, 0);
	glPopAttrib();
	glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
	return true;
}
