// ****************************************************************************
//  arithmetic.xs                                   XL - An extensible language
// ****************************************************************************
//
//   File Description:
//
//     Specification for arithmetic operations
//
//
//
//
//
//
//
//
// ****************************************************************************
//  (C) 2019 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
//   See LICENSE file for details.
// ****************************************************************************

interface XL.ARITHMETIC[type value] is
// ----------------------------------------------------------------------------
//   Specification for arithmetic operations
// ----------------------------------------------------------------------------
//   Arithmetic operations perform the most elementary operations.
//   Operations in this module are common to a variety of data types,
//   notably integer, unsigned, real, complex, vector and matrix.

    use UNSIGNED

    x:value + y:value           as value // Addition
    x:value - y:value           as value // Subtraction
    x:value * y:value           as value // Multiplication
    x:value / y:value           as value // Division
    x:value rem y:value         as value // Remainder
    x:value mod y:value         as value // Modulo
    -x:value                    as value // Negation
    abs  x:value                as value // Absolute value
    sign x:value                as value // Sign
    x:value ^ y:value           as value // Power
    x:value ^ y:unsigned        as value // Power

    ++in out x:value            as value // Pre-incrementation
    --in out x:value            as value // Pre-decrementation
    in out x:value++            as value // Post-incrementation
    in out x:value--            as value // Post-decrementation
