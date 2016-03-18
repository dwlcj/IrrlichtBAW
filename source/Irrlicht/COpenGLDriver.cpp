// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COpenGLDriver.h"
// needed here also because of the create methods' parameters
#include "CNullDriver.h"

#include "vectorSIMD.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_

#include "COpenGLTexture.h"
#include "COpenGLRenderBuffer.h"
#include "COpenGLPersistentlyMappedBuffer.h"
#include "COpenGLFrameBuffer.h"
#include "COpenGLSLMaterialRenderer.h"
#include "COpenGLOcclusionQuery.h"
#include "os.h"
#include "../Irrlicht/glext.h"

#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_
#include "MacOSX/CIrrDeviceMacOSX.h"
#endif

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
#include <SDL/SDL.h>
#endif

namespace irr
{
namespace video
{

// -----------------------------------------------------------------------
// WINDOWS CONSTRUCTOR
// -----------------------------------------------------------------------
#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
//! Windows constructor and init code
COpenGLDriver::COpenGLDriver(const irr::SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceWin32* device)
: CNullDriver(io, params.WindowSize), COpenGLExtensionHandler(),
	CurrentRenderMode(ERM_NONE), ResetRenderStates(true), Transformation3DChanged(true),
	AntiAlias(params.AntiAlias), CurrentFBO(0), CurrentVAO(0),
	CurrentRendertargetSize(0,0), ColorFormat(ECF_R8G8B8), Params(params),
	HDc(0), Window(static_cast<HWND>(params.WindowId)), Win32Device(device),
	DeviceType(EIDT_WIN32)
{
	#ifdef _DEBUG
	setDebugName("COpenGLDriver");
	#endif
}


bool COpenGLDriver::changeRenderContext(const SExposedVideoData& videoData, CIrrDeviceWin32* device)
{
	if (videoData.OpenGLWin32.HWnd && videoData.OpenGLWin32.HDc && videoData.OpenGLWin32.HRc)
	{
		if (!wglMakeCurrent((HDC)videoData.OpenGLWin32.HDc, (HGLRC)videoData.OpenGLWin32.HRc))
		{
			os::Printer::log("Render Context switch failed.");
			return false;
		}
		else
		{
			HDc = (HDC)videoData.OpenGLWin32.HDc;
		}
	}
	// set back to main context
	else if (HDc != ExposedData.OpenGLWin32.HDc)
	{
		if (!wglMakeCurrent((HDC)ExposedData.OpenGLWin32.HDc, (HGLRC)ExposedData.OpenGLWin32.HRc))
		{
			os::Printer::log("Render Context switch failed.");
			return false;
		}
		else
		{
			HDc = (HDC)ExposedData.OpenGLWin32.HDc;
		}
	}
	return true;
}

//! inits the open gl driver
bool COpenGLDriver::initDriver(CIrrDeviceWin32* device)
{
	// Create a window to test antialiasing support
	const fschar_t* ClassName = __TEXT("GLCIrrDeviceWin32");
	HINSTANCE lhInstance = GetModuleHandle(0);

	// Register Class
	WNDCLASSEX wcex;
	wcex.cbSize        = sizeof(WNDCLASSEX);
	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = (WNDPROC)DefWindowProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = lhInstance;
	wcex.hIcon         = NULL;
	wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName  = 0;
	wcex.lpszClassName = ClassName;
	wcex.hIconSm       = 0;
	wcex.hIcon         = 0;
	RegisterClassEx(&wcex);

	RECT clientSize;
	clientSize.top = 0;
	clientSize.left = 0;
	clientSize.right = Params.WindowSize.Width;
	clientSize.bottom = Params.WindowSize.Height;

	DWORD style = WS_POPUP;
	if (!Params.Fullscreen)
		style = WS_SYSMENU | WS_BORDER | WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

	AdjustWindowRect(&clientSize, style, FALSE);

	const s32 realWidth = clientSize.right - clientSize.left;
	const s32 realHeight = clientSize.bottom - clientSize.top;

	const s32 windowLeft = (GetSystemMetrics(SM_CXSCREEN) - realWidth) / 2;
	const s32 windowTop = (GetSystemMetrics(SM_CYSCREEN) - realHeight) / 2;

	HWND temporary_wnd=CreateWindow(ClassName, __TEXT(""), style, windowLeft,
			windowTop, realWidth, realHeight, NULL, NULL, lhInstance, NULL);

	if (!temporary_wnd)
	{
		os::Printer::log("Cannot create a temporary window.", ELL_ERROR);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	HDc = GetDC(temporary_wnd);

	// Set up pixel format descriptor with desired parameters
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),             // Size Of This Pixel Format Descriptor
		1,                                         // Version Number
		PFD_DRAW_TO_WINDOW |                       // Format Must Support Window
		PFD_SUPPORT_OPENGL |                       // Format Must Support OpenGL
		(Params.Doublebuffer?PFD_DOUBLEBUFFER:0) | // Must Support Double Buffering
		(Params.Stereobuffer?PFD_STEREO:0),        // Must Support Stereo Buffer
		PFD_TYPE_RGBA,                             // Request An RGBA Format
		Params.Bits,                               // Select Our Color Depth
		0, 0, 0, 0, 0, 0,                          // Color Bits Ignored
		0,                                         // No Alpha Buffer
		0,                                         // Shift Bit Ignored
		0,                                         // No Accumulation Buffer
		0, 0, 0, 0,	                               // Accumulation Bits Ignored
		Params.ZBufferBits,                        // Z-Buffer (Depth Buffer)
		BYTE(Params.Stencilbuffer ? 1 : 0),        // Stencil Buffer Depth
		0,                                         // No Auxiliary Buffer
		PFD_MAIN_PLANE,                            // Main Drawing Layer
		0,                                         // Reserved
		0, 0, 0                                    // Layer Masks Ignored
	};

	GLuint PixelFormat;

	for (u32 i=0; i<6; ++i)
	{
		if (i == 1)
		{
			if (Params.Stencilbuffer)
			{
				os::Printer::log("Cannot create a GL device with stencil buffer, disabling stencil shadows.", ELL_WARNING);
				Params.Stencilbuffer = false;
				pfd.cStencilBits = 0;
			}
			else
				continue;
		}
		else
		if (i == 2)
		{
			pfd.cDepthBits = 24;
		}
		else
		if (i == 3)
		{
			if (Params.Bits!=16)
				pfd.cDepthBits = 16;
			else
				continue;
		}
		else
		if (i == 4)
		{
			// try single buffer
			if (Params.Doublebuffer)
				pfd.dwFlags &= ~PFD_DOUBLEBUFFER;
			else
				continue;
		}
		else
		if (i == 5)
		{
			os::Printer::log("Cannot create a GL device context", "No suitable format for temporary window.", ELL_ERROR);
			ReleaseDC(temporary_wnd, HDc);
			DestroyWindow(temporary_wnd);
			UnregisterClass(ClassName, lhInstance);
			return false;
		}

		// choose pixelformat
		PixelFormat = ChoosePixelFormat(HDc, &pfd);
		if (PixelFormat)
			break;
	}

	SetPixelFormat(HDc, PixelFormat, &pfd);
	HGLRC hrc=wglCreateContext(HDc);
	if (!hrc)
	{
		os::Printer::log("Cannot create a temporary GL rendering context.", ELL_ERROR);
		ReleaseDC(temporary_wnd, HDc);
		DestroyWindow(temporary_wnd);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	SExposedVideoData data;
	data.OpenGLWin32.HDc = HDc;
	data.OpenGLWin32.HRc = hrc;
	data.OpenGLWin32.HWnd = temporary_wnd;


	if (!changeRenderContext(data, device))
	{
		os::Printer::log("Cannot activate a temporary GL rendering context.", ELL_ERROR);
		wglDeleteContext(hrc);
		ReleaseDC(temporary_wnd, HDc);
		DestroyWindow(temporary_wnd);
		UnregisterClass(ClassName, lhInstance);
		return false;
	}

	core::stringc wglExtensions;
#ifdef WGL_ARB_extensions_string
	PFNWGLGETEXTENSIONSSTRINGARBPROC irrGetExtensionsString = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (irrGetExtensionsString)
		wglExtensions = irrGetExtensionsString(HDc);
#elif defined(WGL_EXT_extensions_string)
	PFNWGLGETEXTENSIONSSTRINGEXTPROC irrGetExtensionsString = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
	if (irrGetExtensionsString)
		wglExtensions = irrGetExtensionsString(HDc);
#endif
	const bool pixel_format_supported = (wglExtensions.find("WGL_ARB_pixel_format") != -1);
	const bool multi_sample_supported = ((wglExtensions.find("WGL_ARB_multisample") != -1) ||
		(wglExtensions.find("WGL_EXT_multisample") != -1) || (wglExtensions.find("WGL_3DFX_multisample") != -1) );
#ifdef _DEBUG
	os::Printer::log("WGL_extensions", wglExtensions);
#endif

#ifdef WGL_ARB_pixel_format
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormat_ARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	if (pixel_format_supported && wglChoosePixelFormat_ARB)
	{
		// This value determines the number of samples used for antialiasing
		// My experience is that 8 does not show a big
		// improvement over 4, but 4 shows a big improvement
		// over 2.

		if(AntiAlias > 32)
			AntiAlias = 32;

		f32 fAttributes[] = {0.0, 0.0};
		s32 iAttributes[] =
		{
			WGL_DRAW_TO_WINDOW_ARB,1,
			WGL_SUPPORT_OPENGL_ARB,1,
			WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
			WGL_COLOR_BITS_ARB,(Params.Bits==32) ? 24 : 15,
			WGL_ALPHA_BITS_ARB,(Params.Bits==32) ? 8 : 1,
			WGL_DEPTH_BITS_ARB,Params.ZBufferBits, // 10,11
			WGL_STENCIL_BITS_ARB,Params.Stencilbuffer ? 1 : 0,
			WGL_DOUBLE_BUFFER_ARB,Params.Doublebuffer ? 1 : 0,
			WGL_STEREO_ARB,Params.Stereobuffer ? 1 : 0,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
#ifdef WGL_ARB_multisample
			WGL_SAMPLES_ARB,AntiAlias, // 20,21
			WGL_SAMPLE_BUFFERS_ARB, 1,
#elif defined(WGL_EXT_multisample)
			WGL_SAMPLES_EXT,AntiAlias, // 20,21
			WGL_SAMPLE_BUFFERS_EXT, 1,
#elif defined(WGL_3DFX_multisample)
			WGL_SAMPLES_3DFX,AntiAlias, // 20,21
			WGL_SAMPLE_BUFFERS_3DFX, 1,
#endif
#ifdef WGL_ARB_framebuffer_sRGB
			WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, Params.HandleSRGB ? 1:0,
#elif defined(WGL_EXT_framebuffer_sRGB)
			WGL_FRAMEBUFFER_SRGB_CAPABLE_EXT, Params.HandleSRGB ? 1:0,
#endif
//			WGL_DEPTH_FLOAT_EXT, 1,
			0,0,0,0
		};
		int iAttrSize = sizeof(iAttributes)/sizeof(int);
		const bool framebuffer_srgb_supported = ((wglExtensions.find("WGL_ARB_framebuffer_sRGB") != -1) ||
			(wglExtensions.find("WGL_EXT_framebuffer_sRGB") != -1));
		if (!framebuffer_srgb_supported)
		{
			memmove(&iAttributes[24],&iAttributes[26],sizeof(int)*(iAttrSize-26));
			iAttrSize -= 2;
		}
		if (!multi_sample_supported)
		{
			memmove(&iAttributes[20],&iAttributes[24],sizeof(int)*(iAttrSize-24));
			iAttrSize -= 4;
		}

		s32 rv=0;
		// Try to get an acceptable pixel format
		do
		{
			int pixelFormat=0;
			UINT numFormats=0;
			const BOOL valid = wglChoosePixelFormat_ARB(HDc,iAttributes,fAttributes,1,&pixelFormat,&numFormats);

			if (valid && numFormats)
				rv = pixelFormat;
			else
				iAttributes[21] -= 1;
		}
		while(rv==0 && iAttributes[21]>1);
		if (rv)
		{
			PixelFormat=rv;
			AntiAlias=iAttributes[21];
		}
	}
	else
#endif
		AntiAlias=0;

	wglMakeCurrent(HDc, NULL);
	wglDeleteContext(hrc);
	ReleaseDC(temporary_wnd, HDc);
	DestroyWindow(temporary_wnd);
	UnregisterClass(ClassName, lhInstance);

	// get hdc
	HDc=GetDC(Window);
	if (!HDc)
	{
		os::Printer::log("Cannot create a GL device context.", ELL_ERROR);
		return false;
	}

	// search for pixel format the simple way
	if (PixelFormat==0 || (!SetPixelFormat(HDc, PixelFormat, &pfd)))
	{
		for (u32 i=0; i<5; ++i)
		{
			if (i == 1)
			{
				if (Params.Stencilbuffer)
				{
					os::Printer::log("Cannot create a GL device with stencil buffer, disabling stencil shadows.", ELL_WARNING);
					Params.Stencilbuffer = false;
					pfd.cStencilBits = 0;
				}
				else
					continue;
			}
			else
			if (i == 2)
			{
				pfd.cDepthBits = 24;
			}
			if (i == 3)
			{
				if (Params.Bits!=16)
					pfd.cDepthBits = 16;
				else
					continue;
			}
			else
			if (i == 4)
			{
				os::Printer::log("Cannot create a GL device context", "No suitable format.", ELL_ERROR);
				return false;
			}

			// choose pixelformat
			PixelFormat = ChoosePixelFormat(HDc, &pfd);
			if (PixelFormat)
				break;
		}

        // set pixel format
        if (!SetPixelFormat(HDc, PixelFormat, &pfd))
        {
            os::Printer::log("Cannot set the pixel format.", ELL_ERROR);
            return false;
        }
    }
	os::Printer::log("Pixel Format", core::stringc(PixelFormat).c_str(), ELL_DEBUG);

	// create rendering context
#ifdef WGL_ARB_create_context
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribs_ARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	if (wglCreateContextAttribs_ARB)
	{
		int iAttribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 1,
			0
		};
		hrc=wglCreateContextAttribs_ARB(HDc, 0, iAttribs);
	}
	else
#endif
		hrc=wglCreateContext(HDc);

