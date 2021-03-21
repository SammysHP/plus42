About Plus42

Plus42 is an advanced scientific programmable calculator, based on Free42.
Free42 is a complete re-implementation of the HP-42S scientific programmable
RPN calculator, which was made from 1988 until 1995 by Hewlett-Packard.
Free42 is a complete rewrite and contains no HP code whatsoever.
Plus42 builds on Free42 and adds algebraic expressions modeled after those
used on the HP-27S and HP-17B/19B; attached units and unit conversions modeled
after those used on the HP-48/49/50 series; directories for more organized
storage of programs and variables; and a larger display, with 131x64 pixels
or up to 8 lines of text.
At this time, the author supports versions that run on Android, iOS, Microsoft
Windows, MacOS, and Linux.


Installing Plus42:

Copy Plus42Decimal.exe (or Plus42Binary.exe, or both) to wherever you want it,
e.g. create a directory "C:\Program Files\Plus42" and put it there.
When Plus42 runs, it will create three additional files; they are state.bin,
print.bin, and keymap.txt, and they are used to store the calculator's internal
state, the contents of the print-out window, and the PC keyboard map,
respectively.
By default, these additional files will be stored in %APPDATA%\Plus42, which is
typically C:\Users\<YourUserName>\AppData\Roaming\Plus42.
If you want to run Plus42 from a removable drive, like a USB stick, and keep
the state.bin, print.bin, and keymap.txt file with Plus42 on that drive, create
a file named "portable" in the same directory as Plus42Decimal.exe or
Plus42Binary.exe. When Plus42 sees this file, it will ignore %APPDATA%\Plus42
and keep those files in its own directory.

Plus42 comes with two skins built in, but you may use different ones, by
storing them in the directory where Plus42Decimal.exe or Plus42Binary are
located, or (if not running from a removable drive with the "portable" file),
in %APPDATA%\Plus42.

If Plus42 does not run as is, you will also need to download and install the
Visual C++ Redistributable for Visual Studio 2015 / 2017 / 2019.
You can download this package from
https://www.microsoft.com/en-us/download/details.aspx?id=48145
(get the file vc_redist.x86.exe)


Uninstalling Plus42:

Remove Plus42Decimal.exe, Plus42Binary.exe, and %APPDATA%\Plus42 and its
contents.


Documentation

Visit https://thomasokken.com/plus42/#doc for more information.


Keyboard Mapping

You don't have to use the mouse to press the keys of the emulated calculator
keyboard; all keys can be operated using the PC's keyboard as well. The
standard keyboard mapping is as follows:

Σ+       F1, or 'a' as in "Accumulate"
Σ-       Shift F1, or 'A' (Shift a)
1/X      F2, or 'v' as in "inVerse"
Y^X      Shift F2, or 'V' (Shift v)
√x       F3, or 'q' as in "sQuare root"
X^2      Shift F3, or 'Q' (Shift q)
LOG      F4, or 'o' as in "lOg, not ln"
10^X     Shift F4, or 'O' (Shift o)
LN       F5, or 'l' as in "Ln, not log"
E^X      Shift F5, or 'L" (Shift l)
XEQ      F6, or 'x' as in "Xeq"
GTO      Shift F6, or 'X' (Shift x), or 'g' as in "Gto"

STO      'm' as in "Memory"
COMPLEX  'M' (Shift m)
RCL      'r' as in "Rcl"
%        'R' (Shift r)
R↓       'd' as in "Down"
π        'D' (Shift d), or 'p' as in "Pi"
SIN      's' as in "Sin"
ASIN     'S' (Shift s)
COS      'c' as in "Cos"
ACOS     'C' (Shift c)
TAN      't' as in "Tan"
ATAN     'T' (Shift t)

ENTER    Enter or Return
ALPHA    Shift Enter or Shift Return
X<>Y     'w' as in "sWap"
LASTX    'W' (Shift w)
+/-      'n' as in "Negative"
MODES    'N' (Shift n)
E        'e' as in "Exponent" (duh...)
DISP     'E' (Shift e)
<-       Backspace
CLEAR    Shift Backspace

▲        CursorUp
BST      Shift CursorUp
7        '7'
SOLVER   '&' (Shift 7)
8        '8'
∫f(x)    Alt 8 (can't use Shift 8 because that's 'x' (multiply))
9        '9'
MATRIX   '(' (Shift 9)
÷        '/'
STAT     '?' (Shift /)

▼        CursorDown
SST      Shift CursorDown
4        '4'
BASE     '$' (Shift 4)
5        '5'
CONVERT  '%' (Shift 5)
6        '6'
FLAGS    '^' (Shift 6)
×        '*'
PROB     Ctrl 8 (can't use Shift * because '*' is shifted itself (Shift 8))

Shift    Shift
1        '1'
ASSIGN   '!' (Shift 1)
2        '2'
CUSTOM   '@' (Shift 2)
3        '3'
PGM.FCN  '#' (Shift 3)
-        '-'
PRINT    '_' (Shift -)

EXIT     Escape
OFF      Shift Escape
0        '0'
TOP.FCN  ')' (Shift 0)
.        . or ,
SHOW     '<' or '>' (Shift . or Shift ,)
R/S      '\' (ummm... because it's close to Enter (or Return))
PRGM     '|' (Shift \)
+        '+'
CATALOG  '=' (Can't use Shift + because + is shifted itself (shift =))

In A..F mode (meaning the "A..F" submenu of the BASE menu), the PC keyboard
keys A through F are mapped to the top row of the calculator's keyboard (Σ+
through XEQ); these mappings override any other mappings that may be defined
for A through F.

In ALPHA mode, all PC keyboard keys that normally generate printable ASCII
characters, enter those characters into the ALPHA register (or to the command
argument, if a command with an alphanumeric argument is being entered). These
mappings override any other mappings that may be defined for those keys.


What's the deal with the "Decimal" and "Binary"?

Starting with release 1.4, Plus42 comes in decimal and binary versions. The two
look and behave identically; the only difference is the way they represent
numbers internally.
Plus42 Decimal uses the Intel Decimal Floating-Point Math Library;
it uses IEEE-754-2008 quadruple precision decimal floating point, which
consumes 16 bytes per number, and gives 34 decimal digits of precision, with
exponents ranging from -6143 to +6144.
Plus42 Binary uses the PC's FPU; it represents numbers as IEEE-754
compatible double precision binary floating point, which consumes 8 bytes per
number, and gives an effective precision of nearly 16 decimal digits, with
exponents ranging from -308 to +308.
The binary version has the advantage of being much faster than the decimal
version; also, it uses less memory. However, numbers such as 0.1 (one-tenth)
cannot be represented exactly in binary, since they are repeating fractions
then. This inexactness can cause some HP-42S programs to fail.
If you understand the issues surrounding binary floating point, and you do not
rely on legacy software that may depend on the exactness of decimal fractions,
you may use Plus42 Binary and enjoy its speed advantage. If, on the other hand,
you need full HP-42S compatibility, you should use Plus42 Decimal.
If you don't fully understand the above, it is best to play safe and use
Plus42 Decimal.


Plus42 is (C) 2004-2021, by Thomas Okken
Contact the author at thomasokken@gmail.com
Look for updates, and versions for other operating systems, at
https://thomasokken.com/plus42/
