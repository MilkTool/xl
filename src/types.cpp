// ****************************************************************************
//  types.cpp                                                     XL project
// ****************************************************************************
//
//   File Description:
//
//     The type system in XL
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License, with the
// following clarification and exception.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library. Thus, the terms and conditions of the
// GNU General Public License cover the whole combination.
//
// As a special exception, the copyright holders of this library give you
// permission to link this library with independent modules to produce an
// executable, regardless of the license terms of these independent modules,
// and to copy and distribute the resulting executable under terms of your
// choice, provided that you also meet, for each linked independent module,
// the terms and conditions of the license of that module. An independent
// module is a module which is not derived from or based on this library.
// If you modify this library, you may extend this exception to your version
// of the library, but you are not obliged to do so. If you do not wish to
// do so, delete this exception statement from your version.
//
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "types.h"
#include "tree.h"
#include "runtime.h"
#include "errors.h"
#include "options.h"
#include "save.h"
#include "compiler-args.h"
#include "cdecls.h"
#include "renderer.h"
#include "basics.h"

#include <iostream>

XL_BEGIN

// ============================================================================
//
//    Type allocation and unification algorithms (hacked Damas-Hilney-Milner)
//
// ============================================================================

uint Types::id= 0;

Types::Types(Scope *scope)
// ----------------------------------------------------------------------------
//   Constructor for top-level type inferences
// ----------------------------------------------------------------------------
    : context(new Context(scope)),
      types(),
      unifications(),
      rcalls(),
      declaration(false)
{
    record(types, "Created Types %p for scope %t", this, scope);
}


Types::Types(Scope *scope, Types *parent)
// ----------------------------------------------------------------------------
//   Constructor for "child" type inferences, i.e. done within a parent
// ----------------------------------------------------------------------------
    : context(new Context(scope)),
      types(parent->types),
      unifications(parent->unifications),
      rcalls(parent->rcalls),
      declaration(false)
{
    context->CreateScope();
    scope = context->CurrentScope();
    record(types, "Created child Types %p scope %t", this, scope);
}


Types::~Types()
// ----------------------------------------------------------------------------
//    Destructor - Nothing to explicitly delete, but useful for debugging
// ----------------------------------------------------------------------------
{
    record(types, "Deleted Types %p", this);
}


Tree *Types::TypeAnalysis(Tree *program)
// ----------------------------------------------------------------------------
//   Perform all the steps of type inference on the given program
// ----------------------------------------------------------------------------
{
    // Once this is done, record all type information for the program
    record(types, "Type analysis for %t in %p", program, this);
    Tree *result = Type(program);
    record(types, "Type for %t in %p is %t", program, this, result);

    // Dump debug information if approriate
    if (RECORDER_TRACE(types_ids))
    {
        std::cout << "TYPES:\n";
        debugt(this);
    }
    if (RECORDER_TRACE(types_unifications))
    {
        std::cout << "UNIFICATIONS:\n";
        debugu(this);
    }
    if (RECORDER_TRACE(types_calls))
    {
        std::cout << "CALLS:\n";
        debugr(this);
    }

    return result;
}


Tree *Types::BaseType(Tree *type)
// ----------------------------------------------------------------------------
//   Return the base type associated to the input type
// ----------------------------------------------------------------------------
{
    // Check if we have a type as input and if there is a base type for it
    auto it = unifications.find(type);
    if (it != unifications.end())
        return (*it).second;
    return type;
}


Tree *Types::Type(Tree *expr)
// ----------------------------------------------------------------------------
//   Return the type associated with a given expression
// ----------------------------------------------------------------------------
{
    // Check type, make sure we don't destroy a nullptr type
    Tree *type = nullptr;
    auto it = types.find(expr);
    if (it == types.end())
    {
        type = expr->Do(this);
        type = AssignType(expr, type);
        record(types_ids, "Created type for %t in %p is %t", expr, this, type);
    }
    else
    {
        type = (*it).second;
        record(types_ids, "Existing type for %t in %p is %t", expr, this, type);
    }
    return type;
}


Tree *Types::DeclarationType(Tree *expr)
// ----------------------------------------------------------------------------
//   Return the type for a declaration, e.g. for [X] in [X is 0]
// ----------------------------------------------------------------------------
{
    Save<bool> save(declaration, true);
    Tree *result = Type(expr);
    return result;
}