	if (!hrc)
	{
		os::Printer::log("Cannot create a GL rendering context.", ELL_ERROR);
		return false;
	}

	// set exposed data
	ExposedData.OpenGLWin32.HDc = HDc;
	ExposedData.OpenGLWin32.HRc = hrc;
	ExposedData.OpenGLWin32.HWnd = Window;

	// activate rendering context

	if (!changeRenderContext(ExposedData, device))
	{
		os::Printer::log("Cannot activate GL rendering context", ELL_ERROR);
		wglDeleteContext(hrc);
		return false;
	}

	int pf = GetPixelFormat(HDc);
	DescribePixelFormat(HDc, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
	if (pfd.cAlphaBits != 0)
	{
		if (pfd.cRedBits == 8)
			ColorFormat = ECF_A8R8G8B8;
		else
			ColorFormat = ECF_A1R5G5B5;
	}
	else
	{
		if (pfd.cRedBits == 8)
			ColorFormat = ECF_R8G8B8;
		else
			ColorFormat = ECF_R5G6B5;
	}

	genericDriverInit();

	extGlSwapInterval(Params.Vsync ? 1 : 0);
	return true;
}

#endif // _IRR_COMPILE_WITH_WINDOWS_DEVICE_

// -----------------------------------------------------------------------
// MacOSX CONSTRUCTOR
// -----------------------------------------------------------------------
#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_
//! Windows constructor and init code
COpenGLDriver::COpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceMacOSX *device)
: CNullDriver(io, params.WindowSize), COpenGLExtensionHandler(),
	CurrentRenderMode(ERM_NONE), ResetRenderStates(true), Transformation3DChanged(true),
	AntiAlias(params.AntiAlias), CurrentFBO(0), CurrentVAO(0),
	CurrentRendertargetSize(0,0), ColorFormat(ECF_R8G8B8),
	Params(params),
	OSXDevice(device), DeviceType(EIDT_OSX)
{
	#ifdef _DEBUG
	setDebugName("COpenGLDriver");
	#endif

	genericDriverInit();
}

#endif

// -----------------------------------------------------------------------
// LINUX CONSTRUCTOR
// -----------------------------------------------------------------------
#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
//! Linux constructor and init code
COpenGLDriver::COpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceLinux* device)
: CNullDriver(io, params.WindowSize), COpenGLExtensionHandler(),
	CurrentRenderMode(ERM_NONE), ResetRenderStates(true),
	Transformation3DChanged(true), AntiAlias(params.AntiAlias),
	CurrentFBO(0), CurrentVAO(0), CurrentRendertargetSize(0,0), ColorFormat(ECF_R8G8B8),
	Params(params), X11Device(device), DeviceType(EIDT_X11)
{
	#ifdef _DEBUG
	setDebugName("COpenGLDriver");
	#endif
}


bool COpenGLDriver::changeRenderContext(const SExposedVideoData& videoData, CIrrDeviceLinux* device)
{
	if (videoData.OpenGLLinux.X11Window)
	{
		if (videoData.OpenGLLinux.X11Display && videoData.OpenGLLinux.X11Context)
		{
			if (!glXMakeCurrent((Display*)videoData.OpenGLLinux.X11Display, videoData.OpenGLLinux.X11Window, (GLXContext)videoData.OpenGLLinux.X11Context))
			{
				os::Printer::log("Render Context switch failed.");
				return false;
			}
			else
			{
				Drawable = videoData.OpenGLLinux.X11Window;
				X11Display = (Display*)videoData.OpenGLLinux.X11Display;
			}
		}
		else
		{
			// in case we only got a window ID, try with the existing values for display and context
			if (!glXMakeCurrent((Display*)ExposedData.OpenGLLinux.X11Display, videoData.OpenGLLinux.X11Window, (GLXContext)ExposedData.OpenGLLinux.X11Context))
			{
				os::Printer::log("Render Context switch failed.");
				return false;
			}
			else
			{
				Drawable = videoData.OpenGLLinux.X11Window;
				X11Display = (Display*)ExposedData.OpenGLLinux.X11Display;
			}
		}
	}
	// set back to main context
	else if (X11Display != ExposedData.OpenGLLinux.X11Display)
	{
		if (!glXMakeCurrent((Display*)ExposedData.OpenGLLinux.X11Display, ExposedData.OpenGLLinux.X11Window, (GLXContext)ExposedData.OpenGLLinux.X11Context))
		{
			os::Printer::log("Render Context switch failed.");
			return false;
		}
		else
		{
			Drawable = ExposedData.OpenGLLinux.X11Window;
			X11Display = (Display*)ExposedData.OpenGLLinux.X11Display;
		}
	}
	return true;
}


//! inits the open gl driver
bool COpenGLDriver::initDriver(CIrrDeviceLinux* device)
{
	ExposedData.OpenGLLinux.X11Context = glXGetCurrentContext();
	ExposedData.OpenGLLinux.X11Display = glXGetCurrentDisplay();
	ExposedData.OpenGLLinux.X11Window = (unsigned long)Params.WindowId;
	Drawable = glXGetCurrentDrawable();
	X11Display = (Display*)ExposedData.OpenGLLinux.X11Display;

	genericDriverInit();

	// set vsync
	//if (queryOpenGLFeature(IRR_))
        extGlSwapInterval(Params.Vsync ? -1 : 0);
	return true;
}

#endif // _IRR_COMPILE_WITH_X11_DEVICE_


// -----------------------------------------------------------------------
// SDL CONSTRUCTOR
// -----------------------------------------------------------------------
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
//! SDL constructor and init code
COpenGLDriver::COpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceSDL* device)
: CNullDriver(io, params.WindowSize), COpenGLExtensionHandler(),
	CurrentRenderMode(ERM_NONE), ResetRenderStates(true),
	Transformation3DChanged(true), AntiAlias(params.AntiAlias),
	CurrentFBO(0), CurrentVAO(0), CurrentRendertargetSize(0,0), ColorFormat(ECF_R8G8B8),
	CurrentTarget(ERT_FRAME_BUFFER), Params(params),
	SDLDevice(device), DeviceType(EIDT_SDL)
{
	#ifdef _DEBUG
	setDebugName("COpenGLDriver");
	#endif

	genericDriverInit();
}

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_


//! destructor
COpenGLDriver::~COpenGLDriver()
{
    CurrentRendertargetSize = ScreenSize;
    extGlBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (CurrentFBO)
        CurrentFBO->drop();
    CurrentFBO = NULL;

	RequestedLights.clear();

	deleteMaterialRenders();

	CurrentTexture.clear();
	// I get a blue screen on my laptop, when I do not delete the
	// textures manually before releasing the dc. Oh how I love this.
	deleteAllTextures();

    extGlBindVertexArray(0);
    if (CurrentVAO)
        CurrentVAO->drop();

	for(std::map<uint64_t,GLuint>::iterator it = SamplerMap.begin(); it != SamplerMap.end(); it++)
    {
        extGlDeleteSamplers(1,&it->second);
    }
    SamplerMap.clear();

#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
	if (DeviceType == EIDT_WIN32)
	{

		if (ExposedData.OpenGLWin32.HRc)
		{
			if (!wglMakeCurrent(HDc, 0))
				os::Printer::log("Release of dc and rc failed.", ELL_WARNING);

			if (!wglDeleteContext((HGLRC)ExposedData.OpenGLWin32.HRc))
				os::Printer::log("Release of rendering context failed.", ELL_WARNING);
		}

		if (HDc)
			ReleaseDC(Window, HDc);
	}
#endif
}

// -----------------------------------------------------------------------
// METHODS
// -----------------------------------------------------------------------

bool COpenGLDriver::genericDriverInit()
{
	Name=L"OpenGL ";
	Name.append(glGetString(GL_VERSION));
	s32 pos=Name.findNext(L' ', 7);
	if (pos != -1)
		Name=Name.subString(0, pos);
	printVersion();

	// print renderer information
	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* vendor = glGetString(GL_VENDOR);
	if (renderer && vendor)
	{
		os::Printer::log(reinterpret_cast<const c8*>(renderer), reinterpret_cast<const c8*>(vendor), ELL_INFORMATION);
		VendorName = reinterpret_cast<const c8*>(vendor);
	}

	u32 i;
	CurrentTexture.clear();
	// load extensions
	initExtensions(Params.Stencilbuffer);
	if (queryFeature(EVDF_ARB_GLSL))
	{
		char buf[32];
		const u32 maj = ShaderLanguageVersion/10;
		snprintf(buf, 32, "%u.%u", maj, ShaderLanguageVersion-maj*10);
		os::Printer::log("GLSL version", buf, ELL_INFORMATION);
	}
	else
    {
		os::Printer::log("GLSL not available.", ELL_ERROR);
		return false;
    }

    if (Version<330)
    {
		os::Printer::log("OpenGL version is less than 3.3", ELL_ERROR);
		return false;
    }

    for (size_t i=0; i<MATERIAL_MAX_TEXTURES; i++)
    {
        CurrentSamplerHash[i] = 0xffffffffffffffffuLL;
    }

	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	// Reset The Current Viewport
	glViewport(0, 0, Params.WindowSize.Width, Params.WindowSize.Height);

	for (i=0; i<ETS_COUNT; ++i)
		setTransform(static_cast<E_TRANSFORMATION_STATE>(i), core::IdentityMatrix);

	setAmbientLight(SColorf(0.0f,0.0f,0.0f,0.0f));
#ifdef GL_EXT_separate_specular_color
	if (FeatureAvailable[IRR_EXT_separate_specular_color])
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#endif
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);

	Params.HandleSRGB &= ((FeatureAvailable[IRR_ARB_framebuffer_sRGB] || FeatureAvailable[IRR_EXT_framebuffer_sRGB]) &&
		FeatureAvailable[IRR_EXT_texture_sRGB]);
#if defined(GL_ARB_framebuffer_sRGB)
	if (Params.HandleSRGB)
		glEnable(GL_FRAMEBUFFER_SRGB);
#elif defined(GL_EXT_framebuffer_sRGB)
	if (Params.HandleSRGB)
		glEnable(GL_FRAMEBUFFER_SRGB_EXT);
#endif


	glClearDepth(1.0);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glDepthFunc(GL_LEQUAL);
	glFrontFace(GL_CW);
	// adjust flat coloring scheme to DirectX version
#if defined(GL_ARB_provoking_vertex) || defined(GL_EXT_provoking_vertex)
	extGlProvokingVertex(GL_FIRST_VERTEX_CONVENTION_EXT);
#endif

	// create material renderers
	createMaterialRenderers();

	// set the renderstates
	setRenderStates3DMode();

	glAlphaFunc(GL_GREATER, 0.f);

	// create matrix for flipping textures
	TextureFlipMatrix.buildTextureTransform(0.0f, core::vector2df(0,0), core::vector2df(0,1.0f), core::vector2df(1.0f,-1.0f));

	// We need to reset once more at the beginning of the first rendering.
	// This fixes problems with intermediate changes to the material during texture load.
	ResetRenderStates = true;

	return true;
}

class SimpleDummyCallBack : public video::IShaderConstantSetCallBack
{
protected:
    video::SConstantLocationNamePair mvpUniform[EMT_COUNT];
    video::E_MATERIAL_TYPE currentMatType;
public:
    SimpleDummyCallBack()
    {
        currentMatType = EMT_COUNT;
        for (size_t i=0; i<EMT_COUNT; i++)
            mvpUniform[i].location = -1;
    }
    virtual void OnUnsetMaterial()
    {
        currentMatType = EMT_COUNT;
    }
    virtual void PostLink(const video::E_MATERIAL_TYPE &materialType, const core::array<video::SConstantLocationNamePair> &constants)
    {
        for (size_t i=0; i<constants.size(); i++)
        {
            if (constants[i].name=="MVPMat")
            {
                mvpUniform[materialType] = constants[i];
                break;
            }
        }
    }
    virtual void OnSetMaterial(video::IMaterialRendererServices* services, const video::SMaterial &material, const video::SMaterial &lastMaterial)
    {
        currentMatType = currentMatType;
	}
	virtual void OnSetConstants(video::IMaterialRendererServices* services, s32 userData)
	{
	    if (currentMatType==EMT_COUNT)
            return;

	    if (mvpUniform[currentMatType].location>=0)
        {
            os::Printer::log("SETTING CONSTANTS IN BUILTIN", ELL_ERROR);
            os::Printer::log("SETTING CONSTANTS IN BUILTIN", ELL_INFORMATION);
            os::Printer::log("SETTING CONSTANTS IN BUILTIN", ELL_DEBUG);
            //services->setShaderConstant(gDriver->getTransform(ETS_WORLDVIEWPROJECTION).pointer(),mvpUniform.location,mvpUniform.type);
            core::matrix4 tempMat = static_cast<video::COpenGLDriver*>(services)->getTransform(ETS_PROJECTION)*static_cast<video::COpenGLDriver*>(services)->getTransform(ETS_VIEW)*static_cast<video::COpenGLDriver*>(services)->getTransform(ETS_WORLD);
            services->setShaderConstant(tempMat.pointer(),mvpUniform[currentMatType].location,mvpUniform[currentMatType].type);
        }
	}
};

