#ifdef __IRR_AABB_SIMD_2D_INT_H_INCLUDED__
#define __IRR_AABB_SIMD_2D_INT_H_INCLUDED__

#include "irr/core/math/irrMath.h"
#include "irr/core/memory/memory.h"
#include "irr/core/alloc/AlignedBase.h"

#include "irr/core/math/aabb2dSIMDIntBase.h"

namespace irr { namespace core {

class aabb2dSIMDi// : public aabb2dSIMDIntBase<aabb2dSIMDi<core::vector4di32_SIMD>>
{
public:
	inline aabb2dSIMDi()
		: internalBoxRepresentation(-INT_MAX, -INT_MAX, -INT_MAX, -INT_MAX) {};

	inline explicit aabb2dSIMDi(const core::vector4di32_SIMD& _box)
		: internalBoxRepresentation(core::vector4di32_SIMD(-_box.x, -_box.y, _box.z, _box.w)) {};

	//need to figure out how to divide signed integers in SSE
	/*inline core::vector4di32_SIMD getCenter() const
	{
		core::vector4di32_SIMD a = (internalBoxRepresentation.zwzw() - internalBoxRepresentation);
		return a;
	}*/

	//! Determines if a point is within this box.
	/** Border is included (IS part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not */
	inline bool isPointInside(const core::vector4di32_SIMD& p) const
	{
		__m128i xmm0 = _mm_cmpgt_epi32(flipSignXY(p.xyxy()).getAsRegister(), internalBoxRepresentation.getAsRegister());
		core::vector4db_SIMD result(xmm0);
		result = !result;

		return result.allBits();
	}

	//! Determines if a point is within this box and not its borders.
	/** Border is excluded (NOT part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not. */
	bool isPointTotalInside(const core::vector4di32_SIMD& p) const
	{
		__m128i xmm0 = _mm_cmplt_epi32(flipSignXY(p.xyxy()).getAsRegister(), internalBoxRepresentation.getAsRegister());
		core::vector4db_SIMD result(xmm0);

		return result.allBits();
	}

	//! Check if this box is completely inside the 'other' box.
	/** \param other: Other box to check against.
	\return True if this box is completly inside the other box,
	otherwise false. */
	bool isFullInside(const aabb2dSIMDi& other) const
	{
		__m128i xmm0 = _mm_cmpgt_epi32(internalBoxRepresentation.getAsRegister(), other.internalBoxRepresentation.getAsRegister());
		core::vector4db_SIMD result(xmm0);
		result = !result;

		return result.allBits();
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */
	bool intersectsWithBox(const aabb2dSIMDi& other) const
	{
		core::vector4di32_SIMD tmp = flipSignXY(other.internalBoxRepresentation);

		return
			isPointInside(tmp) ||
			isPointInside(tmp.zwxx()) ||
			isPointInside(tmp.xwxx()) ||
			isPointInside(tmp.zyxx());
	}

private:

	//will change that most likely
	inline core::vector4di32_SIMD flipSignXY() const
	{ 
		return (internalBoxRepresentation ^ core::vector4du32_SIMD(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)) + core::vector4du32_SIMD(1, 1, 0, 0);
	}

	inline core::vector4di32_SIMD flipSignXY(const core::vector4di32_SIMD& vec) const
	{
		return (vec ^ core::vector4du32_SIMD(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)) + core::vector4du32_SIMD(1, 1, 0, 0);
	}


private:
	core::vector4di32_SIMD internalBoxRepresentation;
};

} 
}

#endif