Tree *Types::NewType(Tree *expr)
// ----------------------------------------------------------------------------
//   Create a new generic type like #T for the given expr
// ----------------------------------------------------------------------------
{
    // Protect against case where type would already exist
    auto it = types.find(expr);
    if (it != types.end())
        return (*it).second;

    Tree *type = NewTypeName(expr->Position());
    types[expr] = type;
    return type;
}


Tree *Types::ValueType(Tree *expr)
// ----------------------------------------------------------------------------
//   Return the value type associated with 'expr', or an error
// ----------------------------------------------------------------------------
{
    auto it = types.find(expr);
    if (it != types.end())
        return (*it).second;
    Ooops("Internal error, no type found for $1, trying deduction", expr);
    Tree *type = Type(expr);
    return type;
}


rcall_map & Types::TypesRewriteCalls()
// ----------------------------------------------------------------------------
//   Returns the list of rewrite calls for this
// ----------------------------------------------------------------------------
{
    record(types_calls, "Number of rewrites for %p is %u", this, rcalls.size());
    return rcalls;
}


Context *Types::TypesContext()
// ----------------------------------------------------------------------------
//   Returns the context where we evaluated the types
// ----------------------------------------------------------------------------
{
    return context;
}


Scope *Types::TypesScope()
// ----------------------------------------------------------------------------
//   Returns the scope where we evaluated the types
// ----------------------------------------------------------------------------
{
    return context->CurrentScope();
}


bool Types::HasCaptures(Tree *form, TreeList &captured)
// ----------------------------------------------------------------------------
//   Check if a given form has captured things from the environment
// ----------------------------------------------------------------------------
{
    auto it = rcalls.find(form);
    if (it == rcalls.end())
        return false;

    RewriteCalls_p calls = (*it).second;
    Scope *scope = context->CurrentScope();
    for (RewriteCandidate &rc : calls->candidates)
        if (rc.scope != scope)
            captured.push_back(rc.rewrite->left);

    return captured.size() != 0;
}


Tree *Types::DoInteger(Integer *what)
// ----------------------------------------------------------------------------
//   Annotate an integer tree with its value
// ----------------------------------------------------------------------------
{
    return DoConstant(what, INTEGER);
}


Tree *Types::DoReal(Real *what)
// ----------------------------------------------------------------------------
//   Annotate a real tree with its value
// ----------------------------------------------------------------------------
{
    return DoConstant(what, REAL);
}


Tree *Types::DoText(Text *what)
// ----------------------------------------------------------------------------
//   Annotate a text tree with its own value
// ----------------------------------------------------------------------------
{
    return DoConstant(what, TEXT);
}


Tree *Types::DoConstant(Tree *what, kind k)
// ----------------------------------------------------------------------------
//   All constants have themselves as type, and evaluate normally
// ----------------------------------------------------------------------------
{
    Tree *type = what;
    if (context->HasRewritesFor(k))
        type = Evaluate(what);
    else
        type = AssignType(what, what); // Assign the value itself as a type
    return type;
}


Tree *Types::DoName(Name *what)
// ----------------------------------------------------------------------------
//   Assign an unknown type to a name
// ----------------------------------------------------------------------------
{
    if (declaration)
        return NewType(what);
    return Evaluate(what);
}


Tree *Types::DoPrefix(Prefix *what)
// ----------------------------------------------------------------------------
//   Assign an unknown type to a prefix and then to its children
// ----------------------------------------------------------------------------
{
    // Deal with bizarre declarations
    if (Name *name = what->left->AsName())
    {
        if (name->value == "extern")
        {
            CDeclaration *cdecl = what->GetInfo<CDeclaration>();
            if (!cdecl)
            {
                Ooops("No C declaration for $1", what);
                return nullptr;
            }
            Tree *type = RewriteType(cdecl->rewrite);
            return type;
        }
    }
    // What really matters is if we can evaluate the top-level expression
    return Evaluate(what);
}


Tree *Types::DoPostfix(Postfix *what)
// ----------------------------------------------------------------------------
//   Assign an unknown type to a postfix and then to its children
// ----------------------------------------------------------------------------
{
    // What really matters is if we can evaluate the top-level expression
    return Evaluate(what);
}


