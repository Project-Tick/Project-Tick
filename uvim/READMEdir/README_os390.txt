README_os390.txt for version 9.2 of MNV: MNV is not Vim.

This readme explains how to build MNV on z/OS.  Formerly called OS/390.
See "README.txt" for general information about MNV.

Most likely there are not many users out there using MNV on z/OS. So chances
are good, that some bugs are still undiscovered.

Getting the source to z/OS:
==========================

First get the source code in one big tar file and ftp it a binary to z/OS. If
the tar file is initially compressed with gzip (tar.gz) or bzip2 (tar.bz2)
uncompress it on your PC, as these tools are (most likely) not available on
the mainframe.

To reduce the size of the tar file you might compress it into a zip file. On
z/OS Unix you might have the command "jar" from java to uncompress a zip. Use:
        jar xvf <zip file name>

Unpack the tar file on z/OS with 
        pax -o from=ISO8859-1,to=IBM-1047 -rf mnv.tar

Note: The MNV source contains a few bitmaps etc which will be destroyed by
this command, but these files are not needed on zOS (at least not for the
console version).


Compiling:
==========

MNV can be compiled with or without GUI support. For 7.4 only the compilation
without GUI was tested. Below is a section about compiling with X11 but this
is from an earlier version of MNV.

Console only:
-------------

If you build MNV without X11 support, compiling and building is nearly
straightforward. 

Change to the mnv directory and do:

    # Don't use c89!
    # Allow intermixing of compiler options and files.

    $ export CC=cc
    $ export _CC_CCMODE=1
    $./configure --with-features=normal --without-x --enable-gui=no
    $ cd src
    $ make

      There may be warnings:
        - include files not found (libc, sys/param.h, ...)
        - Redeclaration of ... differs from ...
        -- just ignore them.

    $ make test

      This will produce lots of garbage on your screen (including error
      messages). Don't worry.

      If the test stops at one point in mnv (might happen in test 11), just
      press :q!

      Expected test failures:
        11: If you don't have gzip installed
        24: test of backslash sequences in regexp are ASCII dependent
        42: Multibyte is not supported on z/OS
        55: ASCII<->EBCDIC sorting
        57: ASCII<->EBCDIC sorting
        58: Spell checking is not supported with EBCDIC
        71: Blowfish encryption doesn't work

    $ make install


With X11:
---------

WARNING: This instruction was not tested with MNV 7.4 or later.

There are two ways for building MNV with X11 support. The first way is simple
and results in a big executable (~13 Mb), the second needs a few additional
steps and results in a much smaller executable (~4.5 Mb). These examples
assume you want Motif.

  The easy way:
    $ export CC=cc
    $ export _CC_CCMODE=1
    $ ./configure --enable-max-features --enable-gui=motif
    $ cd src
    $ make

    With this MNV is linked statically with the X11 libraries.

  The smarter way:
    Make MNV as described above. Then create a file named 'link.sed' with the
    following content (see src/link.390):

	s/-lXext  *//g
	s/-lXmu  *//g
	s/-lXm	*/\/usr\/lib\/Xm.x /g
	s/-lX11  */\/usr\/lib\/X11.x /g
	s/-lXt	*//g
	s/-lSM	*/\/usr\/lib\/SM.x /g
	s/-lICE  */\/usr\/lib\/ICE.x /g

    Then do:
    $ rm mnv
    $ make

    Now MNV is linked with the X11-DLLs.

    See the Makefile and the file link.sh on how link.sed is used.