void COpenGLDriver::createMaterialRenderers()
{
	// create OpenGL material renderers
    const char* std_vert =
    "#version 330 compatibility\n"
    "uniform mat4 MVPMat;\n"
    "layout(location = 0) in vec4 vPosAttr;\n"
    "layout(location = 2) in vec2 vTCAttr;\n"
    "layout(location = 1) in vec4 vColAttr;\n"
    "\n"
    "out vec4 vxCol;\n"
    "out vec2 tcCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = gl_ModelViewProjectionMatrix*vPosAttr;"
    "   vxCol = vColAttr;"
    "   tcCoord = vTCAttr;"
    "}";
    const char* std_solid_frag =
    "#version 330 core\n"
    "in vec4 vxCol;\n"
    "in vec2 tcCoord;\n"
    "\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
    "uniform sampler2D tex0;"
    "\n"
    "void main()\n"
    "{\n"
    "   outColor = texture(tex0,tcCoord);"
    "}";
    const char* std_trans_add_frag =
    "#version 330 core\n"
    "in vec4 vxCol;\n"
    "in vec2 tcCoord;\n"
    "\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
    "uniform sampler2D tex0;"
    "\n"
    "void main()\n"
    "{\n"
    "   outColor = texture(tex0,tcCoord);"
    "}";
    const char* std_trans_alpha_frag =
    "#version 330 core\n"
    "in vec4 vxCol;\n"
    "in vec2 tcCoord;\n"
    "\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
    "uniform sampler2D tex0;"
    "\n"
    "void main()\n"
    "{\n"
    "   vec4 tmp = texture(tex0,tcCoord)*vxCol;\n"
    "   if (tmp.a<0.00000000000000000000000000000000001)\n"
    "       discard;\n"
    "   outColor = tmp;"
    "}";
    const char* std_trans_vertex_frag =
    "#version 330 core\n"
    "in vec4 vxCol;\n"
    "in vec2 tcCoord;\n"
    "\n"
    "layout(location = 0) out vec4 outColor;\n"
    "\n"
    "uniform sampler2D tex0;"
    "\n"
    "void main()\n"
    "{\n"
    "   if (vxCol.a<0.00000000000000000000000000000000001)\n"
    "       discard;\n"
    "   outColor = vec4(texture(tex0,tcCoord).rgb,1.0)*vxCol;"
    "}";
    s32 nr;

    SimpleDummyCallBack* sdCB = new SimpleDummyCallBack();

    COpenGLSLMaterialRenderer* rdr = new COpenGLSLMaterialRenderer(
		this, nr,
		std_vert, "main",
		std_solid_frag, "main",
		NULL, NULL, NULL, NULL, NULL, NULL,3,sdCB,EMT_SOLID);
    if (rdr)
        rdr->drop();

	rdr = new COpenGLSLMaterialRenderer(
		this, nr,
		std_vert, "main",
		std_trans_add_frag, "main",
		NULL, NULL, NULL, NULL, NULL, NULL,3,sdCB,EMT_TRANSPARENT_ADD_COLOR);
    if (rdr)
        rdr->drop();

	rdr = new COpenGLSLMaterialRenderer(
		this, nr,
		std_vert, "main",
		std_trans_alpha_frag, "main",
		NULL, NULL, NULL, NULL, NULL, NULL,3,sdCB,EMT_TRANSPARENT_ALPHA_CHANNEL);
    if (rdr)
        rdr->drop();

	rdr = new COpenGLSLMaterialRenderer(
		this, nr,
		std_vert, "main",
		std_trans_vertex_frag, "main",
		NULL, NULL, NULL, NULL, NULL, NULL,3,sdCB,EMT_TRANSPARENT_VERTEX_ALPHA);
    if (rdr)
        rdr->drop();

    sdCB->drop();
}


//! presents the rendered scene on the screen, returns false if failed
bool COpenGLDriver::endScene()
{
	CNullDriver::endScene();

	//glFlush();

#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
	if (DeviceType == EIDT_WIN32)
		return SwapBuffers(HDc) == TRUE;
#endif

#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
	if (DeviceType == EIDT_X11)
	{
		glXSwapBuffers(X11Display, Drawable);
		return true;
	}
#endif

#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_
	if (DeviceType == EIDT_OSX)
	{
		OSXDevice->flush();
		return true;
	}
#endif

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
	if (DeviceType == EIDT_SDL)
	{
		SDL_GL_SwapBuffers();
		return true;
	}
#endif

	// todo: console device present

	return false;
}


//! init call for rendering start
bool COpenGLDriver::beginScene(bool backBuffer, bool zBuffer, SColor color,
		const SExposedVideoData& videoData, core::rect<s32>* sourceRect)
{
	CNullDriver::beginScene(backBuffer, zBuffer, color, videoData, sourceRect);
#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_
	if (DeviceType==EIDT_OSX)
		changeRenderContext(videoData, (void*)0);
#endif // _IRR_COMPILE_WITH_OSX_DEVICE_

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
	if (DeviceType == EIDT_SDL)
	{
		// todo: SDL sets glFrontFace(GL_CCW) after driver creation,
		// it would be better if this was fixed elsewhere.
		glFrontFace(GL_CW);
	}
#endif

    if (zBuffer)
    {
        clearZBuffer(1.0);
    }

    if (backBuffer)
    {
        core::vectorSIMDf colorf(color.getRed(),color.getGreen(),color.getBlue(),color.getAlpha());
        colorf /= 255.f;
        clearScreen(Params.Doublebuffer ? ESB_BACK_LEFT:ESB_FRONT_LEFT,reinterpret_cast<float*>(&colorf));
    }
	return true;
}


//! Returns the transformation set by setTransform
const core::matrix4& COpenGLDriver::getTransform(E_TRANSFORMATION_STATE state) const
{
	return Matrices[state];
}


//! sets transformation
void COpenGLDriver::setTransform(E_TRANSFORMATION_STATE state, const core::matrix4& mat)
{
	Matrices[state] = mat;
	Transformation3DChanged = true;

	switch (state)
	{
	case ETS_VIEW:
	case ETS_WORLD:
		{
			// OpenGL only has a model matrix, view and world is not existent. so lets fake these two.
			glMatrixMode(GL_MODELVIEW);

			// first load the viewing transformation for user clip planes
			glLoadMatrixf((Matrices[ETS_VIEW]).pointer());

			// now the real model-view matrix
			glMultMatrixf(Matrices[ETS_WORLD].pointer());
		}
		break;
	case ETS_PROJECTION:
		{
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(mat.pointer());
		}
		break;
	case ETS_COUNT:
		return;
	}
}