Tree *Types::DoInfix(Infix *what)
// ----------------------------------------------------------------------------
//   Assign type to infix forms
// ----------------------------------------------------------------------------
//   We deal with the following special forms:
//   - [X;Y]: a statement sequence, type is the type of last statement
//   - [X:T] and [X as T]: a type declaration, assign the type T to X
//   - [X is Y]: A declaration, assign a type [type X => type Y]
{
    // For a sequence, both sub-expressions must succeed individually.
    // The type of the sequence is the type of the last statement
    if (what->name == "\n" || what->name == ";")
        return Statements(what, what->left, what->right);

    // Case of [X : T] : Set type of [X] to [T] and unify [X:T] with [X]
    if (what->name == ":" || what->name == "as")
        return TypeDeclaration(what);

    // Case of [X is Y]: Analysis if any will be done during
    if (what->name == "is")
        return RewriteType(what);

    // For all other cases, evaluate the infix
    return Evaluate(what);
}


Tree *Types::DoBlock(Block *what)
// ----------------------------------------------------------------------------
//   A block evaluates either as itself, or as its child
// ----------------------------------------------------------------------------
{
    Tree *type = Evaluate(what, true);
    if (!type)
    {
        type = Evaluate(what->child);
        type = AssignType(what, type);
    }
    return type;
}


Tree *Types::AssignType(Tree *expr, Tree *type)
// ----------------------------------------------------------------------------
//   Set the type of the expression to be 'type'
// ----------------------------------------------------------------------------
{
    if (type)
    {
        auto it = types.find(expr);
        if (it != types.end())
        {
            Tree *existing = (*it).second;
            type = Unify(existing, type);
        }
    }
    types[expr] = type;
    return type;
}


Tree *Types::TypeOf(Tree *expr)
// ----------------------------------------------------------------------------
//   Return the type of the expr as a [type X] expression
// ----------------------------------------------------------------------------
{
    // Check if we know a type for this expression, if so return it
    auto it = types.find(expr);
    if (it != types.end())
        return (*it).second;

    // Otherwise, return [type X] and assign it to this expr
    TreePosition pos = expr->Position();
    Tree *type = new Prefix(new Name("type", pos), expr, pos);
    types[expr] = type;
    return type;
}


Tree *Types::TypeDeclaration(Infix *decl)
// ----------------------------------------------------------------------------
//   Explicitely define the type for an expression
// ----------------------------------------------------------------------------
{
    Tree *declared = decl->left;
    Tree *type = EvaluateType(decl->right);
    Tree *declt = TypeOf(declared);
    record(types_ids, "Declaration %t declared %t type %t in %p",
           decl, declared, type, this);
    type = Join(declt, type);
    return declt;
}


Tree *Types::RewriteType(Infix *what)
// ----------------------------------------------------------------------------
//   Assign an [A => B] type to a rewrite
// ----------------------------------------------------------------------------
//   Here, we are processing [X is Y]
//   There are three special cases we want to deal with:
//   - [X is builtin Op]: Check that X has types and that Op exists
//   - [X is C function]: Check that X has types and that function exists
//   - [X is self]: assign [type X] to X
//
//   In other cases, we unify the type of [X] with that of [Y] and
//   return the type [type X => type Y] for the [X is Y] expression.
{
    record(types_calls, "Processing rewrite %t in %p", what, this);

    Tree *decl = what->left;
    Tree *init = what->right;
    Tree *declt = DeclarationType(decl);
    bool inited = true;        // True if init type can be evaluated

    // Case of [X is self]:
    if (Name *name = init->AsName())
        if (name->value == "self")
            inited = false;

    // Case of [X is C name] or [X is builtin Op]
    if (Prefix *prefix = init->AsPrefix())
        if (Name *name = prefix->left->AsName())
            if (name->value == "C" || name->value == "builtin")
                inited = false;

    // Create a [type Decl => type Init] type
    Tree *initt = inited ? Type(init) : NewType(init);
    if (!declt || !initt)
    {
        record(types_calls, "Failed type for %t declt=%t initt=%t",
               what, declt, initt);
        return nullptr;
    }

    // Unify with the type of the right hand side
    initt = Unify(declt, initt);

    // If the left side has a complex shape, e.g. [X+Y], return [DT => I]
    Tree *type = new Infix("=> ", declt, initt, what->Position());
    type = AssignType(what, type);

    record(types_calls, "Rewrite for %t is %t (%t => %t)",
           what, type, decl, init);
    return type;
}


