#define _IRR_STATIC_LIB_
#include <irrlicht.h>


#include "../../ext/OptiX/Manager.h"

// cuda and optix stuff
#include "vector_types.h"
#include "surface_types.h"
#include "common.h"

/**
This example just shows a screen which clears to red,
nothing fancy, just to show that Irrlicht links fine
**/
using namespace irr;

/*
The start of the main function starts like in most other example. We ask the
user for the desired renderer and start it up.
*/
int main()
{
	// create device with full flexibility over creation parameters
	// you can add more parameters if desired, check irr::SIrrlichtCreationParameters
	irr::SIrrlichtCreationParameters params;
	params.Bits = 24; //may have to set to 32bit for some platforms
	params.ZBufferBits = 24; //we'd like 32bit here
	params.DriverType = video::EDT_OPENGL; //! Only Well functioning driver, software renderer left for sake of 2D image drawing
	params.WindowSize = core::dimension2d<uint32_t>(1280, 720);
	params.Fullscreen = false;
	params.Vsync = false;
	params.Doublebuffer = true;
	params.Stencilbuffer = false; //! This will not even be a choice soon
	params.AuxGLContexts = 16;
	IrrlichtDevice* device = createDeviceEx(params);

	if (device == 0)
		return 1; // could not create selected driver.

	auto filesystem = device->getFileSystem();
	video::IVideoDriver* driver = device->getVideoDriver();
	auto optixmgr = irr::ext::OptiX::Manager::create(driver,filesystem);
	if (!optixmgr)
		return 2;

	OptixDeviceContextOptions context_options = {};
	auto optixctx = optixmgr->createContext(0u,context_options);
	if (!optixctx)
		return 3;

#if 0
	// Specify options for the build. We use default options for simplicity.
	OptixAccelBuildOptions accel_options = {};
	accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;
	accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;

	// Triangle build input: simple list of three vertices
	const float vertices_data[3][3] =
	{
		{ -0.5f, -0.5f, 0.0f },
		{  0.5f, -0.5f, 0.0f },
		{  0.0f,  0.5f, 0.0f }
	};

	// Allocate and copy device memory for our input triangle vertices
	const uint32_t numVertices = 3u;
	const size_t vertices_size = sizeof(float)*numVertices*3ull;
	cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer> vertices_link;
	vertices_link.obj = core::smart_refctd_ptr<video::IGPUBuffer>(driver->createFilledDeviceLocalGPUBufferOnDedMem(vertices_size,vertices_data), core::dont_grab);
	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::registerBuffer(&vertices_link,CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY)))
		return 3;

	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsMapResources(1u, &vertices_link.cudaHandle, stream)))
		return 4;

	CUdeviceptr d_vertices = 0;
	{
		size_t tmp = vertices_size;
		if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsResourceGetMappedPointer_v2(&d_vertices, &tmp, vertices_link.cudaHandle)))
			return 5;
	}

	// Populate the build input struct with our triangle data as well as
	// information about the sizes and types of our data
	const uint32_t triangle_input_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
	OptixBuildInput triangle_input = {};
	triangle_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
	triangle_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
	triangle_input.triangleArray.numVertices = numVertices;
	triangle_input.triangleArray.vertexBuffers = &d_vertices;
	triangle_input.triangleArray.flags = triangle_input_flags;
	triangle_input.triangleArray.numSbtRecords = 1;

	// Query OptiX for the memory requirements for our GAS 
	OptixAccelBufferSizes gas_buffer_sizes;
	optixAccelComputeMemoryUsage(
		optixctx->getOptiXHandle(),         // The device context we are using
		&accel_options,
		&triangle_input, // Describes our geometry
		1,               // Number of build inputs, could have multiple
		&gas_buffer_sizes);

	// Allocate device memory for the scratch space buffer as well
	// as the GAS itself
	CUdeviceptr d_temp_buffer_gas, d_gas_output_buffer;
	cuda::CCUDAHandler::cuda.pcuMemAlloc_v2(&d_temp_buffer_gas,gas_buffer_sizes.tempSizeInBytes);
	cuda::CCUDAHandler::cuda.pcuMemAlloc_v2(&d_gas_output_buffer,gas_buffer_sizes.outputSizeInBytes);

	// Now build the GAS
	OptixTraversableHandle gas_handle = 0u;
	optixAccelBuild(
		optixctx->getOptiXHandle(),         // The device context we are using
		stream,           // CUDA stream
		&accel_options,
		&triangle_input,
		1,           // num build inputs
		d_temp_buffer_gas,
		gas_buffer_sizes.tempSizeInBytes,
		d_gas_output_buffer,
		gas_buffer_sizes.outputSizeInBytes,
		&gas_handle, // Output handle to the struct
		nullptr,     // emitted property list
		0);         // num emitted properties

	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsUnmapResources(1, &vertices_link.cudaHandle, stream)))
		return 6;

	// We can now free scratch space used during the build
	cuda::CCUDAHandler::cuda.pcuMemFree_v2(d_temp_buffer_gas);
