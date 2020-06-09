/* irrlicht.h -- interface of the 'IrrlichtBAW Engine'

  Copyright (C) 2019 - DevSH Graphics Programming Sp. z O.O.

  This software is provided 'as-is', under the Apache 2.0 license,
  without any express or implied warranty.  In no event will the authors
  be held liable for any damages arising from the use of this software.

  See LICENSE.md for full licensing information.

  Please note that the IrrlichtBAW Engine is based in part on the work of others,
  this means that if you use the IrrlichtBAW Engine in your product,
  you must acknowledge somewhere in your documentation that you've used their code.
  See README.md for all mentions of 3rd party software used.
*/

#ifndef __IRRLICHT_H_INCLUDED__
#define __IRRLICHT_H_INCLUDED__

// core lib
#include "irr/core/core.h"

// system lib (fibers, mutexes, file I/O operations) [DEPENDS: core]
#include "irr/system/system.h"
// should we move "core/parallel" to "system/parallel"

// asset lib (importing and exporting meshes, textures and shaders) [DEPENDS: system]
#include "irr/asset/asset.h"
// ui lib (window set up, software blit, joysticks, multi-touch, keyboard, etc.) [DEPENDS: system]
#include "irr/ui/ui.h"

// video lib (access to Graphics API, remote rendering, etc) [DEPENDS: asset, (optional) ui]
#include "irr/video/video.h"

// scene lib (basic rendering, culling, scene graph etc.) [DEPENDS: video, ui]
#include "irr/scene/scene.h"


#include "IrrCompileConfig.h"
#include "aabbox3d.h"
#include "vector2d.h"
#include "vector3d.h"
#include "vectorSIMD.h"
#include "line3d.h"
#include "matrix4SIMD.h"
#include "position2d.h"
#include "quaternion.h"
#include "rect.h"
#include "dimension2d.h"
#include "EDriverTypes.h"
#include "ESceneNodeAnimatorTypes.h"
#include "ESceneNodeTypes.h"
#include "IAnimatedMesh.h"
#include "IAnimatedMeshSceneNode.h"
#include "ICameraSceneNode.h"
#include "ICursorControl.h"
#include "IDummyTransformationSceneNode.h"
#include "IEventReceiver.h"
#include "IFileList.h"
#include "IFileSystem.h"
#include "ILogger.h"
#include "IOSOperator.h"
#include "IReadFile.h"
#include "IrrlichtDevice.h"
#include "path.h"
#include "ISceneManager.h"
#include "ISceneNode.h"
#include "ISceneNodeAnimator.h"
#include "ISceneNodeAnimatorCameraFPS.h"
#include "ISceneNodeAnimatorCameraMaya.h"
#include "ISkinnedMeshSceneNode.h"
#include "ITimer.h"
#include "IVideoDriver.h"
#include "IWriteFile.h"
#include "Keycodes.h"
#include "splines.h"


#include "SColor.h"
#include "SCollisionEngine.h"
#include "SExposedVideoData.h"
#include "SIrrCreationParameters.h"
#include "SKeyMap.h"
#include "SViewFrustum.h"


#include "SIrrCreationParameters.h"

//! Everything in the Irrlicht Engine can be found in this namespace.
namespace irr
{
	//! Creates an Irrlicht device with the option to specify advanced parameters.
	/** Usually you should use createDevice() for creating an Irrlicht Engine device.
	Use this function only if you wish to specify advanced parameters like a window
	handle in which the device should be created.
	\param parameters: Structure containing advanced parameters for the creation of the device.
	See irr::SIrrlichtCreationParameters for details.
	\return Returns pointer to the created IrrlichtDevice or null if the
	device could not be created. */
	core::smart_refctd_ptr<IrrlichtDevice> createDeviceEx(const SIrrlichtCreationParameters& parameters);


	// THE FOLLOWING IS AN EMPTY LIST OF ALL SUB NAMESPACES
	// EXISTING ONLY FOR THE DOCUMENTATION SOFTWARE DOXYGEN.

	//! Basic classes such as vectors, planes, arrays, lists, and so on can be found in this namespace.
	namespace core
	{
	}

	//! This namespace provides interfaces for input/output: Reading and writing files, accessing zip archives, faking files ...
	namespace io
	{
	}
    
    //! All asset loading and mutation is performed here: Loading and Saving Images, Models, Shaders, Mesh and Texture CPU manipulation ...
    namespace asset
    {
    }

	//! All scene management can be found in this namespace: scene graph, scene nodes, cameras, animation, etc...
	namespace scene
	{
	}

	//! The video namespace contains classes for accessing the graphics API
	namespace video
	{
	}
}

/*! \file irrlicht.h
	\brief Main header file of the irrlicht, the only file needed to include.
*/

#endif