Tree *Types::Statements(Tree *expr, Tree *left, Tree *right)
// ----------------------------------------------------------------------------
//   Return the type of a combo statement, skipping declarations
// ----------------------------------------------------------------------------
{
    Tree *lt = Type(left);
    if (!lt)
        return lt;

    Tree *rt = Type(right);
    if (!rt)
        return rt;

    // Check if right term is a declaration, otherwise return that
    Tree *type = rt;
    if (IsRewriteType(rt) && !IsRewriteType(lt))
        type = lt;
    type = AssignType(expr, type);
    return type;
}


static Tree *lookupRewriteCalls(Scope *evalScope, Scope *sc,
                                Tree *what, Infix *entry, void *i)
// ----------------------------------------------------------------------------
//   Used to check if RewriteCalls pass
// ----------------------------------------------------------------------------
{
    RewriteCalls *rc = (RewriteCalls *) i;
    return rc->Check(sc, what, entry);
}


Tree *Types::Evaluate(Tree *what, bool mayFail)
// ----------------------------------------------------------------------------
//   Find candidates for the given expression and infer types from that
// ----------------------------------------------------------------------------
{
    record(types_calls, "Evaluating %t in %p", what, this);

    // Test if we are already trying to evaluate this particular form
    rcall_map::iterator found = rcalls.find(what);
    bool recursive = found != rcalls.end();
    if (recursive)
    {
        // Need to assign a type name, will be unified by outer Evaluate()
        return NewType(what);
    }

    // Identify all candidate rewrites in the current context
    RewriteCalls_p rc = new RewriteCalls(this);
    rcalls[what] = rc;
    uint count = 0;
    Errors errors;
    errors.Log (Error("Unable to evaluate $1:", what), true);
    context->Lookup(what, lookupRewriteCalls, rc);

    // If we have no candidate, this is a failure
    count = rc->candidates.size();
    if (count == 0)
    {
        if (!mayFail)
            Ooops("No form matches $1", what);
        return nullptr;
    }
    errors.Clear();
    errors.Log(Error("Unable to check types in $1 because", what), true);

    // The resulting type is the union of all candidates
    Tree *type = rc->candidates[0].type;
    for (uint i = 1; i < count; i++)
    {
        Tree *ctype = rc->candidates[i].type;
        type = UnionType(type, ctype);
    }
    type = AssignType(what, type);
    return type;
}


static Tree *lookupType(Scope *evalScope, Scope *sc,
                        Tree *what, Infix *entry, void *i)
// ----------------------------------------------------------------------------
//   Used to check if a
// ----------------------------------------------------------------------------
{
    if (Name *test = what->AsName())
        if (Name *decl = entry->left->AsName())
            if (test->value == decl->value)
                return decl;
    return nullptr;
}


Tree *Types::EvaluateType(Tree *type)
// ----------------------------------------------------------------------------
//   Find candidates for the given expression and infer types from that
// ----------------------------------------------------------------------------
{
    record(types_calls, "Evaluating type %t in %p", type, this);
    Tree *found = context->Lookup(type, lookupType, nullptr);
    if (found)
        type = Join(type, found);
    return type;
}


