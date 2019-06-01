#ifdef __IRR_AABB_SIMD_2D_H_INCLUDED__
#define __IRR_AABB_SIMD_2D_H_INCLUDED__

#include "irr/core/math/irrMath.h"
#include "irr/core/memory/memory.h"
#include "irr/core/alloc/AlignedBase.h"

#include <iostream>

#define SIGN_FLIP_MASK_XY	core::vector3du32_SIMD(0x80000000, 0x80000000, 0x00000000, 0x00000000)

namespace irr { namespace core {

	//max, addPoint
class aabb2dSIMDf
{
public:
	inline aabb2dSIMDf()
		: internalBoxRepresentation(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX) {};

	inline explicit aabb2dSIMDf(const core::vectorSIMDf& other)
		: internalBoxRepresentation(other ^ SIGN_FLIP_MASK_XY) {};

	inline void addBox(const aabb2dSIMDf& other)
	{
		internalBoxRepresentation = core::max_(internalBoxRepresentation, other.internalBoxRepresentation);
	}

	inline void addPoint(const core::vector2df_SIMD& point)
	{
		addBox(aabb2dSIMDf(point));
	}

	inline core::vectorSIMDf getMinEdge() const
	{
		core::vectorSIMDf minEdge = internalBoxRepresentation ^ SIGN_FLIP_MASK_XY;
		minEdge.makeSafe2D();

		return minEdge;
	}

	inline core::vectorSIMDf getMaxEdge() const
	{
		core::vectorSIMDf maxEdge = internalBoxRepresentation.zwzw();
		maxEdge.makeSafe2D();

		return maxEdge;
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
	inline bool isFullInside(const aabb2dSIMDf& other) const
	{
		core::vector4db_SIMD a = internalBoxRepresentation <= other.internalBoxRepresentation;
		return a.allBits();
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */
	inline bool intersectsWithBox(const aabb2dSIMDf& other) const
	{
		//will privide better algorithm 

		core::vectorSIMDf tmp = other.internalBoxRepresentation ^ SIGN_FLIP_MASK_XY;

		return
			isPointInside(tmp) ||
			isPointInside(tmp.zwxx()) ||
			isPointInside(tmp.xwxx()) ||
			isPointInside(tmp.zyxx());
	}

private:
	core::vectorSIMDf internalBoxRepresentation;
};


} 
}

#undef SIGN_FLIP_MASK_XY

#endif
