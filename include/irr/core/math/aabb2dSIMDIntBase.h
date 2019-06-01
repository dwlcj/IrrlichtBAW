#ifndef __IRR_AABB_SIMD_INT_BASE_H_INCLUDED__
#define __IRR_AABB_SIMD_INT_BASE_H_INCLUDED__

#include "irr/core/math/irrMath.h"
#include "irr/core/memory/memory.h"
#include "irr/core/alloc/AlignedBase.h"
#include "C:\IrrlichtBAW\IrrlichtBAW\include\vectorSIMD.h"

#include <iostream>

namespace irr { namespace core {

//will modify this class so aabbSIMDf will be able do derive from it too
template<template<typename> typename CRTP, typename VECTOR_TYPE>
class aabbSIMDBase
{
	static_assert(	
		std::is_same<VECTOR_TYPE, core::vector4di32_SIMD>::value ||
		std::is_same<VECTOR_TYPE, core::vector4du32_SIMD>::value, 
		"assertion failed: cannot use this type");

public:
	inline void addPoint(const VECTOR_TYPE& other)
	{
		static_cast<CRTP<VECTOR_TYPE>*>(this)->addBox();
	}

	inline VECTOR_TYPE getExtent() const
	{
		return getMaxEdge() - getMinEdge();
	}

	inline VECTOR_TYPE getMinEdge() const
	{
		VECTOR_TYPE minEdge = box;
		minEdge.makeSafe2D();
		return minEdge;
	}

	inline VECTOR_TYPE getMaxEdge() const
	{
		VECTOR_TYPE maxEdge = box.zwzw();
		maxEdge.makeSafe2D();
		return maxEdge;
	}

	inline CRTP<VECTOR_TYPE> reset(const VECTOR_TYPE& point) const
	{
		box = point.xyxy();
		return *this;
	}

	//! Repairs the box.
	/** Necessary if for example MinEdge and MaxEdge are swapped. */
	inline CRTP<VECTOR_TYPE> repair()
	{
		reset(getMinEdge());
		addPoint(getMaxEdge());

		return *this;
	}

	//! Get the surface area of the box in squared units
	inline float getArea() const
	{
		core::vector4di32_SIMD a = box + box.zwzw();
		return a.x* a.y;
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */
	bool intersectsWithBox(const CRTP<VECTOR_TYPE>& other) const
	{
		return
			static_cast<const CRTP<VECTOR_TYPE>*>(this)->isPointInside(other.box) ||
			static_cast<const CRTP<VECTOR_TYPE>*>(this)->isPointInside(other.box.zwxx()) ||
			static_cast<const CRTP<VECTOR_TYPE>*>(this)->isPointInside(other.box.xwxx()) ||
			static_cast<const CRTP<VECTOR_TYPE>*>(this)->isPointInside(other.box.zyxx());
	}

protected:
	VECTOR_TYPE box;
};

template<typename VECTOR_TYPE>
class aabbSIMD : public aabbSIMDBase<aabbSIMD, VECTOR_TYPE> 
{};


template<>
class aabbSIMD<core::vector4di32_SIMD> : public aabbSIMDBase<aabbSIMD,core::vector4di32_SIMD>
{
public:
	aabbSIMD(const core::vector4di32_SIMD& _box)
		//:box(_box) - doesn't compile
	{
		this->box = _box;
	}

	bool isPointInside(const core::vector2di32_SIMD& point) const
	{
		//temporary solution
		return	
			point.x <= getMaxEdge().x &&
			point.y <= getMaxEdge().y &&
			point.x >= getMinEdge().x &&
			point.x >= getMinEdge().y;
	}

};

typedef aabbSIMD<core::vector4di32_SIMD> aabbSIMDi;

}
}

#endif