Tree *Types::Unify(Tree *t1, Tree *t2)
// ----------------------------------------------------------------------------
//   Unify two type forms
// ----------------------------------------------------------------------------
// Unification happens almost as "usual" for Algorithm W, except for how
// we deal with XL "shape-based" type constructors, e.g. [type P] where
// P is a pattern like [X:integer, Y:real]
{
    // Check if already unified
    if (t1 == t2)
        return t1;

    // Check if one of the sides had a type error
    if (!t1 || !t2)
        return nullptr;

    // Strip out blocks in type specification, i.e. [T] == [(T)]
    if (Block *b1 = t1->AsBlock())
        return Unify(b1->child, t2);
    if (Block *b2 = t2->AsBlock())
        return Unify(t1, b2->child);

    // Check if we have a unification for this type
    TreeMap::iterator i1 = unifications.find(t1);
    if (i1 != unifications.end())
        return Unify((*i1).second, t2);
    TreeMap::iterator i2 = unifications.find(t2);
    if (i2 != unifications.end())
        return Unify(t1, (*i2).second);

    // Lookup type names, replace them with their value
    t1 = DeclaredTypeName(t1);
    t2 = DeclaredTypeName(t2);
    if (t1 == t2)
        return t1;

    // If either is a generic, unify with the other
    if (IsGeneric(t1))
        return Join(t1, t2);
    if (IsGeneric(t2))
        return Join(t2, t1);

    // Success if t1 covers t2 or t2 covers t1
    if (TypeCoversType(t1, t2))
        return Join(t2, t1);
    if (TypeCoversType(t2, t1))
        return Join(t1, t2);

    // Check type patterns, i.e. [type X] as in [type(X:integer, Y:real)]
    if (Tree *pat1 = IsTypeOf(t1))
    {
        // If we have two type patterns, they must be structurally identical
        if (Tree *pat2 = IsTypeOf(t2))
            if (TreePatternsMatch(pat1, pat2))
                return Join(t1, t2);

        // Match a type pattern with another value
        if (TreePatternMatchesValue(pat1, t2))
            return Join(t2, t1);
    }
    if (Tree *pat2 = IsTypeOf(t2))
        if (TreePatternMatchesValue(pat2, t1))
            return Join(t1, t2);

    // Check functions [X => Y]
    if (Infix *r1 = IsRewriteType(t1))
    {
        if (Infix *r2 = IsRewriteType(t2))
        {
            Tree *ul = Unify(r1->left, r2->left);
            Tree *ur = Unify(r1->right, r2->right);
            if (!ul || !ur)
                return nullptr;
            if (ul == r1->left && ur == r1->right)
                return Join(r2, r1);
            if (ul == r2->left && ur == r2->right)
                return Join(r1, r2);
            Tree *type = new Infix("=>", ul, ur, r1->Position());
            type = Join(r1, type);
            type = Join(r2, type);
            return type;
        }
    }

    // TODO: Range versus range?

    // None of the above: fail
    return TypeError(t1, t2);
}


Tree *Types::IsTypeOf(Tree *type)
// ----------------------------------------------------------------------------
//   Check if type is a type pattern, i.e. type ( ... )
// ----------------------------------------------------------------------------
{
    if (Prefix *pfx = type->AsPrefix())
    {
        if (Name *tname = pfx->left->AsName())
        {
            if (tname->value == "type")
            {
                Tree *pattern = pfx->right;
                if (Block *block = pattern->AsBlock())
                    pattern = block->child;
                return pattern;
            }
        }
    }

    return nullptr;
}


Infix *Types::IsRewriteType(Tree *type)
// ----------------------------------------------------------------------------
//   Check if type is a rewrite type, i.e. something like [X => Y]
// ----------------------------------------------------------------------------
{
    if (Infix *infix = type->AsInfix())
        if (infix->name == "=>")
            return infix;

    return nullptr;
}


Infix *Types::IsRangeType(Tree *type)
// ----------------------------------------------------------------------------
//   Check if type is a range type, i.e. [X..Y] with [X] and [Y] constant
// ----------------------------------------------------------------------------
{
    if (Infix *infix = type->AsInfix())
    {
        if (infix->name == "..")
        {
            kind l = infix->left->Kind();
            kind r = infix->right->Kind();
            if (l == r && (l == INTEGER || l == REAL || l == TEXT))
                return infix;
        }
    }

    return nullptr;
}


Infix *Types::IsUnionType(Tree *type)
// ----------------------------------------------------------------------------
//   Check if type is a union type, i.e. something like [integer|real]
// ----------------------------------------------------------------------------
{
    if (Infix *infix = type->AsInfix())
        if (infix->name == "|")
            return infix;

    return nullptr;
}


Tree *Types::Join(Tree *old, Tree *replace)
// ----------------------------------------------------------------------------
//   Replace the old type with the new one
// ----------------------------------------------------------------------------
{
    // Deal with error cases
    if (!old || !replace)
        return nullptr;
    if (old == replace)
        return old;

    // Replace the type in the types map
    for (auto &t : types)
        if (t.second == old)
            t.second = replace;

    // Replace the type in the 'unifications' map
    for (auto &u : unifications)
        if (u.second == old)
            u.second = replace;
    unifications.erase(replace);
    unifications[old] = replace;

    // Replace the type in the rewrite calls
    for (auto &r : rcalls)
        for (auto &rc : r.second->candidates)
            if (rc.type == old)
                rc.type = replace;

    return replace;
}


