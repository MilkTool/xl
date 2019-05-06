#ifndef NATIVE_H
#define NATIVE_H
// ****************************************************************************
//  native.h                                                      Tao3D project
// ****************************************************************************
//
//   File Description:
//
//     Native interface between XL and native code
//
//     The new interface requires C++11 features, notably variadic templates
//
//
//
//
//
//
// ****************************************************************************
//   (C) 2019 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
// ****************************************************************************
//   This file is part of Tao3D.
//
//   Tao3D is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   Foobar is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with Tao3D.  If not, see <https://www.gnu.org/licenses/>.
// ****************************************************************************

#include "basics.h"
#include "compiler.h"
#include "llvm-crap.h"
#include "tree.h"

#include <recorder/recorder.h>
#include <type_traits>

RECORDER_DECLARE(native);

namespace XL
{
// ============================================================================
//
//    xl_type: Convert a native type into its XL counterpart
//
// ============================================================================

template<typename T, typename Enable = void>
struct xl_type
// ----------------------------------------------------------------------------
//   Provide a common interface to convert an XL type into its
// ----------------------------------------------------------------------------
{
    typedef Tree *       tree_type;
    typedef T            native_type;
    static PointerType_p TreeType(Compiler &c)   { return c.treePtrTy; }
    static PointerType_p NativeType(Compiler &c) { return c.treePtrTy; }
    static Tree *        Shape()                 { return XL::tree_type; }
};


template <typename Num>
struct xl_type<Num,
               typename std::enable_if<std::is_integral<Num>::value>::type>
// ----------------------------------------------------------------------------
//   Specialization for integer types
// ----------------------------------------------------------------------------
{
    typedef Integer *    tree_type;
    typedef Num          native_type;

    static PointerType_p TreeType(Compiler &c)
    {
        return c.integerTreePtrTy;
    }

    static Type_p NativeType(Compiler &c)
    {
        return c.jit.IntegerType<Num>();
    }
    static Tree *Shape()
    {
        return integer_type;
    }
};


template <> inline Tree *xl_type<bool>  ::Shape() { return boolean_type; }
template <> inline Tree *xl_type<int8>  ::Shape() { return integer8_type; }
template <> inline Tree *xl_type<int16> ::Shape() { return integer16_type; }
template <> inline Tree *xl_type<int32> ::Shape() { return integer32_type; }
template <> inline Tree *xl_type<int64> ::Shape() { return integer64_type; }
template <> inline Tree *xl_type<uint8> ::Shape() { return unsigned8_type; }
template <> inline Tree *xl_type<uint16>::Shape() { return unsigned16_type; }
template <> inline Tree *xl_type<uint32>::Shape() { return unsigned32_type; }
template <> inline Tree *xl_type<uint64>::Shape() { return unsigned64_type; }


template <typename Num>
struct xl_type<Num,
          typename std::enable_if<std::is_floating_point<Num>::value>::type>
// ----------------------------------------------------------------------------
//   Specialization for floating-point types
// ----------------------------------------------------------------------------
{
    typedef Real *       tree_type;
    typedef Num          native_type;

    static PointerType_p TreeType(Compiler &c)
    {
        return c.realTreePtrTy;
    }

    static Type_p NativeType(Compiler &c)
    {
        return c.jit.FloatType(c.jit.BitsPerByte * sizeof(Num));
    }

    static Tree *Shape()
    {
        return real_type;
    }
};


template <> inline Tree *xl_type<float>  ::Shape()  { return real32_type; }
template <> inline Tree *xl_type<double> ::Shape()  { return real64_type; }

template <>
struct xl_type<kstring>
// ----------------------------------------------------------------------------
//   Specialization for C-style C strings
// ----------------------------------------------------------------------------
{
    typedef Text *       tree_type;
    typedef kstring      native_type;

    static PointerType_p TreeType(Compiler &c)
    {
        return c.textTreePtrTy;
    }

    static Type_p NativeType(Compiler &c)
    {
        return c.charPtrTy;
    }

    static Tree *Shape()
    {
        return text_type;
    }
};


template <>
struct xl_type<text>
// ----------------------------------------------------------------------------
//   Specialization for C++-style C strings
// ----------------------------------------------------------------------------
{
    typedef Text *       tree_type;
    typedef text         native_type;

    static PointerType_p TreeType(Compiler &c)
    {
        return c.textTreePtrTy;
    }

    static Type_p NativeType(Compiler &c)
    {
        return c.textPtrTy;
    }

    static Tree *Shape()
    {
        return text_type;
    }
};


template <>
struct xl_type<Scope *>
// ----------------------------------------------------------------------------
//   Specialization for C++-style C strings
// ----------------------------------------------------------------------------
{
    typedef Tree *       tree_type;
    typedef Scope *      native_type;

    static PointerType_p TreeType(Compiler &c)
    {
        return c.scopePtrTy;
    }

    static Type_p NativeType(Compiler &c)
    {
        return c.scopePtrTy;
    }

    static Tree *Shape()
    {
        static Name scope("scope", Tree::BUILTIN);
        return &scope;
    }
};



// ============================================================================
//
//    Extracting information about a function type
//
// ============================================================================

template <typename F> struct function_type;

template <typename R>
struct function_type<R(*)()>
// ----------------------------------------------------------------------------
//   Special case for functions without arguments
// ----------------------------------------------------------------------------
{
    typedef R return_type;
    static void Args(Compiler &, Signature &)  {}
    static Tree *Shape()        { return nullptr; }
    static Tree *ReturnShape()  { return xl_type<R>::Shape(); }
};


template <typename R, typename T>
struct function_type<R(*)(T)>
// ----------------------------------------------------------------------------
//   Special case for a function with a single argument
// ----------------------------------------------------------------------------
{
    typedef R return_type;
    static void Args(Compiler &compiler, Signature &signature)
    {
        Type_p argTy = xl_type<T>::NativeType(compiler);
        signature.push_back(argTy);
    }
    static Tree *Shape(uint &index)
    {
        Tree *type = xl_type<T>::Shape();
        Name *name = new Name(text(1, 'A' + index), Tree::BUILTIN);
        ++index;
        if (type)
        {
            Infix *infix = new Infix(":", name, type);
            return infix;
        }
        return name;
    }
    static Tree *ReturnShape()  { return xl_type<R>::Shape(); }
};


template <typename R, typename T, typename ... A>
struct function_type<R(*)(T,A...)>
// ----------------------------------------------------------------------------
//   Recursive for the rest
// ----------------------------------------------------------------------------
{
    typedef R return_type;
    static void Args(Compiler &compiler, Signature &signature)
    {
        function_type<R(*)(T)>::Args(compiler, signature);
        function_type<R(*)(A...)>::Args(compiler, signature);
    }
    static Tree *Shape(uint &index)
    {
        Tree *left = function_type<R(*)(T)>::Shape(index);
        Tree *right = function_type<R(*)(A...)>::Shape(index);
        Infix *infix = new Infix(",", left, right);
        return infix;
    }
    static Tree *ReturnShape()  { return xl_type<R>::Shape(); }
};



// ============================================================================
//
//    JIT interface for function types
//
// ============================================================================

struct NativeInterface
// ----------------------------------------------------------------------------
//   Base functions to generate JIT code for a native function
// ----------------------------------------------------------------------------
{
    virtual     ~NativeInterface() {}
    virtual     Type_p          ReturnType(Compiler &)                  = 0;
    virtual     FunctionType_p  FunctionType(Compiler &)                = 0;
    virtual     Function_p      Prototype(Compiler &, text name)        = 0;
    virtual     Tree *          Shape(uint &index)                      = 0;
};


template<typename fntype>
struct NativeImplementation : NativeInterface
// ----------------------------------------------------------------------------
//   Generate the interface
// ----------------------------------------------------------------------------
{
    virtual Type_p ReturnType(Compiler &compiler) override
    {
        typedef typename function_type<fntype>::return_type return_type;
        xl_type<return_type> xlt;
        return xlt.NativeType(compiler);
    }

    virtual FunctionType_p FunctionType(Compiler &compiler) override
    {
        Type_p rty = ReturnType(compiler);

        function_type<fntype> ft;
        Signature sig;
        ft.Args(compiler, sig);
        return compiler.jit.FunctionType(rty, sig);
    }

    virtual Function_p Prototype(Compiler &compiler, text name) override
    {
        FunctionType_p fty = FunctionType(compiler);
        Function_p f = compiler.jit.ExternFunction(fty, name);
        return f;
    }

    virtual Tree *Shape(uint &index) override
    {
        function_type<fntype> ft;
        Tree *result = ft.Shape(index);
        Tree *retType = ft.ReturnShape();
        if (retType)
            result = new Infix("as", result, retType);
        return result;
    }
};



// ============================================================================
//
//    Native interface builder
//
// ============================================================================

struct Native
// ----------------------------------------------------------------------------
//   Native interface for a function with the given signature
// ----------------------------------------------------------------------------
{
    template<typename fntype>
    Native(kstring symbol, fntype input)
        : symbol(symbol),
          address((void *) input),
          implementation(new NativeImplementation<fntype>),
          shape(nullptr),
          next(list)
    {
        list = this;
    }

    ~Native();

    static Native *     First()                 { return list; }
    Native *            Next()                  { return next; }

    Type_p              ReturnType(Compiler &compiler);
    FunctionType_p      FunctionType(Compiler &compiler);
    Function_p          Prototype(Compiler &compiler, text name);

    Tree *              Shape();

    static void         EnterPrototypes(Compiler &compiler);

public:
    kstring             symbol;
    void *              address;
    NativeInterface *   implementation;
    Tree_p              shape;

private:
    static Native      *list;
    Native             *next;
};


inline Type_p Native::ReturnType(Compiler &compiler)
// ----------------------------------------------------------------------------
//   Delegate the return type computation to the implementation
// ----------------------------------------------------------------------------
{
    return implementation->ReturnType(compiler);
}


inline FunctionType_p Native::FunctionType(Compiler &compiler)
// ----------------------------------------------------------------------------
//   Delegate the function type computation to the implementation
// ----------------------------------------------------------------------------
{
    return implementation->FunctionType(compiler);
}


inline Function_p Native::Prototype(Compiler &compiler, text name)
// ----------------------------------------------------------------------------
//   Delegate the prototype generation to the implementation
// ----------------------------------------------------------------------------
{
    return implementation->Prototype(compiler, name);
}


inline Tree *Native::Shape()
// ----------------------------------------------------------------------------
//   Delegate the shape generation to the implementation
// ----------------------------------------------------------------------------
{
    if (!shape)
    {
        uint index = 0;
        Name *name = new Name(symbol, Tree::BUILTIN);
        if (Tree *args = implementation->Shape(index))
        {
            Prefix *prefix = new Prefix(name, args, Tree::BUILTIN);
            shape = prefix;
        }
        else
        {
            shape = name;
        }
    }
    return shape;
}

} // namespace XL

#define NATIVE(Name)    static Native xl_##Name##_native(#Name, Name)
#define XL_NATIVE(Name) static Native xl_##Name##_native(#Name, xl_##Name)

#endif // NATIVE_H