IGPUBuffer* COpenGLDriver::createGPUBuffer(const size_t &size, void* data, const bool canModifySubData, const bool &inCPUMem, const E_GPU_BUFFER_ACCESS &usagePattern)
{
    switch (usagePattern)
    {
        case EGBA_READ:
            return new COpenGLBuffer(size,data,(canModifySubData ? GL_DYNAMIC_STORAGE_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_READ_BIT);
        case EGBA_WRITE:
            return new COpenGLBuffer(size,data,(canModifySubData ? GL_DYNAMIC_STORAGE_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_WRITE_BIT);
        case EGBA_READ_WRITE:
            return new COpenGLBuffer(size,data,(canModifySubData ? GL_DYNAMIC_STORAGE_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);
        default:
            return new COpenGLBuffer(size,data,(canModifySubData ? GL_DYNAMIC_STORAGE_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0));
    }
}

IGPUMappedBuffer* COpenGLDriver::createPersistentlyMappedBuffer(const size_t &size, void* data, const E_GPU_BUFFER_ACCESS &usagePattern, const bool &assumedCoherent, const bool &inCPUMem)
{
    switch (usagePattern)
    {
        case EGBA_READ:
            return new COpenGLPersistentlyMappedBuffer(size,data,GL_MAP_PERSISTENT_BIT|(assumedCoherent ? GL_MAP_COHERENT_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_READ_BIT);
        case EGBA_WRITE:
            return new COpenGLPersistentlyMappedBuffer(size,data,GL_MAP_PERSISTENT_BIT|(assumedCoherent ? GL_MAP_COHERENT_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_WRITE_BIT);
        case EGBA_READ_WRITE:
            return new COpenGLPersistentlyMappedBuffer(size,data,GL_MAP_PERSISTENT_BIT|(assumedCoherent ? GL_MAP_COHERENT_BIT:0)|(inCPUMem ? GL_CLIENT_STORAGE_BIT:0)|GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);
        default:
            return NULL;
    }
}

scene::IGPUMeshDataFormatDesc* COpenGLDriver::createGPUMeshDataFormatDesc()
{
    return new COpenGLVAO();
}

scene::SGPUMesh* COpenGLDriver::createGPUMeshFromCPU(scene::SCPUMesh* mesh, const E_MESH_DESC_CONVERT_BEHAVIOUR& bufferOptions)
{
    scene::SGPUMesh* outmesh = new scene::SGPUMesh();
    for (size_t i=0; i<mesh->getMeshBufferCount(); i++)
    {
        scene::ICPUMeshBuffer* origmeshbuf = mesh->getMeshBuffer(i);
        scene::ICPUMeshDataFormatDesc* origdesc = dynamic_cast<scene::ICPUMeshDataFormatDesc*>(origmeshbuf->getMeshDataAndFormat());
        if (!origdesc)
            continue;

        bool success = true;
        bool noAttributes = true;
        const core::ICPUBuffer* oldbuffer[scene::EVAI_COUNT];
        scene::E_COMPONENTS_PER_ATTRIBUTE components[scene::EVAI_COUNT];
        scene::E_COMPONENT_TYPE componentTypes[scene::EVAI_COUNT];
        for (size_t j=0; j<scene::EVAI_COUNT; j++)
        {
            oldbuffer[j] = origdesc->getMappedBuffer((scene::E_VERTEX_ATTRIBUTE_ID)j);
            if (oldbuffer[j])
                noAttributes = false;

            scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;
            components[attrId] = origdesc->getAttribComponentCount(attrId);
            componentTypes[attrId] = origdesc->getAttribType(attrId);
            if (scene::vertexAttrSize[componentTypes[attrId]][components[attrId]]>=0xdeadbeefu)
            {
                os::Printer::log("createGPUMeshFromCPU input ICPUMeshBuffer(s) have one or more invalid attribute specs!\n",ELL_ERROR);
                success = false;
            }
        }
        if (noAttributes||!success)
            continue;
        //
        size_t oldBaseVertex;
        size_t indexBufferByteSize = 0;
        void* newIndexBuffer = NULL;
        //set indexCount
        scene::IGPUMeshBuffer* meshbuffer = new scene::IGPUMeshBuffer();
        meshbuffer->setIndexCount(origmeshbuf->getIndexCount());
        if (origdesc->getIndexBuffer())
        {
            //set indices
            uint32_t minIx = 0xffffffffu;
            uint32_t maxIx = 0;
            bool success = origmeshbuf->getIndexCount()>0;
            for (size_t j=0; success&&j<origmeshbuf->getIndexCount(); j++)
            {
                uint32_t ix;
                switch (origmeshbuf->getIndexType())
                {
                    case EIT_16BIT:
                        ix = ((uint16_t*)origmeshbuf->getIndices())[j];
                        break;
                    case EIT_32BIT:
                        ix = ((uint32_t*)origmeshbuf->getIndices())[j];
                        break;
                    default:
                        success = false;
                        break;
                }
                if (ix<minIx)
                    minIx = ix;
                if (ix>maxIx)
                    maxIx = ix;
            }
            //nothing will work if this is fucked
            for (size_t j=0; j<scene::EVAI_COUNT; j++)
            {
                if (!oldbuffer[j])
                    continue;

                scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;

                size_t byteEnd = origdesc->getMappedBufferOffset(attrId);
                byteEnd += (maxIx+origmeshbuf->getBaseVertex())*origdesc->getMappedBufferStride(attrId);
                byteEnd += scene::vertexAttrSize[componentTypes[attrId]][components[attrId]];
                if (byteEnd>oldbuffer[j]->getSize())
                    success = false;
            }
            // kill MB
            if (!success)
            {
                meshbuffer->drop();
                continue;
            }
            oldBaseVertex = minIx+origmeshbuf->getBaseVertex();
            uint32_t indexRange = maxIx-minIx;
            if (indexRange<0x10000u)
            {
                meshbuffer->setIndexType(EIT_16BIT);
                indexBufferByteSize = meshbuffer->getIndexCount()*2;
            }
            else
            {
                meshbuffer->setIndexType(EIT_32BIT);
                indexBufferByteSize = meshbuffer->getIndexCount()*4;
            }
            newIndexBuffer = malloc(indexBufferByteSize);
            //doesnt matter if shared VAO or not, range gets checked before baseVx
            meshbuffer->setIndexRange(0,indexRange);

            if (origmeshbuf->getIndexType()==meshbuffer->getIndexType()&&minIx==0)
                memcpy(newIndexBuffer,origmeshbuf->getIndices(),indexBufferByteSize);
            else
            {
                for (size_t j=0; j<origmeshbuf->getIndexCount(); j++)
                {
                    uint32_t ix;
                    if (origmeshbuf->getIndexType()==EIT_16BIT)
                        ix = ((uint16_t*)origmeshbuf->getIndices())[j];
                    else
                        ix = ((uint32_t*)origmeshbuf->getIndices())[j];

                    ix -= minIx;
                    if (indexRange<0x10000u)
                        ((uint16_t*)newIndexBuffer)[j] = ix;
                    else
                        ((uint32_t*)newIndexBuffer)[j] = ix;
                }
            }
        }
        else
        {
            oldBaseVertex = origmeshbuf->getBaseVertex();
            //
            size_t bigIx = origmeshbuf->getIndexCount();
            bool success = bigIx!=0;
            bigIx--;
            bigIx += oldBaseVertex;
            //check for overflow
            for (size_t j=0; success&&j<scene::EVAI_COUNT; j++)
            {
                if (!oldbuffer[j])
                    continue;

                scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;

                size_t byteEnd = origdesc->getMappedBufferOffset(attrId);
                byteEnd += bigIx*origdesc->getMappedBufferStride(attrId);
                byteEnd += scene::vertexAttrSize[componentTypes[attrId]][components[attrId]];
                if (byteEnd>oldbuffer[j]->getSize())
                    success = false;
            }
            // kill MB
            if (!success)
            {
                meshbuffer->drop();
                continue;
            }
            meshbuffer->setIndexRange(0,origmeshbuf->getIndexCount()-1);
        }
        //set bbox
        core::aabbox3df oldBBox = origmeshbuf->getBoundingBox();
        origmeshbuf->recalculateBoundingBox();
        meshbuffer->setBoundingBox(origmeshbuf->getBoundingBox());
        origmeshbuf->setBoundingBox(oldBBox);
        //set primitive type
        meshbuffer->setPrimitiveType(origmeshbuf->getPrimitiveType());
        //set material
        meshbuffer->getMaterial() = origmeshbuf->getMaterial();


        size_t bufferBindings[scene::EVAI_COUNT] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        size_t attrStride[scene::EVAI_COUNT];
        size_t attrOffset[scene::EVAI_COUNT];
        size_t bufferMin[scene::EVAI_COUNT];
        size_t bufferMax[scene::EVAI_COUNT];
        for (size_t j=0; j<scene::EVAI_COUNT; j++)
        {
            if (!oldbuffer[j])
                continue;

            bool alternateBinding = false;
            for (size_t k=0; k<j; k++)
            {
                if (oldbuffer[j]==oldbuffer[k])
                {
                    alternateBinding = true;
                    bufferBindings[j] = k;
                    break;
                }
            }


            scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;

            attrStride[j] = origdesc->getMappedBufferStride(attrId);
            attrOffset[j] = origdesc->getMappedBufferOffset(attrId);
            //
            size_t minMemPos = attrOffset[j]+oldBaseVertex*attrStride[j];
            if (!alternateBinding)
                bufferMin[j] = minMemPos;
            else if (minMemPos<bufferMin[bufferBindings[j]])
                bufferMin[bufferBindings[j]] = minMemPos;
            //
            size_t maxMemPos = minMemPos+meshbuffer->getIndexMaxBound()*attrStride[j]+scene::vertexAttrSize[componentTypes[attrId]][components[attrId]];
            if (!alternateBinding)
                bufferMax[j] = maxMemPos;
            else if (maxMemPos>bufferMax[bufferBindings[j]])
                bufferMax[bufferBindings[j]] = maxMemPos;
        }
        scene::IGPUMeshDataFormatDesc* desc = createGPUMeshDataFormatDesc();
        meshbuffer->setMeshDataAndFormat(desc);
        desc->drop();
        ///since we only copied relevant shit over
        //meshbuffer->setBaseVertex(0);
        //if (newIndexBuffer)
            //meshbuffer->setIndexBufferOffset(0);
        switch (bufferOptions)
        {
            //! It would be beneficial if this function compacted subdata ranges of used buffers to eliminate unused bytes
            //! But for This maybe a ICPUMeshBuffer "isolate" function is needed outside of this API, so we dont bother here?
            case EMDCB_CLONE_AND_MIRROR_LAYOUT:
                {
                    if (newIndexBuffer)
                    {
                        IGPUBuffer* indexBuf = createGPUBuffer(indexBufferByteSize,newIndexBuffer);
                        desc->mapIndexBuffer(indexBuf);
                        indexBuf->drop();
                    }

                    size_t allocatedGPUBuffers = 0;
                    IGPUBuffer* attrBuf[scene::EVAI_COUNT] = {NULL};
                    for (size_t j=0; j<scene::EVAI_COUNT; j++)
                    {
                        if (!oldbuffer[j])
                            continue;

                        if (bufferBindings[j]==j)
                            attrBuf[j] = createGPUBuffer(bufferMax[j]-bufferMin[j],((uint8_t*)oldbuffer[j]->getPointer())+bufferMin[j]);

                        scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;
                        desc->mapVertexAttrBuffer(attrBuf[bufferBindings[j]],attrId,components[attrId],componentTypes[attrId],attrStride[attrId],attrOffset[attrId]+oldBaseVertex*attrStride[j]-bufferMin[bufferBindings[j]]);
                        if (bufferBindings[j]==j)
                            attrBuf[bufferBindings[j]]->drop();
                    }
                }
                break;
            /**
            These conversion functions need to take into account the empty space (unused data) in buffers to avoid duplication
            This is why they are unfinished
            case EMDCB_PACK_ATTRIBUTES_SINGLE_BUFFER:
                {
                    if (newIndexBuffer)
                    {
                        IGPUBuffer* indexBuf = createGPUBuffer(indexBufferByteSize,newIndexBuffer);
                        desc->mapIndexBuffer(indexBuf);
                        indexBuf->drop();
                    }

                    size_t allocatedGPUBuffers = 0;
                    for (size_t j=0; j<scene::EVAI_COUNT; j++)
                    {
                        if (!oldbuffer[j])
                            continue;

                        if (bufferBindings[j]==j)
                            attrBuf[j] = createGPUBuffer(bufferMax[j]-bufferMin[j],((uint8_t*)oldbuffer[j]->getPointer())+bufferMin[j]);

                        scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)j;
                        desc->mapVertexAttrBuffer(attrBuf[bufferBindings[j]],attrId,components[attrId],componentTypes[attrId],attrStride[attrId],attrOffset[attrId]+oldBaseVertex*attrStride[j]-bufferMin[bufferBindings[j]]);
                        if (bufferBindings[j]==j)
                            attrBuf[bufferBindings[j]]->drop();
                    }
                }
                break;
            case EMDCB_PACK_ALL_SINGLE_BUFFER:
                break;**/
            case EMDCB_INTERLEAVED_PACK_ATTRIBUTES_SINGLE_BUFFER:
            case EMDCB_INTERLEAVED_PACK_ALL_SINGLE_BUFFER:
                {
                    size_t vertexSize = 0;
                    uint8_t* inPtr[scene::EVAI_COUNT] = {NULL};
                    for (size_t j=0; j<scene::EVAI_COUNT; j++)
                    {
                        if (!oldbuffer[j])
                            continue;

                        inPtr[j] = (uint8_t*)oldbuffer[j]->getPointer();
                        inPtr[j] += attrOffset[j]+oldBaseVertex*attrStride[j];

                        vertexSize += scene::vertexAttrSize[componentTypes[j]][components[j]];
                    }

                    size_t vertexBufferSize = vertexSize*(meshbuffer->getIndexMaxBound()+1);
                    void* mem = malloc(vertexBufferSize+indexBufferByteSize);
                    uint8_t* memPtr = (uint8_t*)mem;
                    for (uint8_t* memPtrLimit = memPtr+vertexBufferSize; memPtr<memPtrLimit; )
                    {
                        for (size_t j=0; j<scene::EVAI_COUNT; j++)
                        {
                            if (!oldbuffer[j])
                                continue;

                            switch (scene::vertexAttrSize[componentTypes[j]][components[j]])
                            {
                                case 1:
                                    ((uint8_t*)memPtr)[0] = ((uint8_t*)inPtr[j])[0];
                                    break;
                                case 2:
                                    ((uint16_t*)memPtr)[0] = ((uint16_t*)inPtr[j])[0];
                                    break;
                                case 3:
                                    ((uint16_t*)memPtr)[0] = ((uint16_t*)inPtr[j])[0];
                                    ((uint8_t*)memPtr)[2] = ((uint8_t*)inPtr[j])[2];
                                    break;
                                case 4:
                                    ((uint32_t*)memPtr)[0] = ((uint32_t*)inPtr[j])[0];
                                    break;
                                case 6:
                                    ((uint32_t*)memPtr)[0] = ((uint32_t*)inPtr[j])[0];
                                    ((uint16_t*)memPtr)[2] = ((uint16_t*)inPtr[j])[2];
                                    break;
                                case 8:
                                    ((uint64_t*)memPtr)[0] = ((uint64_t*)inPtr[j])[0];
                                    break;
                                case 12:
                                    ((uint64_t*)memPtr)[0] = ((uint64_t*)inPtr[j])[0];
                                    ((uint32_t*)memPtr)[2] = ((uint32_t*)inPtr[j])[2];
                                    break;
                                case 16:
                                    ((uint64_t*)memPtr)[0] = ((uint64_t*)inPtr[j])[0];
                                    ((uint64_t*)memPtr)[1] = ((uint64_t*)inPtr[j])[1];
                                    break;
                                case 24:
                                    ((uint64_t*)memPtr)[0] = ((uint64_t*)inPtr[j])[0];
                                    ((uint64_t*)memPtr)[1] = ((uint64_t*)inPtr[j])[1];
                                    ((uint64_t*)memPtr)[2] = ((uint64_t*)inPtr[j])[2];
                                    break;
                                case 32:
                                    ((uint64_t*)memPtr)[0] = ((uint64_t*)inPtr[j])[0];
                                    ((uint64_t*)memPtr)[1] = ((uint64_t*)inPtr[j])[1];
                                    ((uint64_t*)memPtr)[2] = ((uint64_t*)inPtr[j])[2];
                                    ((uint64_t*)memPtr)[3] = ((uint64_t*)inPtr[j])[3];
                                    break;
                            }
                            memPtr += scene::vertexAttrSize[componentTypes[j]][components[j]];

                            inPtr[j] += attrStride[j];
                        }
                    }
                    IGPUBuffer* vertexbuffer;
                    if (newIndexBuffer&&bufferOptions==EMDCB_INTERLEAVED_PACK_ALL_SINGLE_BUFFER)
                    {
                        memcpy(memPtr,newIndexBuffer,indexBufferByteSize);
                        vertexbuffer = createGPUBuffer(vertexBufferSize+indexBufferByteSize,mem);
                    }
                    else
                        vertexbuffer = createGPUBuffer(vertexBufferSize,mem);
                    free(mem);

                    size_t offset = 0;
                    for (size_t j=0; j<scene::EVAI_COUNT; j++)
                    {
                        if (!oldbuffer[j])
                            continue;

                        desc->mapVertexAttrBuffer(vertexbuffer,(scene::E_VERTEX_ATTRIBUTE_ID)j,components[j],componentTypes[j],vertexSize,offset);
                        offset += scene::vertexAttrSize[componentTypes[j]][components[j]];
                    }
                    vertexbuffer->drop();

                    if (newIndexBuffer)
                    {
                        if (bufferOptions==EMDCB_INTERLEAVED_PACK_ALL_SINGLE_BUFFER)
                        {
                            desc->mapIndexBuffer(vertexbuffer);
                            meshbuffer->setIndexBufferOffset(vertexBufferSize);
                        }
                        else
                        {
                            IGPUBuffer* indexBuf = createGPUBuffer(indexBufferByteSize,newIndexBuffer);
                            desc->mapIndexBuffer(indexBuf);
                            indexBuf->drop();
                        }
                    }
                }
                break;
            default:
                os::Printer::log("THIS CPU to GPU Mesh CONVERSION NOT SUPPORTED YET!\n",ELL_ERROR);
                if (newIndexBuffer)
                    free(newIndexBuffer);
                meshbuffer->drop();
                outmesh->drop();
                return NULL;
                break;
        }

        if (newIndexBuffer)
            free(newIndexBuffer);

        outmesh->addMeshBuffer(meshbuffer);
        meshbuffer->drop();
    }

    return outmesh;
}


//! Create occlusion query.
/** Use node for identification and mesh for occlusion test. */
IOcclusionQuery* COpenGLDriver::createOcclusionQuery(bool binary)
{
    COpenGLOcclusionQuery* query = new COpenGLOcclusionQuery(this,binary ? GL_ANY_SAMPLES_PASSED:GL_SAMPLES_PASSED);
    return query;
}


//! Update occlusion query. Retrieves results from GPU.
/** If the query shall not block, set the flag to false.
Update might not occur in this case, though */
void COpenGLDriver::updateOcclusionQuery(IOcclusionQuery* query, bool block)
{
    COpenGLOcclusionQuery* queryGL = (static_cast<COpenGLOcclusionQuery*>(query));

    if (currentQuery==queryGL||!queryGL||queryGL->getGLHandle()==0)
        return;


    if (block)
        extGlGetQueryObjectiv(queryGL->getGLHandle(),GL_QUERY_RESULT,queryGL->getCounterPointer());
    else
    {
        GLint available = GL_FALSE;
        extGlGetQueryObjectiv(queryGL->getGLHandle(),GL_QUERY_RESULT_AVAILABLE,queryGL->getCounterPointer());

        if (available==GL_TRUE)
            extGlGetQueryObjectiv(queryGL->getGLHandle(),GL_QUERY_RESULT,queryGL->getCounterPointer());
        else
            queryGL->getCounterPointer()[0] = 2147483647;
    }
}


void COpenGLDriver::beginOcclusionQuery(IOcclusionQuery* query)
{
    if (currentQuery)
        return;

    CNullDriver::beginOcclusionQuery(query);

    COpenGLOcclusionQuery* queryGL = (static_cast<COpenGLOcclusionQuery*>(currentQuery));

    if (!queryGL||queryGL->getGLHandle()==0)
        return;

    extGlBeginQuery(queryGL->getType(),queryGL->getGLHandle());
}

void COpenGLDriver::endOcclusionQuery()
{
    if (!currentQuery)
        return;

    COpenGLOcclusionQuery* queryGL = (static_cast<COpenGLOcclusionQuery*>(currentQuery));

    if (queryGL->getGLHandle()==0)
        return;

    extGlEndQuery(queryGL->getType());

    CNullDriver::endOcclusionQuery();
}

// small helper function to create vertex buffer object adress offsets
static inline u8* buffer_offset(const long offset)
{
	return ((u8*)0 + offset);
}


void COpenGLDriver::drawMeshBuffer(scene::ICPUMeshBuffer* mb, IOcclusionQuery* query)
{
    if (CurrentVAO)
    {
        CurrentVAO->drop();
        CurrentVAO = NULL;
        extGlBindVertexArray(0);
    }
    scene::ICPUMeshBuffer* mbAsCPUMB = dynamic_cast<scene::ICPUMeshBuffer*>(mb);
	if (!mbAsCPUMB)
		return;
    const scene::ICPUMeshDataFormatDesc* meshLayout = dynamic_cast<const scene::ICPUMeshDataFormatDesc*>(mbAsCPUMB->getMeshDataAndFormat());
    if (!meshLayout)
        return;

#ifdef _DEBUG
	if (mb->getIndexCount() > getMaximalIndicesCount())
	{
		char tmp[1024];
		sprintf(tmp,"Could not draw, too many indices(%u), maxium is %u.", mb->getIndexCount(), getMaximalIndicesCount());
		os::Printer::log(tmp, ELL_ERROR);
	}
#endif // _DEBUG

	CNullDriver::drawMeshBuffer(mbAsCPUMB,query);

	// draw everything
	setRenderStates3DMode();

    COpenGLOcclusionQuery* queryGL = (static_cast<COpenGLOcclusionQuery*>(query));

    bool didConditional = false;
    if ((currentQuery!=queryGL)&&queryGL&&(queryGL->getGLHandle()!=0))
    {
        extGlBeginConditionalRender(queryGL->getGLHandle(),queryGL->getCondWaitModeGL());
        didConditional = true;
    }

	GLenum indexSize=0;
    if (meshLayout->getIndexBuffer())
    {
        switch (mbAsCPUMB->getIndexType())
        {
            case EIT_16BIT:
            {
                indexSize=GL_UNSIGNED_SHORT;
                break;
            }
            case EIT_32BIT:
            {
                indexSize=GL_UNSIGNED_INT;
                break;
            }
            default:
                break;
        }
    }

    for (size_t i=0; i<scene::EVAI_COUNT; i++)
    {
        scene::E_VERTEX_ATTRIBUTE_ID attrId = (scene::E_VERTEX_ATTRIBUTE_ID)i;
        const core::ICPUBuffer* vBuff = meshLayout->getMappedBuffer(attrId);
        if (!vBuff)
            continue;

        extGlEnableVertexAttribArray(i);
        switch (meshLayout->getAttribType(attrId))
        {
            case scene::ECT_FLOAT:
            case scene::ECT_HALF_FLOAT:
            case scene::ECT_DOUBLE_IN_FLOAT_OUT:
            case scene::ECT_UNSIGNED_INT_10F_11F_11F_REV:
            //INTEGER FORMS
            case scene::ECT_NORMALIZED_INT_2_10_10_10_REV:
            case scene::ECT_NORMALIZED_UNSIGNED_INT_2_10_10_10_REV:
            case scene::ECT_NORMALIZED_BYTE:
            case scene::ECT_NORMALIZED_UNSIGNED_BYTE:
            case scene::ECT_NORMALIZED_SHORT:
            case scene::ECT_NORMALIZED_UNSIGNED_SHORT:
            case scene::ECT_NORMALIZED_INT:
            case scene::ECT_NORMALIZED_UNSIGNED_INT:
            case scene::ECT_INT_2_10_10_10_REV:
            case scene::ECT_UNSIGNED_INT_2_10_10_10_REV:
            case scene::ECT_BYTE:
            case scene::ECT_UNSIGNED_BYTE:
            case scene::ECT_SHORT:
            case scene::ECT_UNSIGNED_SHORT:
            case scene::ECT_INT:
            case scene::ECT_UNSIGNED_INT:
                extGlVertexAttribPointer(i,eComponentsPerAttributeToGLint[meshLayout->getAttribComponentCount(attrId)],eComponentTypeToGLenum[meshLayout->getAttribType(attrId)],isNormalized(meshLayout->getAttribType(attrId)),
                                            meshLayout->getMappedBufferStride(attrId),((uint8_t*)vBuff->getPointer())+meshLayout->getMappedBufferOffset(attrId));
                break;
            case scene::ECT_INTEGER_INT_2_10_10_10_REV:
            case scene::ECT_INTEGER_UNSIGNED_INT_2_10_10_10_REV:
            case scene::ECT_INTEGER_BYTE:
            case scene::ECT_INTEGER_UNSIGNED_BYTE:
            case scene::ECT_INTEGER_SHORT:
            case scene::ECT_INTEGER_UNSIGNED_SHORT:
            case scene::ECT_INTEGER_INT:
            case scene::ECT_INTEGER_UNSIGNED_INT:/*
                extGlVertexAttribIPointer(i,eComponentsPerAttributeToGLint[meshLayout->getAttribComponentCount(attrId)],eComponentTypeToGLenum[meshLayout->getAttribType(attrId)],
                                            meshLayout->getMappedBufferStride(attrId),((uint8_t*)vBuff->getPointer())+meshLayout->getMappedBufferOffset(attrId));
                break;
        //special
            case scene::ECT_DOUBLE_IN_DOUBLE_OUT:
                extGlVertexAttribLPointer(i,eComponentsPerAttributeToGLint[meshLayout->getAttribComponentCount(attrId)],GL_DOUBLE,
                                            meshLayout->getMappedBufferStride(attrId),((uint8_t*)vBuff->getPointer())+meshLayout->getMappedBufferOffset(attrId));*/
                os::Printer::log("This CPU-side vertex attribute format is unsupported!zn",ELL_ERROR);
                break;
        }
    }

    GLenum primType = primitiveTypeToGL(mbAsCPUMB->getPrimitiveType());
	switch (mbAsCPUMB->getPrimitiveType())
	{
		case scene::EPT_POINTS:
		case scene::EPT_POINT_SPRITES:
		{
			if (mbAsCPUMB->getPrimitiveType()==scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite])
				glEnable(GL_POINT_SPRITE_ARB);

			// prepare size and attenuation (where supported)
			GLfloat particleSize=Material.Thickness;
//			if (AntiAlias)
//				particleSize=core::clamp(particleSize, DimSmoothedPoint[0], DimSmoothedPoint[1]);
//			else
				particleSize=core::clamp(particleSize, DimAliasedPoint[0], DimAliasedPoint[1]);
			const float att[] = {1.0f, 1.0f, 0.0f};
			extGlPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att);
//			extGlPointParameterf(GL_POINT_SIZE_MIN,1.f);
			extGlPointParameterf(GL_POINT_SIZE_MAX, particleSize);
			extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 1.0f);
			glPointSize(particleSize);

            if (indexSize)
                extGlDrawRangeElementsBaseVertex(GL_POINTS,0,COpenGLExtensionHandler::MaxVertices,mbAsCPUMB->getIndexCount(),indexSize,((uint8_t*)meshLayout->getIndexBuffer()->getPointer())+mbAsCPUMB->getIndexBufferOffset(),mbAsCPUMB->getBaseVertex());
            else
                glDrawArrays(GL_POINTS, mbAsCPUMB->getBaseVertex(), mbAsCPUMB->getIndexCount());

			if (mbAsCPUMB->getPrimitiveType()==scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite])
			{
				glDisable(GL_POINT_SPRITE_ARB);
			}
		}
			break;
		case scene::EPT_TRIANGLES:
		case scene::EPT_QUADS:
        {
            if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
            {
                COpenGLSLMaterialRenderer* shaderRenderer = dynamic_cast<COpenGLSLMaterialRenderer*>(MaterialRenderers[Material.MaterialType].Renderer);
                if (shaderRenderer&&shaderRenderer->isTessellation())
                    primType = GL_PATCHES;
            }

            if (indexSize)
                extGlDrawRangeElementsBaseVertex(primType,0,COpenGLExtensionHandler::MaxVertices,mbAsCPUMB->getIndexCount(),indexSize,((uint8_t*)meshLayout->getIndexBuffer()->getPointer())+mbAsCPUMB->getIndexBufferOffset(),mbAsCPUMB->getBaseVertex());
            else
                glDrawArrays(primType, mbAsCPUMB->getBaseVertex(), mbAsCPUMB->getIndexCount());
        }
			break;
        default:
            if (indexSize)
                extGlDrawRangeElementsBaseVertex(primType,0,COpenGLExtensionHandler::MaxVertices,mbAsCPUMB->getIndexCount(),indexSize,((uint8_t*)meshLayout->getIndexBuffer()->getPointer())+mbAsCPUMB->getIndexBufferOffset(),mbAsCPUMB->getBaseVertex());
            else
                glDrawArrays(primType, mbAsCPUMB->getBaseVertex(), mbAsCPUMB->getIndexCount());
			break;
	}

    for (size_t i=0; i<scene::EVAI_COUNT; i++)
    {
        const core::ICPUBuffer* vBuff = meshLayout->getMappedBuffer((scene::E_VERTEX_ATTRIBUTE_ID)i);
        if (!vBuff)
            continue;

        extGlDisableVertexAttribArray(i);
    }



    if (didConditional)
        extGlEndConditionalRender();
}

void COpenGLDriver::drawMeshBuffer(scene::IGPUMeshBuffer* mb, IOcclusionQuery* query)
{
	if (!mb)
    {
        if (CurrentVAO)
        {
            CurrentVAO->drop();
            CurrentVAO = NULL;
        }
        extGlBindVertexArray(0);
		return;
    }
    COpenGLVAO* meshLayoutVAO = dynamic_cast<COpenGLVAO*>(mb->getMeshDataAndFormat());
	if (!meshLayoutVAO)
    {
        if (CurrentVAO)
        {
            CurrentVAO->drop();
            CurrentVAO = NULL;
        }
        extGlBindVertexArray(0);
		return;
    }

#ifdef _DEBUG
	if (mb->getIndexCount() > getMaximalIndicesCount())
	{
		char tmp[1024];
		sprintf(tmp,"Could not draw, too many indices(%u), maxium is %u.", mb->getIndexCount(), getMaximalIndicesCount());
		os::Printer::log(tmp, ELL_ERROR);
	}
#endif // _DEBUG

	CNullDriver::drawMeshBuffer(mb,query);

	// draw everything
	setRenderStates3DMode();

    COpenGLOcclusionQuery* queryGL = (static_cast<COpenGLOcclusionQuery*>(query));

    bool didConditional = false;
    if ((currentQuery!=queryGL)&&queryGL&&(queryGL->getGLHandle()!=0))
    {
        extGlBeginConditionalRender(queryGL->getGLHandle(),queryGL->getCondWaitModeGL());
        didConditional = true;
    }

    if (!meshLayoutVAO->rebindRevalidate())
    {
#ifdef _DEBUG
        os::Printer::log("VAO revalidation failed!",ELL_ERROR);
#endif // _DEBUG
        return;
    }

    if (CurrentVAO!=meshLayoutVAO)
    {
        meshLayoutVAO->grab();
        extGlBindVertexArray(meshLayoutVAO->getOpenGLName());
        if (CurrentVAO)
            CurrentVAO->drop();

        CurrentVAO = meshLayoutVAO;
    }

	GLenum indexSize=0;
    if (meshLayoutVAO->getIndexBuffer())
    {
        switch (mb->getIndexType())
        {
            case EIT_16BIT:
            {
                indexSize=GL_UNSIGNED_SHORT;
                break;
            }
            case EIT_32BIT:
            {
                indexSize=GL_UNSIGNED_INT;
                break;
            }
            default:
                break;
        }
    }

    GLenum primType = primitiveTypeToGL(mb->getPrimitiveType());
	switch (mb->getPrimitiveType())
	{
		case scene::EPT_POINTS:
		case scene::EPT_POINT_SPRITES:
		{
			if (mb->getPrimitiveType()==scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite])
				glEnable(GL_POINT_SPRITE_ARB);

			// prepare size and attenuation (where supported)
			GLfloat particleSize=Material.Thickness;
//			if (AntiAlias)
//				particleSize=core::clamp(particleSize, DimSmoothedPoint[0], DimSmoothedPoint[1]);
//			else
				particleSize=core::clamp(particleSize, DimAliasedPoint[0], DimAliasedPoint[1]);
			const float att[] = {1.0f, 1.0f, 0.0f};
			extGlPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att);
//			extGlPointParameterf(GL_POINT_SIZE_MIN,1.f);
			extGlPointParameterf(GL_POINT_SIZE_MAX, particleSize);
			extGlPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 1.0f);
			glPointSize(particleSize);

            if (indexSize)
                extGlDrawRangeElementsBaseVertex(GL_POINTS,mb->getIndexMinBound(),mb->getIndexMaxBound(),mb->getIndexCount(),indexSize,(void*)mb->getIndexBufferOffset(),mb->getBaseVertex());
            else
                glDrawArrays(GL_POINTS, mb->getBaseVertex(), mb->getIndexCount());

			if (mb->getPrimitiveType()==scene::EPT_POINT_SPRITES && FeatureAvailable[IRR_ARB_point_sprite])
			{
				glDisable(GL_POINT_SPRITE_ARB);
			}
		}
			break;
		case scene::EPT_TRIANGLES:
		case scene::EPT_QUADS:
        {
            if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
            {
                COpenGLSLMaterialRenderer* shaderRenderer = dynamic_cast<COpenGLSLMaterialRenderer*>(MaterialRenderers[Material.MaterialType].Renderer);
                if (shaderRenderer&&shaderRenderer->isTessellation())
                    primType = GL_PATCHES;
            }

            if (indexSize)
                extGlDrawRangeElementsBaseVertex(primType,mb->getIndexMinBound(),mb->getIndexMaxBound(),mb->getIndexCount(),indexSize,(void*)mb->getIndexBufferOffset(),mb->getBaseVertex());
            else
                glDrawArrays(primType, mb->getBaseVertex(), mb->getIndexCount());
        }
			break;
        default:
            if (indexSize)
                extGlDrawRangeElementsBaseVertex(primType,mb->getIndexMinBound(),mb->getIndexMaxBound(),mb->getIndexCount(),indexSize,(void*)mb->getIndexBufferOffset(),mb->getBaseVertex());
            else
                glDrawArrays(primType, mb->getBaseVertex(), mb->getIndexCount());
			break;
	}


    if (didConditional)
        extGlEndConditionalRender();
}


