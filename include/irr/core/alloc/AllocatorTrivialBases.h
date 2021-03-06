// Copyright (C) 2018 Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW Engine"
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_ALLOCATOR_TRIVIAL_BASES_H_INCLUDED__
#define __IRR_ALLOCATOR_TRIVIAL_BASES_H_INCLUDED__


namespace irr
{
namespace core
{

template<typename T> class AllocatorTrivialBase;

template<>
class IRR_FORCE_EBO AllocatorTrivialBase<void>
{
    public:
        typedef void                                            value_type;
        typedef void*                                           pointer;
        typedef const void*                                     const_pointer;

        typedef void*                                           void_pointer;
        typedef const void*                                     const_void_pointer;
};

template<typename T>
class IRR_FORCE_EBO AllocatorTrivialBase
{
    public:
        typedef T                                               value_type;
        typedef T*                                              pointer;
        typedef const T*                                        const_pointer;
        typedef T&                                              reference;
        typedef const T&                                        const_reference;

        typedef void*                                           void_pointer;
        typedef const void*                                     const_void_pointer;
};


}
}

#endif // __IRR_ALLOCATOR_TRIVIAL_BASES_H_INCLUDED__
