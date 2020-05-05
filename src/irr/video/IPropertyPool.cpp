#include "irr/video/IPropertyPool.h"

#include <cstring>
#include <cstdio>

using namespace irr;
using namespace video;

//
static const char* IPropertyPool::CommonShaderSource = R"(
#version 430 core
#define _IRR_BUILTIN_PROPERTY_COPY_COUNT_ %d
#define _IRR_BUILTIN_PROPERTY_COPY_GROUP_SIZE_ %d
layout(local_size_x=_IRR_BUILTIN_PROPERTY_COPY_GROUP_SIZE_, local_size_y=_IRR_BUILTIN_PROPERTY_COPY_COUNT_) in;

layout(set=0,binding=0) readonly restrict buffer Indices
{
    uint elementCount;
    uint indices[];
};

#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_1_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=1
struct CopyType1
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_1_ CopyType1
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_2_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=2
struct CopyType2
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_2_ CopyType2
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_3_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=3
struct CopyType3
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_3_ CopyType3
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_4_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=4
struct CopyType4
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_4_ CopyType4
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_5_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=5
struct CopyType5
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_5_ CopyType5
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_6_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=6
struct CopyType6
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_6_ CopyType6
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_7_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=7
struct CopyType7
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_7_ CopyType7
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_8_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=8
struct CopyType8
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_8_ CopyType8
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_9_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=9
struct CopyType9
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_9_ CopyType9
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_10_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=10
struct CopyType10
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_10_ CopyType10
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_11_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=11
struct CopyType11
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_11_ CopyType11
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_12_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=12
struct CopyType12
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_12_ CopyType12
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_13_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=13
struct CopyType13
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_13_ CopyType13
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_14_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=14
struct CopyType14
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_14_ CopyType14
#endif
#endif
#ifndef _IRR_BUILTIN_PROPERTY_COPY_TYPE_15_
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=15
struct CopyType15
{
    uint data[%d];
};
#define _IRR_BUILTIN_PROPERTY_COPY_TYPE_15_ CopyType15
#endif
#endif


#define IRR_EVAL(ARG) ARG


