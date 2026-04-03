#
# Makefile to run all tests for MNV, on Amiga
#
# Requires "rm", "csh" and "diff"!

MNVPROG = /mnv

default: nongui

include Make_all.mak

SCRIPTS = $(SCRIPTS_TINY_OUT)

.SUFFIXES: .in .out .res .mnv

nongui:	/tmp $(SCRIPTS)
	csh -c echo ALL DONE

clean:
	csh -c \rm -rf *.out Xdir1 Xfind XfakeHOME Xdotest test.ok mnvinfo

.in.out:
	copy $*.ok test.ok
	$(MNVPROG) -u util/amiga.mnv -U NONE --noplugin --not-a-term -s dotest.in $*.in
	diff test.out $*.ok
	rename test.out $*.out
	-delete X#? ALL QUIET
	-delete test.ok

# Create a directory for temp files
/tmp:
	makedir /tmp

# Manx requires all dependencies, but we stopped updating them.
# Delete the .out file(s) to run test(s).
