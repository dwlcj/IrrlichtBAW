#ifndef __IRR_I_GPU_DESCRIPTOR_SET_H_INCLUDED__
#define __IRR_I_GPU_DESCRIPTOR_SET_H_INCLUDED__

#include "irr/asset/IDescriptorSet.h"

#include "IGPUBuffer.h"
#include "ITexture.h"
#include "irr/video/IGPUBufferView.h"
#include "irr/video/IGPUSampler.h"
#include "irr/video/IGPUDescriptorSetLayout.h"

namespace irr
{
namespace video
{

class IGPUDescriptorSet : public asset::IDescriptorSet<IGPUDescriptorSetLayout, IGPUBuffer, ITexture, IGPUBufferView, IGPUSampler>
{
	public:
		using asset::IDescriptorSet<IGPUDescriptorSetLayout, IGPUBuffer, ITexture, IGPUBufferView, IGPUSampler>::IDescriptorSet;

	protected:
		virtual ~IGPUDescriptorSet() = default;
};

}
}

#endif