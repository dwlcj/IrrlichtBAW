#define _IRR_STATIC_LIB_
#include <irrlicht.h>

#include "../common/QToQuitEventReceiver.h"

using namespace irr;
using namespace core;
using namespace asset;
using namespace video;


struct DrawData
{
	uint32_t count;
	uint32_t instanceCount;
	uint32_t firstIndex;
	uint32_t baseVertex;
	uint32_t baseInstance;
	core::matrix3x4SIMD instanceTransform;
};

struct IndirectDrawInfo
{
	core::smart_refctd_ptr<IGPURenderpassIndependentPipeline> pipeline;

	core::vector<DrawData> indirectDrawData;
	core::smart_refctd_ptr<IGPUBuffer> indirectDrawBuffer;

	_IRR_STATIC_INLINE_CONSTEXPR uint32_t MaxTexturesPerDraw = 2u; // need full bindless (by handle) to handle more than 32 textures
	core::smart_refctd_ptr<IGPUDescriptorSet> textureSet;

	core::vector<IGPUMeshBuffer*> backup;
};


struct IndirectDrawKey
{
	IndirectDrawKey(video::IGPURenderpassIndependentPipeline* _pipeline, uint32_t* vertexBindingOffsets) : pipeline(_pipeline)
	{
		for (auto i=0u; i<SVertexInputParams::MAX_ATTR_BUF_BINDING_COUNT; i++)
		{
			offset[i] = vertexBindingOffsets[i];
		}
	}

	inline bool operator<(const IndirectDrawKey& other) const
	{
		if (pipeline<other.pipeline)
			return true;
		
		if (pipeline!=other.pipeline)
			return true;

		// parents equal, now compare children
		for (auto i=0u; i<SVertexInputParams::MAX_ATTR_BUF_BINDING_COUNT; i++)
		{
			if (offset[i] < other.offset[i])
				return true;
			if (offset[i] > other.offset[i])
				return false;
		}
		return false;
	}

	inline bool operator!=(const IndirectDrawKey& other) const
	{
		if (pipeline!=other.pipeline)
			return true;

		for (auto i=0u; i<SVertexInputParams::MAX_ATTR_BUF_BINDING_COUNT; i++)
		{
			if (offset[i]!=other.offset[i])
				return true;
		}
		return false;
	}

	inline bool operator==(const IndirectDrawKey& other) const
	{
		return !((*this) != other);
	}

	IGPURenderpassIndependentPipeline* pipeline;
	uint32_t offset[SVertexInputParams::MAX_ATTR_BUF_BINDING_COUNT];
};