Tree *Types::UnionType(Tree *t1, Tree *t2)
// ----------------------------------------------------------------------------
//    Create the union of two types
// ----------------------------------------------------------------------------
{
    if (t1 == t2)
        return t1;
    if (!t1 || !t2)
        return nullptr;

    if (TypeCoversType(t1, t2))
        return t1;
    if (TypeCoversType(t2, t1))
        return t2;

    // TODO: Union of range types and constants

    return new Infix("|", t1, t2, t1->Position());

}


bool Types::TypeCoversConstant(Tree *type, Tree *cst)
// ----------------------------------------------------------------------------
//    Check if a type covers a constant or range
// ----------------------------------------------------------------------------
{
    // Deal with type errors
    if (!type || !cst)
        return false;

    // If the type is something like 0..3, set 'range' to that range
    Infix *range = IsRangeType(type);

    // Check if we match against some sized type, otherwise force type
    if (Integer *icst = cst->AsInteger())
    {
        if (type == integer_type   || type == unsigned_type   ||
            type == integer8_type  || type == unsigned8_type  ||
            type == integer16_type || type == unsigned16_type ||
            type == integer32_type || type == unsigned32_type ||
            type == integer64_type || type == unsigned64_type)
            return true;
        if (range)
            if (Integer *il = range->left->AsInteger())
                if (Integer *ir = range->right->AsInteger())
                    return
                        icst->value >= il->value &&
                        icst->value <= ir->value;
        return false;
    }

    if (Real *rcst = cst->AsReal())
    {
        if (type == real_type   ||
            type == real64_type ||
            type == real32_type)
            return true;
        if (range)
            if (Real *rl = range->left->AsReal())
                if (Real *rr = range->right->AsReal())
                    return
                        rcst->value >= rl->value &&
                        rcst->value <= rr->value;
        return false;
    }

    if (Text *tcst = cst->AsText())
    {
        bool isChar = tcst->IsCharacter();
        if ((isChar  && type == character_type) ||
            (!isChar && type == text_type))
            return true;

        if (range)
            if (Text *tl = range->left->AsText())
                if (Text *tr = range->right->AsText())
                    return
                        tl->IsCharacter() == isChar &&
                        tr->IsCharacter() == isChar &&
                        tcst->value >= tl->value &&
                        tcst->value <= tr->value;
        return false;
    }
    return false;
}


bool Types::TypeCoversType(Tree *top, Tree *bottom)
// ----------------------------------------------------------------------------
//   Check if the top type covers all values in the bottom type
// ----------------------------------------------------------------------------
{
    // Deal with type errors
    if (!top || !bottom)
        return false;

    // Quick exit when types are the same or the tree type is used
    if (top == bottom)
        return true;
    if (top == tree_type)
        return true;
    if (Infix *u = IsUnionType(top))
        if (TypeCoversType(u->left, bottom) || TypeCoversType(u->right, bottom))
            return true;
    if (TypeCoversConstant(top, bottom))
        return true;

    // Failed to match type
    return false;
}


bool Types::TreePatternsMatch(Tree *t1, Tree *t2)
// ----------------------------------------------------------------------------
//   Check if two patterns describe the same tree shape
// ----------------------------------------------------------------------------
//   Patterns have to match exactly, i.e. [X:integer] matches [X:integer]
//   but neither [Y:integer] nor [X:0]
{
    if (!t1 || !t2)
        return false;
    if (t1 == t2)
        return true;

    switch(t1->Kind())
    {
    case INTEGER:
        if (Integer *x1 = t1->AsInteger())
            if (Integer *x2 = t2->AsInteger())
                return x1->value == x2->value;
        return false;

    case REAL:
        if (Real *x1 = t1->AsReal())
            if (Real *x2 = t2->AsReal())
                return x1->value == x2->value;
        return false;

    case TEXT:
        if (Text *x1 = t1->AsText())
            if (Text *x2 = t2->AsText())
                return x1->value == x2->value;
        return false;

    case NAME:
        // We don't attempt to allow renames. Names must match, it's simpler.
        if (Name *x1 = t1->AsName())
            if (Name *x2 = t2->AsName())
                return x1->value == x2->value;
        return false;

    case INFIX:
        if (Infix *x1 = t1->AsInfix())
            if (Infix *x2 = t2->AsInfix())
                if (x1->name == x2->name &&
                    TreePatternsMatch(x1->left, x2->left) &&
                    TreePatternsMatch(x1->right, x2->right))
                    return true;
        return false;

    case PREFIX:
        if (Prefix *x1 = t1->AsPrefix())
            if (Prefix *x2 = t2->AsPrefix())
                return
                    TreePatternsMatch(x1->left, x2->left) &&
                    TreePatternsMatch(x1->right, x2->right);

        return false;

    case POSTFIX:
        if (Postfix *x1 = t1->AsPostfix())
            if (Postfix *x2 = t2->AsPostfix())
                return
                    TreePatternsMatch(x1->left, x2->left) &&
                    TreePatternsMatch(x1->right, x2->right);

        return false;

    case BLOCK:
        if (Block *x1 = t1->AsBlock())
            if (Block *x2 = t2->AsBlock())
                return
                    x1->opening == x2->opening &&
                    x1->closing == x2->closing &&
                    TreePatternsMatch(x1->child, x2->child);

        return false;

    }

    return false;
}


