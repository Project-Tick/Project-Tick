#
# Makefile to run all tests for MNV, on Dos-like machines.
#
# Requires a set of Unix tools: echo, diff, etc.
#

# included common tools
!INCLUDE ..\auto\nmake\tools.mak

# Testing may be done with a debug build 
!IF EXIST(..\\mnvd.exe) && !EXIST(..\\mnv.exe)
MNVPROG = ..\\mnvd.exe
!ELSE
MNVPROG = ..\\mnv.exe
!ENDIF

DIFF = diff.exe

default: nongui

!INCLUDE .\Make_all.mak

# Explicit dependencies.
test_options_all.res: opt_test.mnv

TEST_OUTFILES = $(SCRIPTS_TINY_OUT)
DOSTMP = dostmp
DOSTMP_OUTFILES = $(TEST_OUTFILES:test=dostmp\test)
DOSTMP_INFILES = $(DOSTMP_OUTFILES:.out=.in)

.SUFFIXES: .in .out .res .mnv

# Add --gui-dialog-file to avoid getting stuck in a dialog.
COMMON_ARGS = $(NO_INITS) --gui-dialog-file guidialog

nongui:	nolog tinytests newtests report

gui:	nolog tinytests newtests report

tiny:	nolog tinytests report

benchmark: $(SCRIPTS_BENCH)

codestyle:
	-@ if exist test_codestyle.res $(RM) test_codestyle.res
	-@ if exist test.log $(RM) test.log
	-@ if exist messages $(RM) messages
	-@ if exist starttime $(RM) starttime
	@ $(MAKE) -lf Make_mvc.mak MNVPROG=$(MNVPROG) test_codestyle.res
	@ type messages
	@ if exist test.log exit 1

report:
	@ rem without the +eval feature test_result.log is a copy of test.log
	@ if exist test.log ( $(CP) test.log test_result.log > nul ) \
		else ( echo No failures reported > test_result.log )
	$(MNVPROG) -u NONE $(COMMON_ARGS) -S util\summarize.mnv messages
	- if exist starttime $(RM) starttime
	@ echo:
	@ echo Test results:
	@ $(CMD) /C type test_result.log
	@ if exist test.log ( echo TEST FAILURE & exit /b 1 ) \
		else ( echo ALL DONE )


# Execute an individual new style test, e.g.:
# 	nmake -f Make_mvc.mak test_largefile
$(NEW_TESTS):
	- if exist $@.res $(RM) $@.res
	- if exist test.log $(RM) test.log
	- if exist messages $(RM) messages
	- if exist starttime $(RM) starttime
	@ $(MAKE) -lf Make_mvc.mak MNVPROG=$(MNVPROG) $@.res
	@ type messages
	@ if exist test.log exit 1


# Delete files that may interfere with running tests.  This includes some files
# that may result from working on the tests, not only from running them.
clean:
	- if exist *.out $(RM) *.out
	- if exist *.failed $(RM) *.failed
	- if exist *.res $(RM) *.res
	- if exist $(DOSTMP) $(RD) $(DOSTMP)
	- if exist test.in $(RM) test.in
	- if exist test.ok $(RM) test.ok
	- if exist Xdir1 $(RD) Xdir1
	- if exist Xfind $(RD) Xfind
	- if exist XfakeHOME $(RD) XfakeHOME
	- if exist X* $(RM) X*
	- for /d %i in (X*) do @$(RD) %i
	- if exist mnvinfo $(RM) mnvinfo
	- if exist test.log $(RM) test.log
	- if exist test_result.log $(RM) test_result.log
	- if exist messages $(RM) messages
	- if exist starttime $(RM) starttime
	- if exist benchmark.out $(RM) benchmark.out
	- if exist opt_test.mnv $(RM) opt_test.mnv
	- if exist gen_opt_test.log $(RM) gen_opt_test.log
	- if exist guidialog $(RM) guidialog
	- if exist guidialogfile $(RM) guidialogfile

nolog:
	- if exist test.log $(RM) test.log
	- if exist test_result.log $(RM) test_result.log
	- if exist messages $(RM) messages
	- if exist starttime $(RM) starttime


