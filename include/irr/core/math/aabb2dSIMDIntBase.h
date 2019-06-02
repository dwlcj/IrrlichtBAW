#ifndef __IRR_AABB_SIMD_INT_BASE_H_INCLUDED__
#define __IRR_AABB_SIMD_INT_BASE_H_INCLUDED__

#include "irr/core/math/irrMath.h"
#include "irr/core/memory/memory.h"
#include "irr/core/alloc/AlignedBase.h"
#include "../include/vectorSIMD.h"

#include <iostream>

#define DWN_CAST_THIS_PTR static_cast<const CRTP<VECTOR_TYPE>*>(this)
#define SIGN_FLIP_MASK_XY	core::vector3du32_SIMD(0x80000000, 0x80000000, 0x00000000, 0x00000000)

namespace irr { namespace core {


template<template<typename> typename CRTP, typename VECTOR_TYPE>
class aabbSIMDBase : public AlignedBase<_IRR_VECTOR_ALIGNMENT>
{
	static_assert(
		std::is_same<VECTOR_TYPE, core::vector2di32_SIMD>::value ||
		std::is_same<VECTOR_TYPE, core::vector2du32_SIMD>::value ||
		std::is_same<VECTOR_TYPE, core::vector2df_SIMD>::value,
		"assertion failed: cannot use this type");

public:
	inline void addPoint(const VECTOR_TYPE& other)
	{
		DWN_CAST_THIS_PTR->addBox();
	}

	inline VECTOR_TYPE getExtent() const
	{
		return DWN_CAST_THIS_PTR->getMaxEdge() - static_cast<const CRTP<VECTOR_TYPE>*>(this)->getMinEdge();
	}

	//need operator/ for core::vector32_i<uint32_t> and core::vector32_i<uint32_t>
	/*inline VECTOR_TYPE getCenter() const
	{
		return static_cast<const CRTP<VECTOR_TYPE>*>(this)->getMaxEdge() - static_cast<const CRTP<VECTOR_TYPE>*>(this)->getMinEdge();
	}*/

	//! Resets the bounding box to a one-point box.
	/** \param x X coord of the point.
	\param y Y coord of the point. */
	inline void reset(const VECTOR_TYPE& point)
	{
		CRTP<VECTOR_TYPE> pointBox(point.xyxy());
		internalBoxRepresentation= pointBox;
	}

	//! Repairs the box.
	/** Necessary if for example MinEdge and MaxEdge are swapped. */
	inline CRTP<VECTOR_TYPE> repair()
	{
		//
		//but will it work for aabbSIMDf for sure?
		//
		//
		reset(DWN_CAST_THIS_PTR->getMinEdge());
		addPoint(DWN_CAST_THIS_PTR->getMaxEdge());

		return *this;
	}

	//! Check if the box is empty.
	/** This means that there is no space between the min and max edge.
	\return True if box is empty, else false. */
	inline bool isEmpty() const
	{
		return return (getMinEdge() == getMaxEdge()).all();;
	}

	//! Get the surface area of the box in squared units
	inline float getArea() const
	{
		VECTOR_TYPE a = DWN_CAST_THIS_PTR->internalBoxRepresentation.getMaxEdge().x - DWN_CAST_THIS_PTR->internalBoxRepresentation.getMinEdge().x;
		VECTOR_TYPE b = DWN_CAST_THIS_PTR->internalBoxRepresentation.getMaxEdge().y - DWN_CAST_THIS_PTR->internalBoxRepresentation.getMinEdge().y;
		return a * b;
	}