int main()
{
	// create device with full flexibility over creation parameters
	// you can add more parameters if desired, check irr::SIrrlichtCreationParameters
	irr::SIrrlichtCreationParameters params;
	params.Bits = 24; //may have to set to 32bit for some platforms
	params.ZBufferBits = 24; //we'd like 32bit here
	params.DriverType = video::EDT_OPENGL; //! Only Well functioning driver, software renderer left for sake of 2D image drawing
	params.WindowSize = dimension2d<uint32_t>(1280, 720);
	params.Fullscreen = false;
	params.Vsync = false;
	params.Doublebuffer = true;
	params.Stencilbuffer = false; //! This will not even be a choice soon
	auto device = createDeviceEx(params);

	if (!device)
		return 1; // could not create selected driver.


	QToQuitEventReceiver receiver;
	device->setEventReceiver(&receiver);


	auto* am = device->getAssetManager();
	
	IAssetLoader::SAssetLoadParams lp;
	auto vertexShaderBundle = am->getAsset("../mesh.vert",lp);
	auto meshShaderBundle = am->getAsset("../mesh.frag",lp);


	video::IVideoDriver* driver = device->getVideoDriver();
	auto 
		auto cpumesh = core::smart_refctd_ptr_static_cast<asset::ICPUMesh>(*asset.getContents().first);
		auto gpumesh = driver->getGPUObjectsFromAssets(&cpumesh.get(), (&cpumesh.get()) + 1)->operator[](0);


	//


	io::IFileSystem* fs = device->getFileSystem();


	//! Load big-ass sponza model
	// really want to get it working with a "../../media/sponza.zip?sponza.obj" path handling
	fs->addFileArchive("../../media/sponza.zip");





	using pipeline_type = asset::IMeshDataFormatDesc<video::IGPUBuffer>;
	core::map<IndirectDrawKey, IndirectDrawInfo> indirectDraws;
	//{
		auto* qnc = am->getMeshManipulator()->getQuantNormalCache();
		//loading cache from file
		qnc->loadNormalQuantCacheFromFile<E_QUANT_NORM_CACHE_TYPE::Q_2_10_10_10>(fs, "../../tmp/normalCache101010.sse", true);

		asset::IAssetLoader::SAssetLoadParams lparams; // crashes OBJ loader with those (0u, nullptr, asset::IAssetLoader::ECF_DONT_CACHE_REFERENCES);
		auto asset = am->getAsset("sponza.obj", lparams);
		auto cpumesh = core::smart_refctd_ptr_static_cast<asset::ICPUMesh>(*asset.getContents().first);
		auto gpumesh = driver->getGPUObjectsFromAssets(&cpumesh.get(), (&cpumesh.get()) + 1)->operator[](0);
		
        //! cache results -- speeds up mesh generation on second run
        qnc->saveCacheToFile(E_QUANT_NORM_CACHE_TYPE::Q_2_10_10_10, fs, "../../tmp/normalCache101010.sse");


		for (auto i = 0u; i < gpumesh->getMeshBufferCount(); i++)
		{
			auto* mb = gpumesh->getMeshBuffer(i);
			pipeline_type* pipeline = mb->getMeshDataAndFormat();
			const auto& descriptors = mb->getMaterial();

			auto& info = indirectDraws[IndirectDrawKey(pipeline)];
			if (info.indirectDrawData.size() == 0u)
			{
				info.material = mb->getMaterial();
				info.material.MaterialType = newMaterialType;
				info.pipeline = core::smart_refctd_ptr<asset::IMeshDataFormatDesc<video::IGPUBuffer> >(pipeline);
				info.primitiveType = mb->getPrimitiveType();
				info.indexType = mb->getIndexType();
				assert(info.indexType != asset::EIT_UNKNOWN);
			}
			else
			{
				assert(info.primitiveType == mb->getPrimitiveType());
				assert(info.indexType == mb->getIndexType());

				assert(info.backup.front()->getMeshDataAndFormat()->getIndexBuffer() == pipeline->getIndexBuffer());
				for (auto j = 0u; j < asset::EVAI_COUNT; j++)
				{
					auto attr = static_cast<asset::E_VERTEX_ATTRIBUTE_ID>(j);
					assert(info.backup.front()->getMeshDataAndFormat()->getMappedBuffer(attr) == pipeline->getMappedBuffer(attr));
					assert(info.backup.front()->getMeshDataAndFormat()->getMappedBufferOffset(attr) == pipeline->getMappedBufferOffset(attr));
					assert(info.backup.front()->getMeshDataAndFormat()->getMappedBufferStride(attr) == pipeline->getMappedBufferStride(attr));
				}
			}

			DrawElementsIndirectCommand cmd;
			cmd.count = mb->getIndexCount();
			cmd.instanceCount = mb->getInstanceCount();
			cmd.firstIndex = mb->getIndexBufferOffset()/(info.indexType!=asset::EIT_32BIT ? sizeof(uint16_t):sizeof(uint32_t));
			cmd.baseVertex = mb->getBaseVertex();
			cmd.baseInstance = mb->getBaseInstance();
			info.indirectDrawData.push_back(std::move(cmd));

			decltype(IndirectDrawInfo::textures)::value_type textures;
			for (uint32_t i=0u; i<IndirectDrawInfo::MaxTexturesPerDraw; i++)
				textures[i] = mb->getMaterial().TextureLayer[i].Texture;
			info.textures.push_back(std::move(textures));

			info.backup.emplace_back(mb);
		}
	//}
	for (auto& draw : indirectDraws)
	{
		draw.second.indirectDrawBuffer = core::smart_refctd_ptr<video::IGPUBuffer>(driver->createFilledDeviceLocalGPUBufferOnDedMem(draw.second.indirectDrawData.size()*sizeof(DrawElementsIndirectCommand),draw.second.indirectDrawData.data()));
	}


	scene::ISceneManager* smgr = device->getSceneManager();
	scene::ICameraSceneNode* camera =
		smgr->addCameraSceneNodeFPS(0, 100.0f, 1.f);
	camera->setPosition(core::vector3df(-4, 0, 0));
	camera->setTarget(core::vector3df(0, 0, 0));
	camera->setNearValue(1.f);
	camera->setFarValue(10000.0f);
	smgr->setActiveCamera(camera);

	device->getCursorControl()->setVisible(false);

	uint64_t lastFPSTime = 0;

	while (device->run() && receiver.keepOpen())
	{
		driver->beginScene(true, true, video::SColor(255, 0, 0, 255));

		//! Draw the view
		smgr->drawAll();

		for (auto& draw : indirectDraws)
		{
			auto& info = draw.second;
			if (!info.indirectDrawBuffer)
				continue; 

			driver->bindGraphicsPipeline(info.pipeline.get());
			driver->bindDescriptorSets(EPBP_GRAPHICS,info.pipeline->getLayout(),0u,1u,&info.textureSet.get(),nullptr);
			driver->drawIndexedIndirect(, mode, indexT, indexBuffer, info.indirectDrawBuffer.get(), 0, info.indirectDrawBuffer->getSize()/sizeof(DrawElementsIndirectCommand), sizeof(DrawElementsIndirectCommand));
		}

		driver->endScene();

		// display frames per second in window title
		uint64_t time = device->getTimer()->getRealTime();
		if (time - lastFPSTime > 1000)
		{
			std::wostringstream sstr;
			sstr << L"Builtin Nodes Demo - Irrlicht Engine FPS:" << driver->getFPS() << " PrimitvesDrawn:" << driver->getPrimitiveCountDrawn();

			device->setWindowCaption(sstr.str().c_str());
			lastFPSTime = time;
		}
	}


	device->drop();

	return 0;
}