bool Types::TreePatternMatchesValue(Tree *pat, Tree *val)
// ----------------------------------------------------------------------------
//   Check if the pattern matches some tree value
// ----------------------------------------------------------------------------
{
    if (!pat || !val)
        return false;
    if (pat == val)             // May occur as [type X] vs [X]
        return true;

    switch(pat->Kind())
    {
    case INTEGER:
        if (Integer *x1 = pat->AsInteger())
            if (Integer *x2 = val->AsInteger())
                return x1->value == x2->value;
        return false;

    case REAL:
        if (Real *x1 = pat->AsReal())
            if (Real *x2 = val->AsReal())
                return x1->value == x2->value;
        return false;

    case TEXT:
        if (Text *x1 = pat->AsText())
            if (Text *x2 = val->AsText())
                return x1->value == x2->value;
        return false;

    case NAME:
        // A name at that stage is a variable, so we match
        // PROBLEM: matching X+X will match twice?
        return Unify(pat, Type(val));

    case INFIX:
        if (Infix *x1 = pat->AsInfix())
        {
            // Check if the pattern is a type declaration
            if (x1->name == ":")
                return Unify(x1->right, Type(val));

            if (Infix *x2 = val->AsInfix())
                return
                    x1->name == x2->name &&
                    TreePatternMatchesValue(x1->left, x2->left) &&
                    TreePatternMatchesValue(x1->right, x2->right);
        }

        return false;

    case PREFIX:
        if (Prefix *x1 = pat->AsPrefix())
            if (Prefix *x2 = val->AsPrefix())
                return
                    TreePatternsMatch(x1->left, x2->left) &&
                    TreePatternMatchesValue(x1->right, x2->right);

        return false;

    case POSTFIX:
        if (Postfix *x1 = pat->AsPostfix())
            if (Postfix *x2 = val->AsPostfix())
                return
                    TreePatternMatchesValue(x1->left, x2->left) &&
                    TreePatternsMatch(x1->right, x2->right);

        return false;

    case BLOCK:
        if (Block *x1 = pat->AsBlock())
            if (Block *x2 = val->AsBlock())
                return
                    x1->opening == x2->opening &&
                    x1->closing == x2->closing &&
                    TreePatternMatchesValue(x1->child, x2->child);

        return false;

    }

    return false;
}


Name * Types::NewTypeName(TreePosition pos)
// ----------------------------------------------------------------------------
//   Automatically generate new type names
// ----------------------------------------------------------------------------
{
    ulong v = id++;
    text  name;
    do
    {
        name = char('A' + v % 26) + name;
        v /= 26;
    } while (v);
    return new Name("#" + name, pos);
}


Tree *Types::DeclaredTypeName(Tree *type)
// ----------------------------------------------------------------------------
//   If we have a type name, lookup its definition
// ----------------------------------------------------------------------------
{
    if (Name *name = type->AsName())
    {
        // Don't lookup type variables (generic names such as #A)
        if (IsGeneric(name->value))
            return name;

        // Check if we have a type definition. If so, use it
        Rewrite_p rewrite;
        Scope_p scope;
        Tree *definition = context->Bound(name, true, &rewrite, &scope);
        if (definition && definition != name)
        {
            type = Join(type, definition);
            type = Join(type, rewrite->left);
        }
    }

    // By default, return input type
    return type;
}


Tree *Types::TypeError(Tree *t1, Tree *t2)
// ----------------------------------------------------------------------------
//   Show type matching errors
// ----------------------------------------------------------------------------
{
    Tree *x1 = nullptr;
    Tree *x2 = nullptr;
    for (auto &t : types)
    {
        if (t.second == t1)
            x1 = t.first;
        if (t.second == t2)
            x2 = t.first;
    }

    if (x1 == x2)
    {
        Ooops("Type of $1 cannot be both $2 and $3", x1, t1, t2);
    }
    else
    {
        Ooops("Cannot unify type $2 of $1", x1, t1);
        Ooops("with type $2 of $1", x2, t2);
    }
    return nullptr;
}



// ============================================================================
//
//   Debug utilities
//
// ============================================================================

void Types::DumpTypes()
// ----------------------------------------------------------------------------
//   Dump the list of types in this
// ----------------------------------------------------------------------------
{
    uint i = 0;

    for (auto &t : types)
    {
        Tree *value = t.first;
        Tree *type = t.second;
        Tree *base = BaseType(type);
        std::cout << "#" << ++i
                  << "\t" << ShortTreeForm(value)
                  << "\t: " << type;
        if (base != type)
            std::cout << "\t= " << base;
        std::cout << "\n";
    }
}


void Types::DumpUnifications()
// ----------------------------------------------------------------------------
//   Dump the current unifications
// ----------------------------------------------------------------------------
{
    uint i = 0;
    for (auto &t : unifications)
    {
        Tree *type = t.first;
        Tree *base = t.second;
        std::cout << "#" << ++i
                  << "\t" << type
                  << "\t= " << base << "\n";
    }
}


void Types::DumpRewriteCalls()
// ----------------------------------------------------------------------------
//   Dump the list of rewrite calls
// ----------------------------------------------------------------------------
{
    uint i = 0;
    for (auto &t : rcalls)
    {
        Tree *expr = t.first;
        std::cout << "#" << ++i << "\t" << ShortTreeForm(expr) << "\n";

        uint j = 0;
        RewriteCalls *calls = t.second;
        RewriteCandidates &rc = calls->candidates;
        for (auto &r : rc)
        {
            std::cout << "\t#" << ++j
                      << "\t" << r.rewrite->left
                      << "\t: " << r.type << "\n";

            RewriteConditions &rt = r.conditions;
            for (auto &t : rt)
                std::cout << "\t\tWhen " << ShortTreeForm(t.value)
                          << "\t= " << ShortTreeForm(t.test) << "\n";

            RewriteBindings &rb = r.bindings;
            for (auto &b : rb)
            {
                std::cout << "\t\t" << b.name;
                std::cout << "\t= " << ShortTreeForm(b.value) << "\n";
            }
        }
    }
}

XL_END


void debugt(XL::Types *ti)
// ----------------------------------------------------------------------------
//   Dump a type inference
// ----------------------------------------------------------------------------
{
    if (!XL::Allocator<XL::Types>::IsAllocated(ti))
    {
        std::cout << "Cowardly refusing to show bad Types pointer "
                  << (void *) ti << "\n";
        return;
    }
    ti->DumpTypes();
}


void debugu(XL::Types *ti)
// ----------------------------------------------------------------------------
//   Dump type unifications in a given inference system
// ----------------------------------------------------------------------------
{
    if (!XL::Allocator<XL::Types>::IsAllocated(ti))
    {
        std::cout << "Cowardly refusing to show bad Types pointer "
                  << (void *) ti << "\n";
        return;
    }
    ti->DumpUnifications();
}



void debugr(XL::Types *ti)
// ----------------------------------------------------------------------------
//   Dump rewrite calls associated with each tree in this type inference system
// ----------------------------------------------------------------------------
{
    if (!XL::Allocator<XL::Types>::IsAllocated(ti))
    {
        std::cout << "Cowardly refusing to show bad Types pointer "
                  << (void *) ti << "\n";
        return;
    }
    ti->DumpRewriteCalls();
}

RECORDER(types,                 64, "Type analysis");
RECORDER(types_ids,             64, "Assigned type identifiers");
RECORDER(types_unifications,    64, "Type unifications");
RECORDER(types_calls,           64, "Type deductions in rewrites (calls)")