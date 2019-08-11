# XL, an extensible programming language

> **WARNING**: XL is a work in progress. Even if there are some bits
> and pieces that happen to already work, XL is presently not suitable
> for any serious programming. Examples given below may sometimes simply not
> work. Take it as a painful reminder that the work is far from finished,
> and, who knows, as an idea for a contribution.
> See [HISTORY](HISTORY.md) for how we came to the present mess.
> The [README](../README.md) will give you a quick overview of the language.

XL is an extensible programming language. It is designed to accomodate
a variety of programming needs with ease. Being _extensible_ means
that the language is designed to make it very easy for programmers to
adapt the language to suit their needs, for example by adding new
programming constructs. In XL, extending the language is a routine
operation, much like adding a function or creating a class in more
traditional programming languages.

As a consequence of this extensibility, XL is intended to be suitable
for programming tasks ranging from the simplest to the most complex,
from documents and application scripting, as illustrated by
[Tao3D](https://tao3d.sf.net), to compilers, as illustrated by the XL2
[self-compiling compiler](../xl2/native) to distributed programming,
as illustrated by [ELFE](https://github.com/c3d/elfe).

Table of contents:
* [Introduction](HANDBOOK_0-introduction.md)
* [Syntax](HANDBOOK_1-syntax.md)
* [Program evaluation](HANDBOOK_2-evaluation.md)
* [Type system](HANDBOOK_3-type-system.md)
* [Basic operations](HANDBOOK_4-basic-operations.md)
* [Standard library](HANDBOOK_5-standard-library.md)