//! Get native wrap mode value
inline GLint getTextureWrapMode(const u8 &clamp)
{
	GLint mode=GL_REPEAT;
	switch (clamp)
	{
		case ETC_REPEAT:
			mode=GL_REPEAT;
			break;
		case ETC_CLAMP_TO_EDGE:
			mode=GL_CLAMP_TO_EDGE;
			break;
		case ETC_CLAMP_TO_BORDER:
            mode=GL_CLAMP_TO_BORDER;
			break;
		case ETC_MIRROR:
            mode=GL_MIRRORED_REPEAT;
			break;
		case ETC_MIRROR_CLAMP_TO_EDGE:
			if (COpenGLExtensionHandler::FeatureAvailable[COpenGLExtensionHandler::IRR_EXT_texture_mirror_clamp])
				mode = GL_MIRROR_CLAMP_TO_EDGE_EXT;
			else if (COpenGLExtensionHandler::FeatureAvailable[COpenGLExtensionHandler::IRR_ATI_texture_mirror_once])
				mode = GL_MIRROR_CLAMP_TO_EDGE_ATI;
			else
				mode = GL_CLAMP;
			break;
		case ETC_MIRROR_CLAMP_TO_BORDER:
			if (COpenGLExtensionHandler::FeatureAvailable[COpenGLExtensionHandler::IRR_EXT_texture_mirror_clamp])
				mode = GL_MIRROR_CLAMP_TO_BORDER_EXT;
			else
				mode = GL_CLAMP;
			break;
	}
	return mode;
}