#define DELCARE_PROPERTY(NUM) \
    layout(set=0,binding=2*(NUM)-1) readonly restrict buffer InData##NUM \
    { \
        IRR_EVAL(_IRR_BUILTIN_PROPERTY_COPY_TYPE_##NUM##_) in##NUM[]; \
    }; \
    layout(set=0,binding=2*(NUM)) writeonly restrict buffer OutData##NUM \
    { \
        IRR_EVAL(_IRR_BUILTIN_PROPERTY_COPY_TYPE_##NUM##_) out##NUM[]; \
    }

#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=1
    DELCARE_PROPERTY(1);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=2
    DELCARE_PROPERTY(2);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=3
    DELCARE_PROPERTY(3);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=4
    DELCARE_PROPERTY(4);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=5
    DELCARE_PROPERTY(5);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=6
    DELCARE_PROPERTY(6);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=7
    DELCARE_PROPERTY(7);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=8
    DELCARE_PROPERTY(8);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=9
    DELCARE_PROPERTY(9);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=10
    DELCARE_PROPERTY(10);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=11
    DELCARE_PROPERTY(11);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=12
    DELCARE_PROPERTY(12);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=13
    DELCARE_PROPERTY(13);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=14
    DELCARE_PROPERTY(14);
#endif
#ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=15
    DELCARE_PROPERTY(15);
#endif

#undef DELCARE_PROPERTY


shared uint workIndices[_IRR_BUILTIN_PROPERTY_COPY_GROUP_SIZE_];


void main()
{
    uint propID = gl_LocationInvocationID.y;
    if (propID==0u)
        workIndices[gl_LocationInvocationID.x] = indices[gl_GlobalInvocationID.x];
    barrier();
    memoryBarrierShared();

    uint index = gl_GlobalInvocationID.x;
    if (index>=elementCount)
        return;

#ifdef _IRR_BUILTIN_PROPERTY_COPY_DOWNLOAD
    uint inIndex = workIndices[gl_LocationInvocationID.x];
    uint outIndex = index;
#else
    uint inIndex = index;
    uint outIndex = workIndices[gl_LocationInvocationID.x];
#endif


#define COPY_PROPERTY(NUM) \
        case NUM: \
            out##NUM[outIndex] = in##NUM[inIndex]; \
            break

    switch(propID)
    {
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=1
            COPY_PROPERTY(1);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=2
            COPY_PROPERTY(2);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=3
            COPY_PROPERTY(3);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=4
            COPY_PROPERTY(4);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=5
            COPY_PROPERTY(5);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=6
            COPY_PROPERTY(6);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=7
            COPY_PROPERTY(7);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=8
            COPY_PROPERTY(8);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=9
            COPY_PROPERTY(9);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=10
            COPY_PROPERTY(10);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=11
            COPY_PROPERTY(11);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=12
            COPY_PROPERTY(12);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=13
            COPY_PROPERTY(13);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=14
            COPY_PROPERTY(14);
        #endif
        #ifdef _IRR_BUILTIN_PROPERTY_COPY_COUNT_>=15
            COPY_PROPERTY(15);
        #endif
    }

#undef COPY_PROPERTY
}
)";

//
core::FactoryAndStaticSafeMT<SharedState> IPropertyPool::sharedCopyState;

//
IGPUPipelineLayout* IPropertyPool::SharedState::getPipelineLayout()
{
    if (!pipelineLayout)
    {
        IGPUDescriptorSetLayout::SBinding bindings[MaxPropertiesPerCS];
        for (auto i=0; i<propCount; i++)
        {
            bindings[i].binding = i;
            bindings[i].type = asset::EDT_STORAGE_BUFFER_DYNAMIC;
            bindings[i].count = 1u;
            bindings[i].stageFlags = IGPUSpecializedShader::ESS_COMPUTE;
        }
        auto descriptorSetLayout = driver->createGPUDescriptorSetLayout(bindings,bindings+propCount);

        pipelineLayout = driver->createGPUPipelineLayout(nullptr,nullptr,std::move(descriptorSetLayout));
    }
    return pipelineLayout.get();
}

//
core::smart_refctd_ptr<IGPUComputePipeline> IPropertyPool::getCopyPipeline(IVideoDriver* driver, IGPUPipelineCache* pipelineCache, const PipelineKey& key, bool canCompileNew)
{
    const auto propCount = key.getPropertyCount();
    if (!propCount)
    {
        #ifdef _IRR_DEBUG
            assert(false);
        #endif
        return nullptr;
    }

    auto stateAndLock = sharedCopyState.getData(driver);
    auto& sharedState = stateAndLock.first;

    auto& realPipelineCache = sharedState.pipelines;
    auto found = realPipelineCache.find(key);
    if (found!= realPipelineCache.end())
        return found->second;

    if (!canCompileNew)
        return nullptr;

    constexpr auto MaxIntegerLiteralLen = 10u;
    const auto maxShaderLength = std::strlen(CommonShaderSource)+MaxIntegerLiteralLen*(2+MaxPropertiesPerCS)+1u;
    auto glslBuffer = core::make_smart_refctd_ptr<ICPUBuffer>(maxShaderLength);
    auto* bufferAsCharPtr = reinterpret_cast<char*>(glslBuffer->getPointer());
    std::snprintf(bufferAsCharPtr,maxShaderLength,CommonShaderSource,
        propCount,
        getWorkGroupSizeX(propCount),
        key.propertySizes[0]/MinimumPropertyAlignment,
        key.propertySizes[1]/MinimumPropertyAlignment,
        key.propertySizes[2]/MinimumPropertyAlignment,
        key.propertySizes[3]/MinimumPropertyAlignment,
        key.propertySizes[4]/MinimumPropertyAlignment,
        key.propertySizes[5]/MinimumPropertyAlignment,
        key.propertySizes[6]/MinimumPropertyAlignment,
        key.propertySizes[7]/MinimumPropertyAlignment,
        key.propertySizes[8]/MinimumPropertyAlignment,
        key.propertySizes[9]/MinimumPropertyAlignment,
        key.propertySizes[10]/MinimumPropertyAlignment,
        key.propertySizes[11]/MinimumPropertyAlignment,
        key.propertySizes[12]/MinimumPropertyAlignment,
        key.propertySizes[13]/MinimumPropertyAlignment,
        key.propertySizes[14]/MinimumPropertyAlignment
    );
    // TODO: improve this at some point w.r.t the memcpies
    auto cpushader = core::make_smart_refctd_ptr<asset::ICPUShader>(bufferAsCharPtr);

    auto shader = driver->createGPUShader(std::move(cpushader));
    auto specshader = driver->createGPUSpecializedShader(shader.get(),{nullptr,nullptr,"main",asset::ISpecializedShader::ESS_COMPUTE});

    auto pipeline = driver->createGPUComputePipeline(pipelineCache,core::smart_refctd_ptr<IGPUPipelineLayout>(sharedState.getPipelineLayout()),std::move(specshader));
    realPipelineCache.insert({key,core::smart_refctd_ptr(pipeline)});
    return pipeline;
}



}
}

#endif