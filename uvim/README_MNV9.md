![MNV Logo](https://github.com/Project-Tick/Project-Tick/blob/master/runtime/mnvlogo.gif)

# What is MNV9?

This is a new syntax for MNV script that was introduced with MNV 9.0.
It intends making MNV script faster and better.


# Why MNV9?

## 1. FASTER MNV SCRIPT

The third item on the poll results of 2018, after popup windows and text
properties, both of which have been implemented, is faster MNV script.
So how do we do that?

I have been throwing some ideas around, and soon came to the conclusion
that the current way functions are called and executed, with
dictionaries for the arguments and local variables, is never going to be
very fast.  We're lucky if we can make it twice as fast.  The overhead
of a function call and executing every line is just too high.

So what then?  We can only make something fast by having a new way of
defining a function, with similar but different properties of the old
way:
* Arguments are only available by name, not through the a: dictionary or
  the a:000 list.
* Local variables are not available in an l: dictionary.
* A few more things that slow us down, such as exception handling details.

I implemented a "proof of concept" and measured the time to run a simple
for loop with an addition (Justin used this example in his presentation,
full code is below):

``` mnv
  let sum = 0
  for i in range(1, 2999999)
    let sum += i
  endfor
```

| how     | time in sec |
| --------| -------- |
| MNV old | 5.018541 |
| Python  | 0.369598 |
| Lua     | 0.078817 |
| LuaJit  | 0.004245 |
| MNV new | 0.073595 |

That looks very promising!  It's just one example, but it shows how much
we can gain, and also that MNV script can be faster than builtin
interfaces.

LuaJit is much faster at Lua-only instructions.  In practice the script would
not do something useless counting, but change the text.  For example,
reindent all the lines:

``` mnv
  let totallen = 0
  for i in range(1, 100000)
    call setline(i, '    ' .. getline(i))
    let totallen += len(getline(i))
  endfor
```

| how     | time in sec |
| --------| -------- |
| MNV old | 0.578598 |
| Python  | 0.152040 |
| Lua     | 0.164917 |
| LuaJit  | 0.128400 |
| MNV new | 0.079692 |

[These times were measured on a different system by Dominique Pelle]

The differences are smaller, but MNV 9 script is clearly the fastest.
Using LuaJIT is only a little bit faster than plain Lua here, clearly the call
back to the MNV code is costly.

How does MNV9 script work?  The function is first compiled into a sequence of
instructions.  Each instruction has one or two parameters and a stack is
used to store intermediate results.  Local variables are also on the
stack, space is reserved during compilation.  This is a fairly normal
way of compilation into an intermediate format, specialized for MNV,
e.g. each stack item is a typeval_T.  And one of the instructions is
"execute Ex command", for commands that are not compiled.


## 2. DEPRIORITIZE INTERFACES

Attempts have been made to implement functionality with built-in script
languages such as Python, Perl, Lua, Tcl and Ruby.  This never gained much
foothold, for various reasons.

Instead of using script language support in MNV:
* Encourage implementing external tools in any language and communicate
  with them.  The job and channel support already makes this possible.
  Really any language can be used, also Java and Go, which are not
  available built-in.
* No priority for the built-in language interfaces.  They will have to be kept
  for backwards compatibility, but many users won't need a MNV build with these
  interfaces.
* Improve the MNV script language, it is used to communicate with the external
  tool and implements the MNV side of the interface.  Also, it can be used when
  an external tool is undesired.

Altogether this creates a clear situation: MNV with the +eval feature
will be sufficient for most plugins, while some plugins require
installing a tool that can be written in any language.  No confusion
about having MNV but the plugin not working because some specific
language is missing.  This is a good long term goal.

Rationale: Why is it better to run a tool separately from MNV than using a
built-in interface and interpreter?  Take for example something that is
written in Python:
* The built-in interface uses the embedded python interpreter.  This is less
  well maintained than the python command.  Building MNV with it requires
  installing developer packages.  If loaded dynamically there can be a version
  mismatch.
* When running the tool externally the standard python command can be used,
  which is quite often available by default or can be easily installed.
* The built-in interface has an API that is unique for MNV with Python. This is
  an extra API to learn.
* A .py file can be compiled into a .pyc file and execute much faster.
* Inside MNV multi-threading can cause problems, since the MNV core is single
  threaded.  In an external tool there are no such problems.
* The MNV part is written in .mnv files, the Python part is in .py files, this
  is nicely separated.
* Disadvantage: An interface needs to be made between MNV and Python.
  JSON is available for this, and it's fairly easy to use.  But it still
  requires implementing asynchronous communication.


## 3. BETTER MNV SCRIPT

To make MNV faster a new way of defining a function needs to be added.
While we are doing that, since the lines in this function won't be fully
backwards compatible anyway, we can also make MNV script easier to use.
In other words: "less weird".  Making it work more like modern
programming languages will help.  No surprises.

A good example is how in a function the arguments are prefixed with
"a:". No other language I know does that, so let's drop it.

Taking this one step further is also dropping "s:" for script-local variables;
everything at the script level is script-local by default.  Since this is not
backwards compatible it requires a new script style: MNV9 script!

To avoid having more variations, the syntax inside a compiled function is the
same as in MNV9 script.  Thus you have legacy syntax and MNV9 syntax.

It should be possible to convert code from other languages to MNV
script.  We can add functionality to make this easier.  This still needs
to be discussed, but we can consider adding type checking and a simple
form of classes.  If you look at JavaScript for example, it has gone
through these stages over time, adding real class support and now
TypeScript adds type checking.  But we'll have to see how much of that
we actually want to include in MNV script.  Ideally a conversion tool
can take Python, JavaScript or TypeScript code and convert it to MNV
script, with only some things that cannot be converted.

MNV script won't work the same as any specific language, but we can use
mechanisms that are commonly known, ideally with the same syntax.  One
thing I have been thinking of is assignments without ":let".  I often
make that mistake (after writing JavaScript especially).  I think it is
possible, if we make local variables shadow commands.  That should be OK,
if you shadow a command you want to use, just rename the variable.
Using "var" and "const" to declare a variable, like in JavaScript and
TypeScript, can work:


``` mnv
def MyFunction(arg: number): number
   var local = 1
   var todo = arg
   const ADD = 88
   while todo > 0
      local += ADD
      todo -= 1
   endwhile
   return local
enddef
```

The similarity with JavaScript/TypeScript can also be used for dependencies
between files.  MNV currently uses the `:source` command, which has several
disadvantages:
*   In the sourced script, is not clear what it provides.  By default all
    functions are global and can be used elsewhere.
*   In a script that sources other scripts, it is not clear what function comes
    from what sourced script.  Finding the implementation is a hassle.
*   Prevention of loading the whole script twice must be manually implemented.

We can use the `:import` and `:export` commands from the JavaScript standard to
make this much better.  For example, in script "myfunction.mnv" define a
function and export it:

``` mnv
mnv9script  " MNV9 script syntax used here

var local = 'local variable is not exported, script-local'

export def MyFunction()  " exported function
...

def LocalFunction() " not exported, script-local
...
```

And in another script import the function:

``` mnv
mnv9script  " MNV9 script syntax used here

import MyFunction from 'myfunction.mnv'
```

This looks like JavaScript/TypeScript, thus many users will understand the
syntax.

These are ideas, this will take time to design, discuss and implement.
Eventually this will lead to MNV 9!


## Code for sum time measurements

MNV was built with -O2.

``` mnv
func MNVOld()
  let sum = 0
  for i in range(1, 2999999)
    let sum += i
  endfor
  return sum
endfunc

func Python()
  py3 << END
sum = 0
for i in range(1, 3000000):
  sum += i
END
  return py3eval('sum')
endfunc

func Lua()
  lua << END
    sum = 0
    for i = 1, 2999999 do
      sum = sum + i
    end
END
  return luaeval('sum')
endfunc

def MNVNew(): number
  var sum = 0
  for i in range(1, 2999999)
    sum += i
  endfor
  return sum
enddef

let start = reltime()
echo MNVOld()
echo 'MNV old: ' .. reltimestr(reltime(start))

let start = reltime()
echo Python()
echo 'Python: ' .. reltimestr(reltime(start))

let start = reltime()
echo Lua()
echo 'Lua: ' .. reltimestr(reltime(start))

let start = reltime()
echo MNVNew()
echo 'MNV new: ' .. reltimestr(reltime(start))
```

## Code for indent time measurements

``` mnv
def MNVNew(): number
  var totallen = 0
  for i in range(1, 100000)
    setline(i, '    ' .. getline(i))
    totallen += len(getline(i))
  endfor
  return totallen
enddef

func MNVOld()
  let totallen = 0
  for i in range(1, 100000)
    call setline(i, '    ' .. getline(i))
    let totallen += len(getline(i))
  endfor
  return totallen
endfunc

func Lua()
  lua << END
    b = mnv.buffer()
    totallen = 0
    for i = 1, 100000 do
      b[i] = "    " .. b[i]
      totallen = totallen + string.len(b[i])
    end
END
  return luaeval('totallen')
endfunc

func Python()
  py3 << END
cb = mnv.current.buffer
totallen = 0
for i in range(0, 100000):
  cb[i] = '    ' + cb[i]
  totallen += len(cb[i])
END
  return py3eval('totallen')
endfunc

new
call setline(1, range(100000))
let start = reltime()
echo MNVOld()
echo 'MNV old: ' .. reltimestr(reltime(start))
bwipe!

new
call setline(1, range(100000))
let start = reltime()
echo Python()
echo 'Python: ' .. reltimestr(reltime(start))
bwipe!

new
call setline(1, range(100000))
let start = reltime()
echo Lua()
echo 'Lua: ' .. reltimestr(reltime(start))
bwipe!

new
call setline(1, range(100000))
let start = reltime()
echo MNVNew()
echo 'MNV new: ' .. reltimestr(reltime(start))
bwipe!
```