#endif
	// Pipeline options must be consistent for all modules used in a
	// single pipeline
	OptixPipelineCompileOptions pipeline_compile_options = {};
	pipeline_compile_options.usesMotionBlur = false;
	pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;  // TODO: should be OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW;

	// This option is important to ensure we compile code which is optimal
	// for our scene hierarchy. We use a single GAS – no instancing or
	// multi-level hierarchies
	pipeline_compile_options.traversableGraphFlags =
		OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS | OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
	
	/*
	// Our device code uses 3 payload registers (r,g,b output value)
	pipeline_compile_options.numPayloadValues = 3;
	*/
	pipeline_compile_options.numPayloadValues = 2;
	pipeline_compile_options.numAttributeValues = 2;

	// This is the name of the param struct variable in our device code
	pipeline_compile_options.pipelineLaunchParamsVariableName = "params";


	// module
	auto file = filesystem->createAndOpenFile("../optixTriangle.cu");
	auto module = optixctx->compileModuleFromFile(file,pipeline_compile_options);//,module_compile_options);
	file->drop();

	if (!module)
		return 7;
	
    //
    // Create program groups, including NULL miss and hitgroups
    //
    OptixProgramGroup raygen_prog_group   = nullptr;
    OptixProgramGroup miss_prog_group     = nullptr;
    OptixProgramGroup hitgroup_prog_group = nullptr;
    {
        OptixProgramGroupOptions program_group_options   = {}; // Initialize to zeros

        OptixProgramGroupDesc raygen_prog_group_desc  = {}; //
        raygen_prog_group_desc.kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        raygen_prog_group_desc.raygen.module            = module->getOptiXHandle();
        raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__draw_solid_color";
        optixProgramGroupCreate(
                    optixctx->getOptiXHandle(),
                    &raygen_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    nullptr,nullptr,
                    &raygen_prog_group
                    );

        // Leave miss group's module and entryfunc name null
        OptixProgramGroupDesc miss_prog_group_desc = {};
        miss_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
        optixProgramGroupCreate(
                    optixctx->getOptiXHandle(),
                    &miss_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    nullptr,nullptr,
                    &miss_prog_group
                    );

        // Leave hit group's module and entryfunc name null
        OptixProgramGroupDesc hitgroup_prog_group_desc = {};
        hitgroup_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        optixProgramGroupCreate(
                    optixctx->getOptiXHandle(),
                    &hitgroup_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    nullptr,nullptr,
                    &hitgroup_prog_group
                    );
    }

    //
    // Link pipeline
    //
	OptixPipelineLinkOptions pipeline_link_options = {};
	pipeline_link_options.maxTraceDepth = 5;
	pipeline_link_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
	pipeline_link_options.overrideUsesMotionBlur = false;
    OptixPipeline pipeline = nullptr;
    {
        OptixProgramGroup program_groups[] = { raygen_prog_group };

        optixPipelineCreate(
                    optixctx->getOptiXHandle(),
                    &pipeline_compile_options,
                    &pipeline_link_options,
                    program_groups,
                    sizeof( program_groups ) / sizeof( program_groups[0] ),
					nullptr, nullptr,
                    &pipeline
                    );
    }

	CUstream stream = optixmgr->getDeviceStream(0u);

    //
    // Set up shader binding table
    //
	constexpr size_t sbt_count = 3ull;
	cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer> sbt_record_buffers[sbt_count];
    OptixShaderBindingTable sbt = {};
    {
		typedef ext::OptiX::SbtRecord<RayGenData> RayGenSbtRecord;
		typedef ext::OptiX::SbtRecord<int>        MissSbtRecord;
		typedef ext::OptiX::SbtRecord<int>        HitGroupSbtRecord;

		auto createRecord = [&](const auto& recordData) -> cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer>
		{
			cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer> outLink = core::smart_refctd_ptr<video::IGPUBuffer>(driver->createFilledDeviceLocalGPUBufferOnDedMem(sizeof(recordData),&recordData), core::dont_grab);
			if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::registerBuffer(&outLink,CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY)))
				return {};
			if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::acquireAndGetPointers(&outLink,&outLink+1,stream)))
				return {};
			assert(outLink.asBuffer.pointer%OPTIX_SBT_RECORD_ALIGNMENT==0ull);
			return outLink;
		};


        RayGenSbtRecord rg_sbt;
        optixSbtRecordPackHeader( raygen_prog_group, &rg_sbt );
        rg_sbt.data = {0.462f, 0.725f, 0.f};

		MissSbtRecord ms_sbt;
        optixSbtRecordPackHeader( miss_prog_group, &ms_sbt );

		HitGroupSbtRecord hg_sbt;
		optixSbtRecordPackHeader(hitgroup_prog_group, &hg_sbt);

		sbt_record_buffers[0] = createRecord(rg_sbt);
		sbt_record_buffers[1] = createRecord(ms_sbt);
		sbt_record_buffers[2] = createRecord(hg_sbt);

        sbt.raygenRecord                = sbt_record_buffers[0].asBuffer.pointer;
        sbt.missRecordBase              = sbt_record_buffers[1].asBuffer.pointer;
        sbt.missRecordStrideInBytes     = sizeof( MissSbtRecord );
        sbt.missRecordCount             = 1;
        sbt.hitgroupRecordBase          = sbt_record_buffers[2].asBuffer.pointer;
        sbt.hitgroupRecordStrideInBytes = sizeof( HitGroupSbtRecord );
        sbt.hitgroupRecordCount         = 1;
    }


	// params and output
	size_t bufferSizes[] = {sizeof(Params),sizeof(uchar4)*params.WindowSize.getArea()};
	constexpr auto buffersToAcquireCount = sizeof(bufferSizes)/sizeof(*bufferSizes);
	cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer> buffers[2];
	for (auto i=0; i<buffersToAcquireCount; i++)
	{
		buffers[i] = core::smart_refctd_ptr<video::IGPUBuffer>(driver->createDeviceLocalGPUBufferOnDedMem(bufferSizes[i]),core::dont_grab);
		if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::registerBuffer(buffers+i)))
			return 8;
	}

	Params p;
	p.image = {};
	p.image_width = params.WindowSize.Width;

	// screen output
	cuda::CCUDAHandler::GraphicsAPIObjLink<video::ITexture> m_accumulation = driver->createGPUTexture(video::ITexture::ETT_2D, &params.WindowSize.Width, 1, asset::EF_R32G32B32A32_SFLOAT);
	irr::video::IFrameBuffer* m_colorBuffer = driver->addFrameBuffer();
	{
		m_colorBuffer->attach(video::EFAP_COLOR_ATTACHMENT0, m_accumulation.getObject());
	}

	uint8_t stackScratch[1024];
    while (device->run())
    {
		// raytrace part
		{
			if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::acquireAndGetPointers(buffers,buffers+buffersToAcquireCount,stream)))
				return 9;

			auto outputCUDAPtrRef = cuda::CCUDAHandler::cast_CUDA_ptr<uchar4>(buffers[1].asBuffer.pointer);
			if (p.image!=outputCUDAPtrRef)
			{
				p.image = outputCUDAPtrRef;
				if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuMemcpyHtoDAsync_v2(buffers[0].asBuffer.pointer,&p,sizeof(Params),stream)))
					return 10;
			}
			optixLaunch( pipeline, stream, buffers[0].asBuffer.pointer, sizeof(Params), &sbt, params.WindowSize.Width, params.WindowSize.Height, /*depth=*/1 );
			
			if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::releaseResourcesToGraphics(stackScratch,buffers,buffers+buffersToAcquireCount,stream)))
				return 11;
		}

		driver->beginScene(false, false);
		
		auto glbuf = static_cast<video::COpenGLBuffer*>(buffers[1].getObject());
		auto gltex = static_cast<video::COpenGLFilterableTexture*>(m_accumulation.getObject());
		video::COpenGLExtensionHandler::extGlBindBuffer(GL_PIXEL_UNPACK_BUFFER,glbuf->getOpenGLName());
		video::COpenGLExtensionHandler::extGlTextureSubImage2D(gltex->getOpenGLName(),gltex->getOpenGLTextureType(),0,0,0, params.WindowSize.Width,params.WindowSize.Height,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
		video::COpenGLExtensionHandler::extGlBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);

		auto oldVP = driver->getViewPort();
		driver->blitRenderTargets(m_colorBuffer, nullptr, false, false, {}, {}, true);
		driver->setViewPort(oldVP);

		driver->endScene();
    }
	driver->removeFrameBuffer(m_colorBuffer);

	// release all resources
	/// optixAccelDestroy?
	//cuda::CCUDAHandler::cuda.pcuMemFree_v2(d_gas_output_buffer);
    {
		if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::releaseResourcesToGraphics(stackScratch,sbt_record_buffers,sbt_record_buffers+sbt_count,stream)))
			return 12;
		for (auto i=0; i<sbt_count; i++)
			sbt_record_buffers[i] = {};

        optixPipelineDestroy(pipeline);
    }

	device->drop();

	return 0;
}