GLuint COpenGLDriver::constructSamplerInCache(const uint64_t &hashVal)
{
    GLuint samplerHandle;
    extGlGenSamplers(1,&samplerHandle);

    const STextureSamplingParams* tmpTSP = reinterpret_cast<const STextureSamplingParams*>(&hashVal);

    switch (tmpTSP->MinFilter)
    {
        case ETFT_NEAREST_NO_MIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            break;
        case ETFT_LINEAR_NO_MIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            break;
        case ETFT_NEAREST_NEARESTMIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            break;
        case ETFT_LINEAR_NEARESTMIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            break;
        case ETFT_NEAREST_LINEARMIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            break;
        case ETFT_LINEAR_LINEARMIP:
            extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;
    }

    extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MAG_FILTER, tmpTSP->MaxFilter ? GL_LINEAR : GL_NEAREST);

    if (tmpTSP->AnisotropicFilter)
        extGlSamplerParameteri(samplerHandle, GL_TEXTURE_MAX_ANISOTROPY_EXT, core::min_(tmpTSP->AnisotropicFilter+1u,uint32_t(MaxAnisotropy)));

    extGlSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_S, getTextureWrapMode(tmpTSP->TextureWrapU));
    extGlSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_T, getTextureWrapMode(tmpTSP->TextureWrapV));
    extGlSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_R, getTextureWrapMode(tmpTSP->TextureWrapW));

    extGlSamplerParameterf(samplerHandle, GL_TEXTURE_LOD_BIAS, tmpTSP->LODBias);
    extGlSamplerParameteri(samplerHandle, GL_TEXTURE_CUBE_MAP_SEAMLESS, tmpTSP->SeamlessCubeMap);

    SamplerMap[hashVal] = samplerHandle;
    return samplerHandle;
}

bool COpenGLDriver::setActiveTexture(u32 stage, const video::ITexture* texture, const video::STextureSamplingParams &sampleParams)
{
	if (stage >= MaxTextureUnits)
		return false;


	if (CurrentTexture[stage]!=texture)
    {
        const video::COpenGLTexture* oldTexture = static_cast<const COpenGLTexture*>(CurrentTexture[stage]);
        CurrentTexture.set(stage,texture);

        if (!texture)
        {
            if (oldTexture)
                extGlBindTextureUnit(stage,0,oldTexture->getOpenGLTextureType());
        }
        else
        {
            if (texture->getDriverType() != EDT_OPENGL)
            {
                CurrentTexture.set(stage, 0);
                if (oldTexture)
                    extGlBindTextureUnit(stage,0,oldTexture->getOpenGLTextureType());
                os::Printer::log("Fatal Error: Tried to set a texture not owned by this driver.", ELL_ERROR);
            }
            else
                extGlBindTextureUnit(stage,static_cast<const COpenGLTexture*>(texture)->getOpenGLName(),static_cast<const COpenGLTexture*>(texture)->getOpenGLTextureType());
        }
    }

    if (CurrentTexture[stage])
    {
        uint64_t hashVal = sampleParams.calculateHash(CurrentTexture[stage]);
        if (CurrentSamplerHash[stage]!=hashVal)
        {
            CurrentSamplerHash[stage] = hashVal;
            std::map<uint64_t,GLuint>::iterator it = SamplerMap.find(hashVal);
            if (it != SamplerMap.end())
            {
                extGlBindSampler(stage,it->second);
            }
            else
            {
                extGlBindSampler(stage,constructSamplerInCache(hashVal));
            }
        }
    }
    else if (CurrentSamplerHash[stage]!=0xffffffffffffffffull)
    {
        CurrentSamplerHash[stage] = 0xffffffffffffffffull;
        extGlBindSampler(stage,0);
    }

	return true;
}


void COpenGLDriver::STextureStageCache::remove(const ITexture* tex)
{
    for (s32 i = MATERIAL_MAX_TEXTURES-1; i>= 0; --i)
    {
        if (CurrentTexture[i] == tex)
        {
            COpenGLExtensionHandler::extGlBindTextureUnit(i,0,static_cast<const COpenGLTexture*>(tex)->getOpenGLTextureType());
            COpenGLExtensionHandler::extGlBindSampler(i,0);
            tex->drop();
            CurrentTexture[i] = 0;
        }
    }
}

void COpenGLDriver::STextureStageCache::clear()
{
    // Drop all the CurrentTexture handles
    for (u32 i=0; i<MATERIAL_MAX_TEXTURES; ++i)
    {
        if (CurrentTexture[i])
        {
            COpenGLExtensionHandler::extGlBindTextureUnit(i,0,static_cast<const COpenGLTexture*>(CurrentTexture[i])->getOpenGLTextureType());
            COpenGLExtensionHandler::extGlBindSampler(i,0);
            CurrentTexture[i]->drop();
            CurrentTexture[i] = 0;
        }
    }
}



//! creates a matrix in supplied GLfloat array to pass to OpenGL
inline void COpenGLDriver::getGLMatrix(GLfloat gl_matrix[16], const core::matrix4& m)
{
	memcpy(gl_matrix, m.pointer(), 16 * sizeof(f32));
}


//! creates a opengltexturematrix from a D3D style texture matrix
inline void COpenGLDriver::getGLTextureMatrix(GLfloat *o, const core::matrix4& m)
{
	o[0] = m[0];
	o[1] = m[1];
	o[2] = 0.f;
	o[3] = 0.f;

	o[4] = m[4];
	o[5] = m[5];
	o[6] = 0.f;
	o[7] = 0.f;

	o[8] = 0.f;
	o[9] = 0.f;
	o[10] = 1.f;
	o[11] = 0.f;

	o[12] = m[8];
	o[13] = m[9];
	o[14] = 0.f;
	o[15] = 1.f;
}


//! returns a device dependent texture from a software surface (IImage)
video::ITexture* COpenGLDriver::createDeviceDependentTexture(IImage* surface, const io::path& name, void* mipmapData)
{
	return new COpenGLTexture(surface, name, mipmapData, this);
}

//! returns a device dependent texture from a software surface (IImage)
video::ITexture* COpenGLDriver::createDeviceDependentTexture(const core::dimension2d<u32>& size, uint32_t mipmapLevels,
			const io::path& name, ECOLOR_FORMAT format)
{
	return new COpenGLTexture(COpenGLTexture::getOpenGLFormatAndParametersFromColorFormat(format),size,NULL, GL_INVALID_ENUM,GL_INVALID_ENUM, name, NULL, this, mipmapLevels);
}


//! Sets a material. All 3d drawing functions draw geometry now using this material.
void COpenGLDriver::setMaterial(const SMaterial& material)
{
	Material = material;
	OverrideMaterial.apply(Material);

	for (s32 i = MaxTextureUnits-1; i>= 0; --i)
	{
		setActiveTexture(i, material.getTexture(i), material.TextureLayer[i].SamplingParams);
	}
}


//! prints error if an error happened.
bool COpenGLDriver::testGLError()
{
#ifdef _DEBUG
	GLenum g = glGetError();
	switch (g)
	{
	case GL_NO_ERROR:
		return false;
	case GL_INVALID_ENUM:
		os::Printer::log("GL_INVALID_ENUM", ELL_ERROR); break;
	case GL_INVALID_VALUE:
		os::Printer::log("GL_INVALID_VALUE", ELL_ERROR); break;
	case GL_INVALID_OPERATION:
		os::Printer::log("GL_INVALID_OPERATION", ELL_ERROR); break;
	case GL_STACK_OVERFLOW:
		os::Printer::log("GL_STACK_OVERFLOW", ELL_ERROR); break;
	case GL_STACK_UNDERFLOW:
		os::Printer::log("GL_STACK_UNDERFLOW", ELL_ERROR); break;
	case GL_OUT_OF_MEMORY:
		os::Printer::log("GL_OUT_OF_MEMORY", ELL_ERROR); break;
	case GL_TABLE_TOO_LARGE:
		os::Printer::log("GL_TABLE_TOO_LARGE", ELL_ERROR); break;
	case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
		os::Printer::log("GL_INVALID_FRAMEBUFFER_OPERATION", ELL_ERROR); break;
	};
//	_IRR_DEBUG_BREAK_IF(true);
	return true;
#else
	return false;
#endif
}


//! sets the needed renderstates
void COpenGLDriver::setRenderStates3DMode()
{
	if (CurrentRenderMode != ERM_3D)
	{
		// Reset Texture Stages
		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// switch back the matrices
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((Matrices[ETS_VIEW] * Matrices[ETS_WORLD]).pointer());

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(Matrices[ETS_PROJECTION].pointer());

		ResetRenderStates = true;
	}

	if (ResetRenderStates || LastMaterial != Material)
	{
		// unset old material

		if (LastMaterial.MaterialType != Material.MaterialType &&
				static_cast<u32>(LastMaterial.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[LastMaterial.MaterialType].Renderer->OnUnsetMaterial();

		// set new material.
		if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
			MaterialRenderers[Material.MaterialType].Renderer->OnSetMaterial(
				Material, LastMaterial, ResetRenderStates, this);

		LastMaterial = Material;
		ResetRenderStates = false;
	}

	if (static_cast<u32>(Material.MaterialType) < MaterialRenderers.size())
		MaterialRenderers[Material.MaterialType].Renderer->OnRender(this);

	CurrentRenderMode = ERM_3D;
}




//! Can be called by an IMaterialRenderer to make its work easier.
void COpenGLDriver::setBasicRenderStates(const SMaterial& material, const SMaterial& lastmaterial,
	bool resetAllRenderStates)
{
	// fillmode
	if (resetAllRenderStates || (lastmaterial.Wireframe != material.Wireframe) || (lastmaterial.PointCloud != material.PointCloud))
		glPolygonMode(GL_FRONT_AND_BACK, material.Wireframe ? GL_LINE : material.PointCloud? GL_POINT : GL_FILL);

	// zbuffer
	if (resetAllRenderStates || lastmaterial.ZBuffer != material.ZBuffer)
	{
		switch (material.ZBuffer)
		{
			case ECFN_NEVER:
				glDisable(GL_DEPTH_TEST);
				break;
			case ECFN_LESSEQUAL:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				break;
			case ECFN_EQUAL:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_EQUAL);
				break;
			case ECFN_LESS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS);
				break;
			case ECFN_NOTEQUAL:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_NOTEQUAL);
				break;
			case ECFN_GREATEREQUAL:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_GEQUAL);
				break;
			case ECFN_GREATER:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_GREATER);
				break;
			case ECFN_ALWAYS:
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_ALWAYS);
				break;
		}
	}

	// zwrite
//	if (resetAllRenderStates || lastmaterial.ZWriteEnable != material.ZWriteEnable)
	{
		if (material.ZWriteEnable && (AllowZWriteOnTransparent || !material.isTransparent()))
		{
			glDepthMask(GL_TRUE);
		}
		else
			glDepthMask(GL_FALSE);
	}

	// back face culling
	if (resetAllRenderStates || (lastmaterial.FrontfaceCulling != material.FrontfaceCulling) || (lastmaterial.BackfaceCulling != material.BackfaceCulling))
	{
		if ((material.FrontfaceCulling) && (material.BackfaceCulling))
		{
			glCullFace(GL_FRONT_AND_BACK);
			glEnable(GL_CULL_FACE);
		}
		else
		if (material.BackfaceCulling)
		{
			glCullFace(GL_BACK);
			glEnable(GL_CULL_FACE);
		}
		else
		if (material.FrontfaceCulling)
		{
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
		}
		else
			glDisable(GL_CULL_FACE);
	}

	// Color Mask
	if (resetAllRenderStates || lastmaterial.ColorMask != material.ColorMask)
	{
		glColorMask(
			(material.ColorMask & ECP_RED)?GL_TRUE:GL_FALSE,
			(material.ColorMask & ECP_GREEN)?GL_TRUE:GL_FALSE,
			(material.ColorMask & ECP_BLUE)?GL_TRUE:GL_FALSE,
			(material.ColorMask & ECP_ALPHA)?GL_TRUE:GL_FALSE);
	}

	if (queryFeature(EVDF_BLEND_OPERATIONS) &&
		(resetAllRenderStates|| lastmaterial.BlendOperation != material.BlendOperation))
	{
		if (material.BlendOperation==EBO_NONE)
			glDisable(GL_BLEND);
		else
		{
			glEnable(GL_BLEND);
			switch (material.BlendOperation)
			{
			case EBO_SUBTRACT:
                extGlBlendEquation(GL_FUNC_SUBTRACT);
				break;
			case EBO_REVSUBTRACT:
                extGlBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				break;
			case EBO_MIN:
                extGlBlendEquation(GL_MIN);
				break;
			case EBO_MAX:
                extGlBlendEquation(GL_MAX);
				break;
			case EBO_MIN_FACTOR:
#if defined(GL_AMD_blend_minmax_factor)
				if (FeatureAvailable[IRR_AMD_blend_minmax_factor])
					extGlBlendEquation(GL_FACTOR_MIN_AMD);
				// fallback in case of missing extension
				else
#endif
				extGlBlendEquation(GL_MIN);
				break;
			case EBO_MAX_FACTOR:
#if defined(GL_AMD_blend_minmax_factor)
				if (FeatureAvailable[IRR_AMD_blend_minmax_factor])
					extGlBlendEquation(GL_FACTOR_MAX_AMD);
				// fallback in case of missing extension
				else
#endif
				extGlBlendEquation(GL_MAX);
				break;
			case EBO_MIN_ALPHA:
#if defined(GL_SGIX_blend_alpha_minmax)
				if (FeatureAvailable[IRR_SGIX_blend_alpha_minmax])
					extGlBlendEquation(GL_ALPHA_MIN_SGIX);
				// fallback in case of missing extension
				else
					if (FeatureAvailable[IRR_EXT_blend_minmax])
						extGlBlendEquation(GL_MIN_EXT);
#endif
				break;
			case EBO_MAX_ALPHA:
#if defined(GL_SGIX_blend_alpha_minmax)
				if (FeatureAvailable[IRR_SGIX_blend_alpha_minmax])
					extGlBlendEquation(GL_ALPHA_MAX_SGIX);
				// fallback in case of missing extension
				else
					if (FeatureAvailable[IRR_EXT_blend_minmax])
						extGlBlendEquation(GL_MAX_EXT);
#endif
				break;
			default:
				extGlBlendEquation(GL_FUNC_ADD);
				break;
			}
		}
	}

	// Polygon Offset
	if (queryFeature(EVDF_POLYGON_OFFSET) && (resetAllRenderStates ||
		lastmaterial.PolygonOffsetDirection != material.PolygonOffsetDirection ||
		lastmaterial.PolygonOffsetFactor != material.PolygonOffsetFactor))
	{
		glDisable(lastmaterial.Wireframe?GL_POLYGON_OFFSET_LINE:lastmaterial.PointCloud?GL_POLYGON_OFFSET_POINT:GL_POLYGON_OFFSET_FILL);
		if (material.PolygonOffsetFactor)
		{
			glDisable(material.Wireframe?GL_POLYGON_OFFSET_LINE:material.PointCloud?GL_POLYGON_OFFSET_POINT:GL_POLYGON_OFFSET_FILL);
			glEnable(material.Wireframe?GL_POLYGON_OFFSET_LINE:material.PointCloud?GL_POLYGON_OFFSET_POINT:GL_POLYGON_OFFSET_FILL);
		}
		if (material.PolygonOffsetDirection==EPO_BACK)
			glPolygonOffset(1.0f, (GLfloat)material.PolygonOffsetFactor);
		else
			glPolygonOffset(-1.0f, (GLfloat)-material.PolygonOffsetFactor);
	}

	// thickness
	if (resetAllRenderStates || lastmaterial.Thickness != material.Thickness)
	{
		if (AntiAlias)
		{
//			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimSmoothedPoint[0], DimSmoothedPoint[1]));
			// we don't use point smoothing
			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedPoint[0], DimAliasedPoint[1]));
			glLineWidth(core::clamp(static_cast<GLfloat>(material.Thickness), DimSmoothedLine[0], DimSmoothedLine[1]));
		}
		else
		{
			glPointSize(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedPoint[0], DimAliasedPoint[1]));
			glLineWidth(core::clamp(static_cast<GLfloat>(material.Thickness), DimAliasedLine[0], DimAliasedLine[1]));
		}
	}

	// Anti aliasing
	if (resetAllRenderStates || lastmaterial.AntiAliasing != material.AntiAliasing)
	{
		if (FeatureAvailable[IRR_ARB_multisample])
		{
			if (material.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);
			else if (lastmaterial.AntiAliasing & EAAM_ALPHA_TO_COVERAGE)
				glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE_ARB);

			if ((AntiAlias >= 2) && (material.AntiAliasing & (EAAM_SIMPLE|EAAM_QUALITY)))
			{
				glEnable(GL_MULTISAMPLE_ARB);
			}
			else
				glDisable(GL_MULTISAMPLE_ARB);
		}
		if ((material.AntiAliasing & EAAM_LINE_SMOOTH) != (lastmaterial.AntiAliasing & EAAM_LINE_SMOOTH))
		{
			if (material.AntiAliasing & EAAM_LINE_SMOOTH)
				glEnable(GL_LINE_SMOOTH);
			else if (lastmaterial.AntiAliasing & EAAM_LINE_SMOOTH)
				glDisable(GL_LINE_SMOOTH);
		}
	}
}