# Tiny tests.  Works even without the +eval feature.
tinytests: $(SCRIPTS_TINY_OUT)

# Copy the input files to dostmp, changing the fileformat to dos.
$(DOSTMP_INFILES): $(*B).in
	if not exist $(DOSTMP)\NUL $(MKD) $(DOSTMP)
	if exist $@ $(RM) $@
	$(MNVPROG) -u util\dos.mnv $(COMMON_ARGS) "+set ff=dos|f $@|wq" $(*B).in

# For each input file dostmp/test99.in run the tests.
# This moves test99.in to test99.in.bak temporarily.
$(TEST_OUTFILES): $(DOSTMP)\$(*B).in
	-@ if exist test.out $(RM) test.out
	-@ if exist $(DOSTMP)\$(*B).out $(RM) $(DOSTMP)\$(*B).out
	$(MV) $(*B).in $(*B).in.bak > nul
	$(CP) $(DOSTMP)\$(*B).in $(*B).in > nul
	$(CP) $(*B).ok test.ok > nul
	$(MNVPROG) -u util\dos.mnv $(COMMON_ARGS) -s dotest.in $(*B).in
	-@ if exist test.out $(MV) test.out $(DOSTMP)\$(*B).out > nul
	-@ if exist $(*B).in.bak $(MV) $(*B).in.bak $(*B).in > nul
	-@ if exist test.ok $(RM) test.ok
	-@ if exist Xdir1 $(RD) Xdir1
	-@ if exist Xfind $(RD) Xfind
	-@ if exist XfakeHOME $(RD) XfakeHOME
	-@ $(RM) X*
	-@ if exist mnvinfo $(RM) mnvinfo
	$(MNVPROG) -u util\dos.mnv $(COMMON_ARGS) "+set ff=unix|f test.out|wq" \
		$(DOSTMP)\$(*B).out
	@ $(DIFF) test.out $*.ok & if errorlevel 1 \
		( $(MV) test.out $*.failed > nul \
		 & $(RM) $(DOSTMP)\$(*B).out \
		 & echo $* FAILED >> test.log ) \
		else ( $(MV) test.out $*.out > nul )


# New style of tests uses MNV script with assert calls.  These are easier
# to write and a lot easier to read and debug.
# Limitation: Only works with the +eval feature.

newtests: newtestssilent
	@ if exist messages type messages

newtestssilent: $(NEW_TESTS_RES)

.mnv.res:
	@ echo $(MNVPROG) > mnvcmd
	$(MNVPROG) -u NONE $(COMMON_ARGS) -S runtest.mnv $*.mnv
	@ $(RM) mnvcmd

test_gui.res: test_gui.mnv
	@ echo $(MNVPROG) > mnvcmd
	$(MNVPROG) -u NONE $(COMMON_ARGS) -S runtest.mnv $*.mnv
	@ $(RM) mnvcmd

test_gui_init.res: test_gui_init.mnv
	@ echo $(MNVPROG) > mnvcmd
	$(MNVPROG) -u util\gui_preinit.mnv -U util\gui_init.mnv $(NO_PLUGINS) \
		-S runtest.mnv $*.mnv
	@ $(RM) mnvcmd

opt_test.mnv: util/gen_opt_test.mnv ../optiondefs.h \
		../../runtime/doc/options.txt
	$(MNVPROG) -e -s -u NONE $(COMMON_ARGS) --nofork -S $**
	@ if exist gen_opt_test.log ( type gen_opt_test.log & exit /b 1 )

test_bench_regexp.res: test_bench_regexp.mnv
	- if exist benchmark.out $(RM) benchmark.out
	@ echo $(MNVPROG) > mnvcmd
	$(MNVPROG) -u NONE $(COMMON_ARGS) -S runtest.mnv $*.mnv
	@ $(RM) mnvcmd
	@ if exist benchmark.out ( type benchmark.out )

# mnv: set noet sw=8 ts=8 sts=0 wm=0 tw=79 ft=make:
