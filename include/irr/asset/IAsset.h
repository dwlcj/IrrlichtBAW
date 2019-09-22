#ifndef __IRR_I_ASSET_H_INCLUDED__
#define __IRR_I_ASSET_H_INCLUDED__

#include <string>
#include "irr/core/core.h"

namespace irr
{
namespace asset
{

class IAssetManager;

class IAssetMetadata : public core::IReferenceCounted
{
protected:
    virtual ~IAssetMetadata() = default;

public:
    //! this could actually be reworked to something more usable
    virtual const char* getLoaderName() = 0;
};

//! An interface class representing any kind of cpu-objects with capability of being cached
/**
	Actually it can be anything like cpu-side meshes scenes, texture data and material pipelines, 
	but must be serializable into/from .baw file format, unless it comes from an extension (irr::ext), 
	so forth cached. There are different asset types you can find at IAsset::E_TYPE. 
	IAsset doesn't provide direct instantiation (virtual destructor), much like IReferenceCounted.

	Every asset must be loaded by a particular class derived from IAssetLoader.
	
	@see IAssetLoader
	@see IAssetManager
	@see IAssetWriter
	@see IReferenceCounted
*/

class IAsset : virtual public core::IReferenceCounted
{
public:

	/**
		Values of E_TYPE represents an Asset type.

		Entire enum as type represents a register with bit flags,
		which represent an available Asset type.

		@see IAsset

		ET_BUFFER represents asset::ICPUBuffer
		ET_SUB_IMAGE represents asset::CImageData
		ET_IMAGE represents ICPUTexture
		ET_SUB_MESH represents
		ET_MESH represents
		ET_SKELETON represents
		ET_KEYFRAME_ANIMATION represents
		ET_SHADER represents
		ET_SPECIALIZED_SHADER represents
		ET_MESH_DATA_DESCRIPTOR represents
		ET_GRAPHICS_PIPELINE represents
		ET_SCENE represents
		ET_IMPLEMENTATION_SPECIFIC_METADATA represents a special value used for things like terminating lists of this enum

		Pay attention that a register is limited to 32 bits.

	*/

    enum E_TYPE : uint64_t
    {
        //! asset::ICPUBuffer
        ET_BUFFER = 1u<<0u,
        //! asset::CImageData - maybe rename to asset::CSubImageData
        ET_SUB_IMAGE = 1u<<1u,
        //! asset::ICPUTexture
        ET_IMAGE = 1u<<2u,
        //! asset::ICPUMeshBuffer
        ET_SUB_MESH = 1u<<3u,
        //! asset::ICPUMesh
        ET_MESH = 1u<<4u,
        //! asset::ICPUSkeleton - to be done by splitting CFinalBoneHierarchy
        ET_SKELETON = 1u<<5u,
        //! asset::ICPUKeyframeAnimation - from CFinalBoneHierarchy
        ET_KEYFRAME_ANIMATION = 1u<<6u,
        //! asset::ICPUShader
        ET_SHADER = 1u<<7u,
        //! asset::ICPUSpecializedShader
        ET_SPECIALIZED_SHADER = 1u<<8,
        //! asset::ICPUMeshDataFormatDesc
        ET_MESH_DATA_DESCRIPTOR = 1u<<8,
        //! reserved, to implement later
        ET_GRAPHICS_PIPELINE = 1u<<9u,
        //! reserved, to implement later
        ET_SCENE = 1u<<10u,
        //! lights, etc.
        ET_IMPLEMENTATION_SPECIFIC_METADATA = 1u<<31u
        //! Reserved special value used for things like terminating lists of this enum
    };
    constexpr static size_t ET_STANDARD_TYPES_COUNT = 12u; //!< The variable shows valid amount of available Asset types in E_TYPE bit register

    static uint32_t typeFlagToIndex(E_TYPE _type)
    {
        uint32_t type = (uint32_t)_type;
        uint32_t r = 0u;
        while (type >>= 1u) ++r;
        return r;
    }

    IAsset() : isDummyObjectForCacheAliasing{false}, m_metadata{nullptr} {}

    virtual size_t conservativeSizeEstimate() const = 0;

    IAssetMetadata* getMetadata() { return m_metadata.get(); }
    const IAssetMetadata* getMetadata() const { return m_metadata.get(); }