//! Enable the 2d override material
void COpenGLDriver::enableMaterial2D(bool enable)
{
	if (!enable)
		CurrentRenderMode = ERM_NONE;
	CNullDriver::enableMaterial2D(enable);
}


//! \return Returns the name of the video driver.
const wchar_t* COpenGLDriver::getName() const
{
	return Name.c_str();
}


//! deletes all dynamic lights there are
void COpenGLDriver::deleteAllDynamicLights()
{
	for (s32 i=0; i<MaxLights; ++i)
		glDisable(GL_LIGHT0 + i);

	RequestedLights.clear();

	CNullDriver::deleteAllDynamicLights();
}


//! adds a dynamic light
s32 COpenGLDriver::addDynamicLight(const SLight& light)
{
	CNullDriver::addDynamicLight(light);

	RequestedLights.push_back(RequestedLight(light));

	u32 newLightIndex = RequestedLights.size() - 1;

	// Try and assign a hardware light just now, but don't worry if I can't
	assignHardwareLight(newLightIndex);

	return (s32)newLightIndex;
}


void COpenGLDriver::assignHardwareLight(u32 lightIndex)
{
	setTransform(ETS_WORLD, core::matrix4());

	s32 lidx;
	for (lidx=GL_LIGHT0; lidx < GL_LIGHT0 + MaxLights; ++lidx)
	{
		if(!glIsEnabled(lidx))
		{
			RequestedLights[lightIndex].HardwareLightIndex = lidx;
			break;
		}
	}

	if(lidx == GL_LIGHT0 + MaxLights) // There's no room for it just now
		return;

	GLfloat data[4];
	const SLight & light = RequestedLights[lightIndex].LightData;

	switch (light.Type)
	{
	case video::ELT_SPOT:
		data[0] = light.Direction.X;
		data[1] = light.Direction.Y;
		data[2] = light.Direction.Z;
		data[3] = 0.0f;
		glLightfv(lidx, GL_SPOT_DIRECTION, data);

		// set position
		data[0] = light.Position.X;
		data[1] = light.Position.Y;
		data[2] = light.Position.Z;
		data[3] = 1.0f; // 1.0f for positional light
		glLightfv(lidx, GL_POSITION, data);

		glLightf(lidx, GL_SPOT_EXPONENT, light.Falloff);
		glLightf(lidx, GL_SPOT_CUTOFF, light.OuterCone);
	break;
	case video::ELT_POINT:
		// set position
		data[0] = light.Position.X;
		data[1] = light.Position.Y;
		data[2] = light.Position.Z;
		data[3] = 1.0f; // 1.0f for positional light
		glLightfv(lidx, GL_POSITION, data);

		glLightf(lidx, GL_SPOT_EXPONENT, 0.0f);
		glLightf(lidx, GL_SPOT_CUTOFF, 180.0f);
	break;
	case video::ELT_DIRECTIONAL:
		// set direction
		data[0] = -light.Direction.X;
		data[1] = -light.Direction.Y;
		data[2] = -light.Direction.Z;
		data[3] = 0.0f; // 0.0f for directional light
		glLightfv(lidx, GL_POSITION, data);

		glLightf(lidx, GL_SPOT_EXPONENT, 0.0f);
		glLightf(lidx, GL_SPOT_CUTOFF, 180.0f);
	break;
	default:
	break;
	}

	// set diffuse color
	data[0] = light.DiffuseColor.r;
	data[1] = light.DiffuseColor.g;
	data[2] = light.DiffuseColor.b;
	data[3] = light.DiffuseColor.a;
	glLightfv(lidx, GL_DIFFUSE, data);

	// set specular color
	data[0] = light.SpecularColor.r;
	data[1] = light.SpecularColor.g;
	data[2] = light.SpecularColor.b;
	data[3] = light.SpecularColor.a;
	glLightfv(lidx, GL_SPECULAR, data);

	// set ambient color
	data[0] = light.AmbientColor.r;
	data[1] = light.AmbientColor.g;
	data[2] = light.AmbientColor.b;
	data[3] = light.AmbientColor.a;
	glLightfv(lidx, GL_AMBIENT, data);

	// 1.0f / (constant + linear * d + quadratic*(d*d);

	// set attenuation
	glLightf(lidx, GL_CONSTANT_ATTENUATION, light.Attenuation.X);
	glLightf(lidx, GL_LINEAR_ATTENUATION, light.Attenuation.Y);
	glLightf(lidx, GL_QUADRATIC_ATTENUATION, light.Attenuation.Z);

	glEnable(lidx);
}


//! Turns a dynamic light on or off
//! \param lightIndex: the index returned by addDynamicLight
//! \param turnOn: true to turn the light on, false to turn it off
void COpenGLDriver::turnLightOn(s32 lightIndex, bool turnOn)
{
	if(lightIndex < 0 || lightIndex >= (s32)RequestedLights.size())
		return;

	RequestedLight & requestedLight = RequestedLights[lightIndex];

	requestedLight.DesireToBeOn = turnOn;

	if(turnOn)
	{
		if(-1 == requestedLight.HardwareLightIndex)
			assignHardwareLight(lightIndex);
	}
	else
	{
		if(-1 != requestedLight.HardwareLightIndex)
		{
			// It's currently assigned, so free up the hardware light
			glDisable(requestedLight.HardwareLightIndex);
			requestedLight.HardwareLightIndex = -1;

			// Now let the first light that's waiting on a free hardware light grab it
			for(u32 requested = 0; requested < RequestedLights.size(); ++requested)
				if(RequestedLights[requested].DesireToBeOn
					&&
					-1 == RequestedLights[requested].HardwareLightIndex)
				{
					assignHardwareLight(requested);
					break;
				}
		}
	}
}


//! returns the maximal amount of dynamic lights the device can handle
u32 COpenGLDriver::getMaximalDynamicLightAmount() const
{
	return MaxLights;
}


//! Sets the dynamic ambient light color. The default color is
//! (0,0,0,0) which means it is dark.
//! \param color: New color of the ambient light.
void COpenGLDriver::setAmbientLight(const SColorf& color)
{
	GLfloat data[4] = {color.r, color.g, color.b, color.a};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, data);
}


// this code was sent in by Oliver Klems, thank you! (I modified the glViewport
// method just a bit.
void COpenGLDriver::setViewPort(const core::rect<s32>& area)
{
	if (area == ViewPort)
		return;
	core::rect<s32> vp = area;
	core::rect<s32> rendert(0,0, getCurrentRenderTargetSize().Width, getCurrentRenderTargetSize().Height);
	vp.clipAgainst(rendert);

	if (vp.getHeight()>0 && vp.getWidth()>0)
	{
		glViewport(vp.UpperLeftCorner.X,
				vp.UpperLeftCorner.Y,
				vp.getWidth(), vp.getHeight());

		ViewPort = vp;
	}
}


IRenderBuffer* COpenGLDriver::addRenderBuffer(const core::dimension2d<u32>& size, const ECOLOR_FORMAT format)
{
	IRenderBuffer* buffer = new COpenGLRenderBuffer(COpenGLTexture::getOpenGLFormatAndParametersFromColorFormat(format),size,this);
	CNullDriver::addRenderBuffer(buffer);
	return buffer;
}

IFrameBuffer* COpenGLDriver::addFrameBuffer()
{
	IFrameBuffer* fbo = new COpenGLFrameBuffer(this);
	CNullDriver::addFrameBuffer(fbo);
	return fbo;
}


//! Removes a texture from the texture cache and deletes it, freeing lot of memory.
void COpenGLDriver::removeTexture(ITexture* texture)
{
	if (!texture)
		return;

	CNullDriver::removeTexture(texture);
	// Remove this texture from CurrentTexture as well
	CurrentTexture.remove(texture);
}

//! Only used by the internal engine. Used to notify the driver that
//! the window was resized.
void COpenGLDriver::OnResize(const core::dimension2d<u32>& size)
{
	CNullDriver::OnResize(size);
	glViewport(0, 0, size.Width, size.Height);
	Transformation3DChanged = true;
}


//! Returns type of video driver
E_DRIVER_TYPE COpenGLDriver::getDriverType() const
{
	return EDT_OPENGL;
}


//! returns color format
ECOLOR_FORMAT COpenGLDriver::getColorFormat() const
{
	return ColorFormat;
}


void COpenGLDriver::setShaderConstant(const void* data, s32 location, E_SHADER_CONSTANT_TYPE type, u32 number)
{
	os::Printer::log("Error: Please call services->setShaderConstant(), not VideoDriver->setShaderConstant().");
}

void COpenGLDriver::setShaderTextures(const s32* textureIndices, s32 location, E_SHADER_CONSTANT_TYPE type, u32 number)
{
	os::Printer::log("Error: Please call services->setShaderTextures(), not VideoDriver->setShaderTextures().");
}


s32 COpenGLDriver::addHighLevelShaderMaterial(
    const c8* vertexShaderProgram,
    const c8* controlShaderProgram,
    const c8* evaluationShaderProgram,
    const c8* geometryShaderProgram,
    const c8* pixelShaderProgram,
    u32 patchVertices,
    E_MATERIAL_TYPE baseMaterial,
    IShaderConstantSetCallBack* callback,
    s32 userData,
    const c8* vertexShaderEntryPointName,
    const c8* controlShaderEntryPointName,
    const c8* evaluationShaderEntryPointName,
    const c8* geometryShaderEntryPointName,
    const c8* pixelShaderEntryPointName)
{
    s32 nr = -1;

	COpenGLSLMaterialRenderer* r = new COpenGLSLMaterialRenderer(
		this, nr,
		vertexShaderProgram, vertexShaderEntryPointName,
		pixelShaderProgram, pixelShaderEntryPointName,
		geometryShaderProgram, geometryShaderEntryPointName,
		controlShaderProgram,controlShaderEntryPointName,
		evaluationShaderProgram,evaluationShaderEntryPointName,
		patchVertices,callback,baseMaterial, userData);
	r->drop();
	return nr;
}

//! Adds a new material renderer to the VideoDriver, using GLSL to render geometry.
s32 COpenGLDriver::addHighLevelShaderMaterial(
	const c8* vertexShaderProgram,
	const c8* vertexShaderEntryPointName,
	const c8* pixelShaderProgram,
	const c8* pixelShaderEntryPointName,
	const c8* geometryShaderProgram,
	const c8* geometryShaderEntryPointName,
	IShaderConstantSetCallBack* callback,
	E_MATERIAL_TYPE baseMaterial,
	s32 userData)
{
    return addHighLevelShaderMaterial(vertexShaderProgram,0,0,geometryShaderProgram,pixelShaderProgram,
                                      3,baseMaterial,callback,userData,
                                      vertexShaderEntryPointName,"main","main",geometryShaderEntryPointName,pixelShaderEntryPointName);
}

//! Returns a pointer to the IVideoDriver interface. (Implementation for
//! IMaterialRendererServices)
IVideoDriver* COpenGLDriver::getVideoDriver()
{
	return this;
}



