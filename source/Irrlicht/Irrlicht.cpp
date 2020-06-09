// Copyright (C) 2002-2011 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IrrCompileConfig.h"

static const char* const copyright = "Irrlicht Engine (c) 2002-2011 Nikolaus Gebhardt";

#ifdef _IRR_WINDOWS_
	#include <windows.h>
	#if defined(_IRR_DEBUG) && !defined(__GNUWIN32__) && !defined(_WIN32_WCE)
		#include <crtdbg.h>
	#endif // _IRR_DEBUG
#endif

#include "irrlicht.h"
#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
#include "CIrrDeviceWin32.h"
#endif

#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
#include "CIrrDeviceLinux.h"
#endif

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
#include "CIrrDeviceSDL.h"
#endif

namespace irr
{

	core::smart_refctd_ptr<IrrlichtDevice> createDeviceEx(const SIrrlichtCreationParameters& params)
	{
		core::smart_refctd_ptr<IrrlichtDevice> dev;

#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_
		dev = core::make_smart_refctd_ptr<CIrrDeviceWin32>(params);
#endif
#ifdef _IRR_COMPILE_WITH_X11_DEVICE_
		dev = core::make_smart_refctd_ptr<CIrrDeviceLinux>(params);
#endif
#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_
		dev = core::make_smart_refctd_ptr<CIrrDeviceSDL>(params);
#endif

		if (dev && !dev->getVideoDriver() && params.DriverType != video::EDT_NULL)
		{
			dev->closeDevice(); // destroy window
			dev->run(); // consume quit message
			dev = nullptr;
		}

		return dev;
	}

} // end namespace irr


#if defined(_IRR_WINDOWS_API_)

BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved )
{
	// _crtBreakAlloc = 139;

    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			#if defined(_IRR_DEBUG) && !defined(__GNUWIN32__) && !defined(__BORLANDC__)
				_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
			#endif
			break;
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

#endif // defined(_IRR_WINDOWS_)