    friend IAssetManager;

private:
    core::smart_refctd_ptr<IAssetMetadata> m_metadata;

    void setMetadata(core::smart_refctd_ptr<IAssetMetadata>&& _metadata) { m_metadata = std::move(_metadata); }

protected:
    bool isDummyObjectForCacheAliasing;
    //! To be implemented by base classes, dummies must retain references to other assets
    //! but cleans up all other resources which are not assets.
    virtual void convertToDummyObject() = 0;
    //! Checks if the object is either not dummy or dummy but in some cache for a purpose
    inline bool isInValidState() { return !isDummyObjectForCacheAliasing /* || !isCached TODO*/; }
    //! Pure virtual destructor to ensure no instantiation
    virtual ~IAsset() = 0;

public:
    //! To be implemented by derived classes
    virtual E_TYPE getAssetType() const = 0;
    //! 
    inline bool isADummyObjectForCache() const { return isDummyObjectForCacheAliasing; }
};

class SAssetBundle
{
	inline bool allSameTypeAndNotNull()
	{
		if (m_contents->size() == 0ull)
			return true;
		if (!*m_contents->begin())
			return false;
		IAsset::E_TYPE t = (*m_contents->begin())->getAssetType();
		for (auto it=m_contents->cbegin(); it!=m_contents->cend(); it++)
			if (!(*it) || (*it)->getAssetType()!=t)
				return false;
		return true;
	}
public:
    using contents_container_t = core::refctd_dynamic_array<core::smart_refctd_ptr<IAsset> >;
    
	SAssetBundle() : m_contents(contents_container_t::create_dynamic_array(0u), core::dont_grab) {}
	SAssetBundle(std::initializer_list<core::smart_refctd_ptr<IAsset> > _contents) : m_contents(contents_container_t::create_dynamic_array(_contents),core::dont_grab)
	{
		assert(allSameTypeAndNotNull());
	}
	template<typename container_t, typename iterator_t = typename container_t::iterator>
	SAssetBundle(const container_t& _contents) : m_contents(contents_container_t::create_dynamic_array(_contents), core::dont_grab)
	{
		assert(allSameTypeAndNotNull());
	}
	template<typename container_t, typename iterator_t = typename container_t::iterator>
    SAssetBundle(container_t&& _contents) : m_contents(contents_container_t::create_dynamic_array(std::move(_contents)), core::dont_grab)
    {
        assert(allSameTypeAndNotNull());
    }

	//! Returns an Asset type associated with the Asset
	/**
		An Asset type is specified in bit register 
		@see E_TYPE
	*/
    inline IAsset::E_TYPE getAssetType() const { return m_contents->front()->getAssetType(); }

	//! Getting beginning and end of an Asset stored by m_contents
    inline std::pair<const core::smart_refctd_ptr<IAsset>*, const core::smart_refctd_ptr<IAsset>*> getContents() const
    {
        return {m_contents->begin(), m_contents->end()};
    }

    //! Whether this asset bundle is in a cache and should be removed from cache to destroy
    inline bool isInAResourceCache() const { return m_isCached; }
    //! Only valid if isInAResourceCache() returns true
    std::string getCacheKey() const { return m_cacheKey; }

	//! Getting size of an Asset stored by m_contents
    size_t getSize() const { return m_contents->size(); }
	//! Checking if an Asset stored by m_contents is empty
    bool isEmpty() const { return getSize()==0ull; }

	//! Overloaded operator checking if both Assets\b are\b the same objects in memory
    bool operator==(const SAssetBundle& _other) const
    {
        return _other.m_contents == m_contents;
    }

	//! Overloaded operator checking if both Assets\b aren't\b the same objects in memory
    bool operator!=(const SAssetBundle& _other) const
    {
        return !((*this) != _other);
    }

private:
    friend class IAssetManager;

    inline void setNewCacheKey(const std::string& newKey) { m_cacheKey = newKey; }
    inline void setNewCacheKey(std::string&& newKey) { m_cacheKey = std::move(newKey); }
    inline void setCached(bool val) { m_isCached = val; }

    std::string m_cacheKey;
    bool m_isCached = false;
    core::smart_refctd_ptr<contents_container_t> m_contents;
};

}
}

#endif // __IRR_I_ASSET_H_INCLUDED__