void COpenGLDriver::blitRenderTargets(IFrameBuffer* in, IFrameBuffer* out, bool copyDepth,
									core::recti srcRect, core::recti dstRect,
									bool bilinearFilter)
{
	GLuint inFBOHandle = 0;
	GLuint outFBOHandle = 0;


	if (srcRect.getArea()==0)
	{
	    if (in)
        {
            if (!static_cast<COpenGLFrameBuffer*>(in)->rebindRevalidate())
                return;

            bool firstAttached = true;
            uint32_t width,height;
            for (size_t i=0; i<EFAP_MAX_ATTACHMENTS; i++)
            {
                const IRenderable* rndrbl = in->getAttachment(i);
                if (!rndrbl)
                    continue;

                if (firstAttached)
                {
                    firstAttached = false;
                    width = rndrbl->getRenderableSize().Width;
                    height = rndrbl->getRenderableSize().Height;
                }
                else
                {
                    width = core::min_(rndrbl->getRenderableSize().Width,width);
                    height = core::min_(rndrbl->getRenderableSize().Height,height);
                }
            }
            if (firstAttached)
                return;

            srcRect = core::recti(0,0,width,height);
        }
        else
            srcRect = core::recti(0,0,ScreenSize.Width,ScreenSize.Height);
	}
	if (dstRect.getArea()==0)
	{
	    if (out)
        {
            if (!static_cast<COpenGLFrameBuffer*>(out)->rebindRevalidate())
                return;

            bool firstAttached = true;
            uint32_t width,height;
            for (size_t i=0; i<EFAP_MAX_ATTACHMENTS; i++)
            {
                const IRenderable* rndrbl = out->getAttachment(i);
                if (!rndrbl)
                    continue;

                if (firstAttached)
                {
                    firstAttached = false;
                    width = rndrbl->getRenderableSize().Width;
                    height = rndrbl->getRenderableSize().Height;
                }
                else
                {
                    width = core::min_(rndrbl->getRenderableSize().Width,width);
                    height = core::min_(rndrbl->getRenderableSize().Height,height);
                }
            }
            if (firstAttached)
                return;

            dstRect = core::recti(0,0,width,height);
        }
        else
            dstRect = core::recti(0,0,ScreenSize.Width,ScreenSize.Height);
	}
	if (srcRect==dstRect||copyDepth)
		bilinearFilter = false;

    setViewPort(dstRect);

    if (in)
        inFBOHandle = static_cast<COpenGLFrameBuffer*>(in)->getOpenGLName();
    if (out)
        outFBOHandle = static_cast<COpenGLFrameBuffer*>(out)->getOpenGLName();

    extGlBlitNamedFramebuffer(inFBOHandle,outFBOHandle,
                        srcRect.UpperLeftCorner.X,srcRect.UpperLeftCorner.Y,srcRect.LowerRightCorner.X,srcRect.LowerRightCorner.Y,
                        dstRect.UpperLeftCorner.X,dstRect.UpperLeftCorner.Y,dstRect.LowerRightCorner.X,dstRect.LowerRightCorner.Y,
						GL_COLOR_BUFFER_BIT|(copyDepth ? GL_DEPTH_BUFFER_BIT:0),
						bilinearFilter ? GL_LINEAR:GL_NEAREST);
}



//! Returns the maximum amount of primitives (mostly vertices) which
//! the device is able to render with one drawIndexedTriangleList
//! call.
u32 COpenGLDriver::getMaximalIndicesCount() const
{
	return MaxIndices;
}



//! Sets multiple render targets
bool COpenGLDriver::setRenderTarget(IFrameBuffer* frameBuffer, bool setNewViewport)
{
    if (frameBuffer==CurrentFBO)
        return true;

    if (!frameBuffer)
    {
        CurrentRendertargetSize = ScreenSize;
        extGlBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (CurrentFBO)
            CurrentFBO->drop();
        CurrentFBO = NULL;

        if (setNewViewport)
            setViewPort(core::recti(0,0,ScreenSize.Width,ScreenSize.Height));

        return true;
    }

    if (!frameBuffer->rebindRevalidate())
    {
        os::Printer::log("FBO revalidation failed!", ELL_ERROR);
        return false;
    }

    bool firstAttached = true;
    core::dimension2du newRTTSize;
    for (size_t i=0; i<EFAP_MAX_ATTACHMENTS; i++)
    {
        const IRenderable* attachment = frameBuffer->getAttachment(i);
        if (!attachment)
            continue;

        if (firstAttached)
        {
            newRTTSize = attachment->getRenderableSize();
            firstAttached = false;
        }
        else
        {
            newRTTSize.Width = core::min_(newRTTSize.Width,attachment->getRenderableSize().Width);
            newRTTSize.Height = core::min_(newRTTSize.Height,attachment->getRenderableSize().Height);
        }
    }

    if (firstAttached)
    {
        os::Printer::log("FBO has no attachments! (We don't support that OpenGL 4.3 feature yet!).", ELL_ERROR);
        return false;
    }
    CurrentRendertargetSize = newRTTSize;


    extGlBindFramebuffer(GL_FRAMEBUFFER, static_cast<COpenGLFrameBuffer*>(frameBuffer)->getOpenGLName());
    if (setNewViewport)
        setViewPort(core::recti(0,0,newRTTSize.Width,newRTTSize.Height));


    frameBuffer->grab();
    if (CurrentFBO)
        CurrentFBO->drop();
    CurrentFBO = static_cast<COpenGLFrameBuffer*>(frameBuffer);
    ResetRenderStates=true; //! OPTIMIZE: Needed?
    Transformation3DChanged=true; //! OPTIMIZE: Needed?


    return true;
}


// returns the current size of the screen or rendertarget
const core::dimension2d<u32>& COpenGLDriver::getCurrentRenderTargetSize() const
{
	if (CurrentRendertargetSize.Width == 0)
		return ScreenSize;
	else
		return CurrentRendertargetSize;
}


//! Clears the ZBuffer.
void COpenGLDriver::clearZBuffer(const float &depth)
{
    glDepthMask(GL_TRUE);
    LastMaterial.ZWriteEnable=true;

    if (CurrentFBO)
        extGlClearNamedFramebufferfv(CurrentFBO->getOpenGLName(),GL_DEPTH,0,&depth);
    else
        extGlClearNamedFramebufferfv(0,GL_DEPTH,0,&depth);
}

void COpenGLDriver::clearStencilBuffer(const int32_t &stencil)
{
    if (CurrentFBO)
        extGlClearNamedFramebufferiv(CurrentFBO->getOpenGLName(),GL_STENCIL,0,&stencil);
    else
        extGlClearNamedFramebufferiv(0,GL_STENCIL,0,&stencil);
}

void COpenGLDriver::clearZStencilBuffers(const float &depth, const int32_t &stencil)
{
    if (CurrentFBO)
        extGlClearNamedFramebufferfi(CurrentFBO->getOpenGLName(),GL_DEPTH_STENCIL,depth,stencil);
    else
        extGlClearNamedFramebufferfi(0,GL_DEPTH_STENCIL,depth,stencil);
}

void COpenGLDriver::clearColorBuffer(const E_FBO_ATTACHMENT_POINT &attachment, const int32_t* vals)
{
    if (attachment<EFAP_COLOR_ATTACHMENT0)
        return;

    if (CurrentFBO)
        extGlClearNamedFramebufferiv(CurrentFBO->getOpenGLName(),GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
    else
        extGlClearNamedFramebufferiv(0,GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
}
void COpenGLDriver::clearColorBuffer(const E_FBO_ATTACHMENT_POINT &attachment, const uint32_t* vals)
{
    if (attachment<EFAP_COLOR_ATTACHMENT0)
        return;

    if (CurrentFBO)
        extGlClearNamedFramebufferuiv(CurrentFBO->getOpenGLName(),GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
    else
        extGlClearNamedFramebufferuiv(0,GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
}
void COpenGLDriver::clearColorBuffer(const E_FBO_ATTACHMENT_POINT &attachment, const float* vals)
{
    if (attachment<EFAP_COLOR_ATTACHMENT0)
        return;

    if (CurrentFBO)
        extGlClearNamedFramebufferfv(CurrentFBO->getOpenGLName(),GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
    else
        extGlClearNamedFramebufferfv(0,GL_COLOR,attachment-EFAP_COLOR_ATTACHMENT0,vals);
}

void COpenGLDriver::clearScreen(const E_SCREEN_BUFFERS &buffer, const float* vals)
{
    switch (buffer)
    {
        case ESB_BACK_LEFT:
            extGlClearNamedFramebufferfv(0,GL_COLOR,0,vals);
            break;
        case ESB_BACK_RIGHT:
            extGlClearNamedFramebufferfv(0,GL_COLOR,0,vals);
            break;
        case ESB_FRONT_LEFT:
            extGlClearNamedFramebufferfv(0,GL_COLOR,0,vals);
            break;
        case ESB_FRONT_RIGHT:
            extGlClearNamedFramebufferfv(0,GL_COLOR,0,vals);
            break;
    }
}
void COpenGLDriver::clearScreen(const E_SCREEN_BUFFERS &buffer, const uint32_t* vals)
{
    switch (buffer)
    {
        case ESB_BACK_LEFT:
            extGlClearNamedFramebufferuiv(0,GL_COLOR,0,vals);
            break;
        case ESB_BACK_RIGHT:
            extGlClearNamedFramebufferuiv(0,GL_COLOR,0,vals);
            break;
        case ESB_FRONT_LEFT:
            extGlClearNamedFramebufferuiv(0,GL_COLOR,0,vals);
            break;
        case ESB_FRONT_RIGHT:
            extGlClearNamedFramebufferuiv(0,GL_COLOR,0,vals);
            break;
    }
}



//! Enable/disable a clipping plane.
void COpenGLDriver::enableClipPlane(u32 index, bool enable)
{
	if (index >= MaxUserClipPlanes)
		return;
	if (enable)
        glEnable(GL_CLIP_DISTANCE0 + index);
	else
		glDisable(GL_CLIP_DISTANCE0 + index);
}


core::dimension2du COpenGLDriver::getMaxTextureSize() const
{
	return core::dimension2du(MaxTextureSize, MaxTextureSize);
}


//! Convert E_PRIMITIVE_TYPE to OpenGL equivalent
GLenum COpenGLDriver::primitiveTypeToGL(scene::E_PRIMITIVE_TYPE type) const
{
	switch (type)
	{
		case scene::EPT_POINTS:
			return GL_POINTS;
		case scene::EPT_LINE_STRIP:
			return GL_LINE_STRIP;
		case scene::EPT_LINE_LOOP:
			return GL_LINE_LOOP;
		case scene::EPT_LINES:
			return GL_LINES;
		case scene::EPT_TRIANGLE_STRIP:
			return GL_TRIANGLE_STRIP;
		case scene::EPT_TRIANGLE_FAN:
			return GL_TRIANGLE_FAN;
		case scene::EPT_TRIANGLES:
			return GL_TRIANGLES;
		case scene::EPT_QUAD_STRIP:
			return GL_QUAD_STRIP;
		case scene::EPT_QUADS:
			return GL_QUADS;
		case scene::EPT_POINT_SPRITES:
#ifdef GL_ARB_point_sprite
			return GL_POINT_SPRITE_ARB;
#else
			return GL_POINTS;
#endif
	}
	return GL_TRIANGLES;
}


GLenum COpenGLDriver::getGLBlend(E_BLEND_FACTOR factor) const
{
	GLenum r = 0;
	switch (factor)
	{
		case EBF_ZERO:			r = GL_ZERO; break;
		case EBF_ONE:			r = GL_ONE; break;
		case EBF_DST_COLOR:		r = GL_DST_COLOR; break;
		case EBF_ONE_MINUS_DST_COLOR:	r = GL_ONE_MINUS_DST_COLOR; break;
		case EBF_SRC_COLOR:		r = GL_SRC_COLOR; break;
		case EBF_ONE_MINUS_SRC_COLOR:	r = GL_ONE_MINUS_SRC_COLOR; break;
		case EBF_SRC_ALPHA:		r = GL_SRC_ALPHA; break;
		case EBF_ONE_MINUS_SRC_ALPHA:	r = GL_ONE_MINUS_SRC_ALPHA; break;
		case EBF_DST_ALPHA:		r = GL_DST_ALPHA; break;
		case EBF_ONE_MINUS_DST_ALPHA:	r = GL_ONE_MINUS_DST_ALPHA; break;
		case EBF_SRC_ALPHA_SATURATE:	r = GL_SRC_ALPHA_SATURATE; break;
	}
	return r;
}

GLenum COpenGLDriver::getZBufferBits() const
{
	GLenum bits = 0;
	switch (Params.ZBufferBits)
	{
	case 16:
		bits = GL_DEPTH_COMPONENT16;
		break;
	case 24:
		bits = GL_DEPTH_COMPONENT24;
		break;
	case 32:
		bits = GL_DEPTH_COMPONENT32;
		break;
	default:
		bits = GL_DEPTH_COMPONENT;
		break;
	}
	return bits;
}


} // end namespace
} // end namespace

#endif // _IRR_COMPILE_WITH_OPENGL_

namespace irr
{
namespace video
{


// -----------------------------------
// WINDOWS VERSION
// -----------------------------------
#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
	io::IFileSystem* io, CIrrDeviceWin32* device)
{
#ifdef _IRR_COMPILE_WITH_OPENGL_
	COpenGLDriver* ogl =  new COpenGLDriver(params, io, device);
	if (!ogl->initDriver(device))
	{
		ogl->drop();
		ogl = 0;
	}
	return ogl;
#else
	return 0;
#endif // _IRR_COMPILE_WITH_OPENGL_
}
#endif // _IRR_COMPILE_WITH_WINDOWS_DEVICE_

// -----------------------------------
// MACOSX VERSION
// -----------------------------------
#if defined(_IRR_COMPILE_WITH_OSX_DEVICE_)
IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceMacOSX *device)
{
#ifdef _IRR_COMPILE_WITH_OPENGL_
	return new COpenGLDriver(params, io, device);
#else
	return 0;
#endif //  _IRR_COMPILE_WITH_OPENGL_
}
#endif // _IRR_COMPILE_WITH_OSX_DEVICE_

// -----------------------------------
// X11 VERSION
// -----------------------------------
#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceLinux* device)
{
#ifdef _IRR_COMPILE_WITH_OPENGL_
	COpenGLDriver* ogl =  new COpenGLDriver(params, io, device);
	if (!ogl->initDriver(device))
	{
		ogl->drop();
		ogl = 0;
	}
	return ogl;
#else
	return 0;
#endif //  _IRR_COMPILE_WITH_OPENGL_
}
#endif // _IRR_COMPILE_WITH_X11_DEVICE_


// -----------------------------------
// SDL VERSION
// -----------------------------------
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters& params,
		io::IFileSystem* io, CIrrDeviceSDL* device)
{
#ifdef _IRR_COMPILE_WITH_OPENGL_
	return new COpenGLDriver(params, io, device);
#else
	return 0;
#endif //  _IRR_COMPILE_WITH_OPENGL_
}
#endif // _IRR_COMPILE_WITH_SDL_DEVICE_

} // end namespace
} // end namespace

