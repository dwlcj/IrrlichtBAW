#ifndef __IRR_AABB_SIMD_2D_H_INCLUDED__
#define __IRR_AABB_SIMD_2D_H_INCLUDED__

#include "irr/core/math/irrMath.h"
#include "irr/core/memory/memory.h"
#include "irr/core/alloc/AlignedBase.h"

#include <iostream>

#define SIGN_FLIP_MASK_XY	core::vector3du32_SIMD(0x80000000, 0x80000000, 0x00000000, 0x00000000)
#define SIGN_FLIP_MASK_XYZW core::vector3du32_SIMD(0x80000000, 0x80000000, 0x80000000, 0x80000000)


namespace irr { namespace core {

class aabb2dSIMDf
{
public:
	inline aabb2dSIMDf()
		: box( FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX ) {};

	inline aabb2dSIMDf(const core::vectorSIMDf& _box)
		: box(_box ^ SIGN_FLIP_MASK_XY) {};

	inline void addBox(const aabb2dSIMDf& _box)		
	{ 
		box = core::max_(box, _box.getBounds()); 
	}

	inline void addPoint(const core::vectorSIMDf& point)	
	{
		addBox(point); 
	}

	inline core::vectorSIMDf getBounds() const
	{
		return box ^ SIGN_FLIP_MASK_XY;
	}


	//! Resets the bounding box to a one-point box.
	/** \param x X coord of the point.
	\param y Y coord of the point. */
	inline void reset(float x, float y)
	{
		box = core::vectorSIMDf(-x, -y, x, y);
	}

	//! Resets the bounding box.
	/** \param initValue New box to set this one to. */
	inline void reset(const aabb2dSIMDf& initValue)
	{
		box = initValue.box;
	}

	//! Resets the bounding box to a one-point box.
	/** \param initValue New point. */
	//void reset(const aabb2dSIMDf& initValue);

	//! Get center of the bounding box
	/** \return Center of the bounding box. */
	inline core::vectorSIMDf getCenter() const
	{
		core::vectorSIMDf minToCenterVec = (box + box.zwzw()) * 0.5f;
		return ((box ^ SIGN_FLIP_MASK_XY) + minToCenterVec);
	}

	//! Get extent of the box (maximal distance of two points in the box)
	/** \return Extent of the bounding box. */
	core::vectorSIMDf getExtent() const
	{
		return box.zwzw() + box;
	}

	//! Check if the box is empty.
	/** This means that there is no space between the min and max edge.
	\return True if box is empty, else false. */
	bool isEmpty() const
	{
		core::vectorSIMDf extent = getExtent();
			//epsilon?
		return (extent.x == 0.0f) || (extent.y == 0.0f);
	}

	//! Get the surface area of the box in squared units
	inline float getArea() const
	{
		core::vectorSIMDf a = box + box.zwxx();	// ( abs(x2-x1), abs(y2-y1) )
		return a.x * a.y;						// multiplication of both sides of the square
	}

	//! Stores all 8 edges of the box into an array
	/** \param edges: Pointer to array of 8 edges. */
	void getEdges(core::vectorSIMDf& edges) const;

	//! Repairs the box.
	/** Necessary if for example MinEdge and MaxEdge are swapped. */
	//TODO
	void repair();

	//! Determines if a point is within this box.
	/** Border is included (IS part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not */
	inline bool isPointInside(const core::vectorSIMDf& p) const
	{
		core::vector4db_SIMD result = (p.xyxy() ^ SIGN_FLIP_MASK_XY) <= box;
		return result.allBits();
	}

	//! Determines if a point is within this box and not its borders.
	/** Border is excluded (NOT part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not. */
	bool isPointTotalInside(const core::vectorSIMDf& p) const
	{
		core::vector4db_SIMD result = (p.xyxy() ^ SIGN_FLIP_MASK_XY) < box;
		return result.allBits();
	}

	//! Check if this box is completely inside the 'other' box.
	/** \param other: Other box to check against.
	\return True if this box is completly inside the other box,
	otherwise false. */
	bool isFullInside(const aabb2dSIMDf& other) const
	{
		core::vector4db_SIMD a = box <= other.box;
		core::vector4db_SIMD b = (box.zwxy() ^ SIGN_FLIP_MASK_XYZW) <= other.box;

		return a.allBits() && b.allBits();
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */

	bool intersectsWithBox(const aabb2dSIMDf& other) const
	{
		return
			isPointInside(other.box) ||
			isPointInside(other.box.zwxx()) ||
			isPointInside(other.box.xwxx()) ||
			isPointInside(other.box.zyxx());
	}

	//! Tests if the box intersects with a line
	/** \param line: Line to test intersection with.
	\return True if there is an intersection , else false. */
	bool intersectsWithLine(const line3d<float>& line) const;

	//! Tests if the box intersects with a line
	/** \param linemiddle Center of the line.
	\param linevect Vector of the line.
	\param halflength Half length of the line.
	\return True if there is an intersection, else false. */
	bool intersectsWithLine(const core::vectorSIMDf& linemiddle,
		const core::vectorSIMDf& linevect, float/*T*/ halflength) const;



private:
	core::vectorSIMDf box;
};

} 
}

#endif