	//! Get center of the bounding box
	/** \return Center of the bounding box. */
	inline core::vectorSIMDf getCenter() const
	{
		return (internalBoxRepresentation.zwzw() - internalBoxRepresentation) / VECTOR_TYPE(2);
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */
	bool intersectsWithBox(const CRTP<VECTOR_TYPE>& other) const
	{
		return
			DWN_CAST_THIS_PTR->isPointInside(other.internalBoxRepresentation) ||
			DWN_CAST_THIS_PTR->isPointInside(other.internalBoxRepresentation.zwxx()) ||
			DWN_CAST_THIS_PTR->isPointInside(other.internalBoxRepresentation.xwxx()) ||
			DWN_CAST_THIS_PTR->isPointInside(other.internalBoxRepresentation.zyxx());
	}

protected:
	VECTOR_TYPE internalBoxRepresentation;
};

template<typename VECTOR_TYPE>
class aabbSIMD;

template<>
class aabbSIMD<core::vector2df_SIMD> : public aabbSIMDBase<aabbSIMD, core::vector2df_SIMD>
{
public:
	inline aabbSIMD()
		//: internalBoxRepresentation(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX)
	{
		internalBoxRepresentation = core::vectorSIMDf(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
	};

	aabbSIMD(const core::vector2df_SIMD& _box)
		//:inernalboxRepresentation(_box) - doesn't compile
	{
		this->internalBoxRepresentation = _box ^ SIGN_FLIP_MASK_XY;
	}

	inline void addBox(const aabbSIMD<core::vector2df_SIMD>& other)
	{
		internalBoxRepresentation = core::max_(internalBoxRepresentation, other.internalBoxRepresentation);
	}

	inline core::vector2df_SIMD getMinEdge() const
	{
		core::vector2df_SIMD minEdge = internalBoxRepresentation ^ SIGN_FLIP_MASK_XY;
		minEdge.makeSafe2D();

		return minEdge;
	}

	inline core::vector2df_SIMD getMaxEdge() const
	{
		core::vector2df_SIMD maxEdge = internalBoxRepresentation.zwzw();
		maxEdge.makeSafe2D();

		return maxEdge;
	}

	//! Determines if a point is within this box.
	/** Border is included (IS part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not */
	inline bool isPointInside(const core::vector2df_SIMD& p) const
	{
		core::vector4db_SIMD result = (p.xyxy() ^ SIGN_FLIP_MASK_XY) <= internalBoxRepresentation;
		return result.allBits();
	}

	//! Determines if a point is within this box and not its borders.
	/** Border is excluded (NOT part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not. */
	inline bool isPointTotalInside(const core::vector2df_SIMD& p) const
	{
		core::vector4db_SIMD result = (p.xyxy() ^ SIGN_FLIP_MASK_XY) < internalBoxRepresentation;
		return result.allBits();
	}

	//! Check if this box is completely inside the 'other' box.
	/** \param other: Other box to check against.
	\return True if this box is completly inside the other box,
	otherwise false. */
	inline bool isFullInside(const aabbSIMD<core::vector2df_SIMD>& other) const
	{
		core::vector4db_SIMD a = internalBoxRepresentation<= other.internalBoxRepresentation;
		return a.allBits();
	}

};

template<typename VECTOR_TYPE>
class aabb2dSIMDInt : public aabbSIMDBase<aabb2dSIMDInt, VECTOR_TYPE>
{
	static_assert(
		std::is_same<VECTOR_TYPE, core::vector2di32_SIMD>::value ||
		std::is_same<VECTOR_TYPE, core::vector2du32_SIMD>::value,
		"assertion failed: cannot use this type");

public:
	inline aabb2dSIMDInt()
	{
		if constexpr (std::is_same<VECTOR_TYPE, core::vector2di32_SIMD>::value)
			internalBoxRepresentation = core::vector4di32_SIMD(INT_MIN);

		if constexpr (std::is_same<VECTOR_TYPE, core::vector2du32_SIMD>::value)
			internalBoxRepresentation = core::vector4du32_SIMD(0u);
	}

	inline aabb2dSIMDInt(const VECTOR_TYPE& _box)
		//:inernalboxRepresentation(_box) - doesn't compile
	{
		this->internalBoxRepresentation = _box;
	}

	inline void addBox(const VECTOR_TYPE& other)
	{
		//TODO
	}

	inline core::vector2di32_SIMD getMinEdge() const
	{
		core::vector2di32_SIMD minEdge = internalBoxRepresentation;
		minEdge.makeSafe2D();

		return minEdge;
	}

	inline core::vector2di32_SIMD getMaxEdge() const
	{
		core::vector2di32_SIMD maxEdge = internalBoxRepresentation.zwzw();
		maxEdge.makeSafe2D();

		return maxEdge;
	} 

	inline bool isPointInside(const VECTOR_TYPE& p) const
	{
		//need operator< and operator>
		//but implementation will be the same for both core::vector2di_SIMD and core::vector2du_SIMD

		return true;
	}

	inline bool isPointTotalInside(const VECTOR_TYPE& p) const
	{
		//need operator< and operator>
		//but implementation will be the same for both core::vector2di_SIMD and core::vector2du_SIMD

		return true;
	}

	inline bool isFullInside(const aabb2dSIMDInt& other) const
	{
		//need operator< and operator>
		//but implementation will be the same for both core::vector2di_SIMD and core::vector2du_SIMD

		return true;
	}

};

typedef aabbSIMD<core::vector2df_SIMD> aabb2dSIMDf;
typedef aabb2dSIMDInt<core::vector2di32_SIMD> aabb2dSIMDi;
typedef aabb2dSIMDInt<core::vector2du32_SIMD> aabb2dSIMDu;

}
} 

#undef DWN_CAST_THIS_PTR
#undef SIGN_FLIP_MASK_XY

#endif
