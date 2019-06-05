#ifndef __IRR_AABBOX_2D_SIMD_H_INCLUDED__
#define __IRR_AABBOX_2D_SIMD_H_INCLUDED__

#include "irr/core/math/irrMath.h"

namespace irr { namespace core {

class aabbox2dSIMDf
{
public:
	inline aabbox2dSIMDf()
		: bounds(-1.0f, -1.0f, 1.0f, 1.0f) {};

	inline aabbox2dSIMDf(const core::vectorSIMDf& _bounds)
		: bounds(_bounds) {};

	inline aabbox2dSIMDf(float minx, float miny, float maxx, float maxy)
		: bounds(core::vectorSIMDf(minx, miny, maxx, maxy)) {};

private:
	core::vectorSIMDf bounds; // xy - min, zw - max

};

}
}

#endif