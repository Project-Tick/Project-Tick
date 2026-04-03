#
# Makefile to run all tests for MNV
#

# Use console or GUI.
MNVPROG = ../mnv
XXDPROG = ../xxd/xxd
# MNVPROG = ../gmnv

SCRIPTSOURCE = ../../runtime

# Comment out this line to see the verbose output of tests.
#
# Catches SwapExists to avoid hanging at the ATTENTION prompt.
REDIR_TEST_TO_NULL = --cmd 'au SwapExists * let v:swapchoice = "e"' | LC_ALL=C awk '/Executing Test_/{match($$0, "([0-9][0-9]:[0-9][0-9] *)?Executing Test_[^\\)]*\\)"); print substr($$0, RSTART, RLENGTH) "\r"; fflush()}'

# Uncomment this line to use valgrind for memory leaks and extra warnings.
#   The output goes into a file "valgrind.testN"
#   MNV should be compiled with EXITFREE to avoid false warnings.
#   This will make testing about 10 times as slow.
# VALGRIND = valgrind --tool=memcheck --leak-check=yes --num-callers=45 --log-file=valgrind.$*

# To execute one test, e.g. in gdb, use something like this:
# run -f -u util/unix.mnv --gui-dialog-file guidialog -U NONE --noplugin --not-a-term -S runtest.mnv test_name.mnv

default: nongui

# The list of tests is common to all systems.
# This defines SCRIPTS_TINY_OUT, NEW_TESTS and NEW_TESTS_RES.
include Make_all.mak

# Explicit dependencies.
test_options_all.res: opt_test.mnv

.SUFFIXES: .in .out .res .mnv

nongui:	nolog tinytests newtests report

gui:	nolog tinytests newtests report

tiny:	nolog tinytests report

benchmark: $(SCRIPTS_BENCH)

report:
	@# without the +eval feature test_result.log is a copy of test.log
	@/bin/sh -c "if test -f test.log; \
		then cp test.log test_result.log; \
		else echo No failures reported > test_result.log; \
		fi"
	$(MNVPROG) -u NONE $(NO_INITS) -S util/summarize.mnv messages $(REDIR_TEST_TO_NULL)
	@rm -f starttime
	@echo
	@echo 'Test results:'
	@cat test_result.log
	@/bin/sh -c "if test -f test.log; \
		then echo TEST FAILURE; exit 1; \
		else echo ALL DONE; \
		fi"

$(SCRIPTS_TINY_OUT) $(NEW_TESTS_RES): $(MNVPROG)

# For $ASAN_OPTIONS and $UBSAN_OPTIONS append the testname to it.
# This assumes $ASAN_OPTIONS contains log_path as last part of the environment variable
# For Github CI, those variables are set in .github/workflows/ci.yml

# Execute an individual new style test, e.g.:
# 	make test_largefile
$(NEW_TESTS):
	rm -f $@.res $(CLEANUP_FILES)
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile $@.res MNVPROG=$(MNVPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@cat messages
	@if test -f test.log; then \
		exit 1; \
	fi

codestyle:
	rm -f test_codestyle.res $(CLEANUP_FILES)
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile test_codestyle.res MNVPROG=$(MNVPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@cat messages
	@if test -f test.log; then \
		exit 1; \
	fi

# Run only tests specific for MNV9 script
test_mnv9:
	rm -f test_mnv9_*.res $(CLEANUP_FILES)
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile $(TEST_MNV9_RES) MNVPROG=$(MNVPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@cat messages
	@rm -f starttime
	@MAKEFLAGS=--no-print-directory $(MAKE) -f Makefile report MNVPROG=$(MNVPROG) XXDPROG=$(XXDPROG) SCRIPTSOURCE=$(SCRIPTSOURCE)
	@if test -f test.log; then \
		exit 1; \
	fi

RM_ON_RUN = test.out X* mnvinfo
RM_ON_START = test.ok benchmark.out
RUN_MNVPROG = MNVRUNTIME=$(SCRIPTSOURCE) $(VALGRIND) $(MNVPROG) -f $(GUI_FLAG) -u util/unix.mnv $(NO_INITS) -s dotest.in

# Delete files that may interfere with running tests.  This includes some files
# that may result from working on the tests, not only from running them.
clean:
	-rm -rf *.out *.failed *.res *.rej *.orig XfakeHOME Xdir1 Xfind failed
	-rm -f opt_test.mnv test_result.log $(CLEANUP_FILES)
	-rm -f gen_opt_test.log
	-rm -rf $(RM_ON_RUN) $(RM_ON_START)
	-rm -f valgrind.*
	-rm -f asan.* asan_test_*
	-rm -f guidialog guidialogfile

# Delete the files produced by benchmarking, so they can run again.
benchmarkclean:
	rm -f $(SCRIPTS_BENCH)

nolog:
	-rm -f test_result.log $(CLEANUP_FILES)


# Tiny tests.  Works even without the +eval feature.
tinytests: $(SCRIPTS_TINY_OUT)

.in.out:
	-rm -rf $*.failed test.ok $(RM_ON_RUN)
	cp $*.ok test.ok
	@# Sleep a moment to avoid that the xterm title is messed up.
	@# 200 msec is sufficient, but only modern sleep supports a fraction of
	@# a second, fall back to a second if it fails.
	@-/bin/sh -c "sleep .2 > /dev/null 2>&1 || sleep 1"
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVPROG) $*.in $(REDIR_TEST_TO_NULL) ; \
	else \
		$(RUN_MNVPROG) $*.in $(REDIR_TEST_TO_NULL) ; \
	fi

	@# Check if the test.out file matches test.ok.
	@/bin/sh -c "if test -f test.out; then \
		  if diff test.out $*.ok; \
		  then mv -f test.out $*.out; \
		  else echo $* FAILED >>test.log; mv -f test.out $*.failed; \
		  fi \
		else echo $* NO OUTPUT >>test.log; \
		fi"
	@/bin/sh -c "if test -f valgrind; then\
		  mv -f valgrind valgrind.$*; \
		fi"
	-rm -rf X* test.ok mnvinfo


# New style of tests uses MNV script with assert calls.  These are easier
# to write and a lot easier to read and debug.
# Limitation: Only works with the +eval feature.
# Add --gui-dialog-file to avoid getting stuck in a dialog.
RUN_MNVTEST = MNVRUNTIME=$(SCRIPTSOURCE) $(VALGRIND) $(MNVPROG) -f $(GUI_FLAG) -u util/unix.mnv --gui-dialog-file guidialog

newtests: newtestssilent
	@/bin/sh -c "if test -f messages; then cat messages; fi"

newtestssilent: $(NEW_TESTS_RES)


.mnv.res:
	@echo "$(MNVPROG)" > mnvcmd
	@echo "$(RUN_MNVTEST)" >> mnvcmd
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv $*.mnv $(REDIR_TEST_TO_NULL) ; \
	else \
		$(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv $*.mnv $(REDIR_TEST_TO_NULL) ; \
	fi
	@rm mnvcmd

test_gui.res: test_gui.mnv
	@echo "$(MNVPROG)" > mnvcmd
	@echo "$(RUN_GMNVTEST)" >> mnvcmd
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVTEST) -u NONE $(NO_INITS) -S runtest.mnv $< ; \
	else \
		$(RUN_MNVTEST) -u NONE $(NO_INITS) -S runtest.mnv $< ; \
	fi

	@rm mnvcmd

test_gui_init.res: test_gui_init.mnv
	@echo "$(MNVPROG)" > mnvcmd
	@echo "$(RUN_GMNVTEST_WITH_GMNVRC)" >> mnvcmd
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVTEST) -u util/gui_preinit.mnv -U util/gui_init.mnv $(NO_PLUGINS) -S runtest.mnv $< ; \
	else \
		$(RUN_MNVTEST) -u util/gui_preinit.mnv -U util/gui_init.mnv $(NO_PLUGINS) -S runtest.mnv $< ; \
	fi
	@rm mnvcmd

GEN_OPT_DEPS = util/gen_opt_test.mnv ../optiondefs.h ../../runtime/doc/options.txt

opt_test.mnv: $(GEN_OPT_DEPS)
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(MNVPROG) -e -s -u NONE $(NO_INITS) --nofork --gui-dialog-file guidialog -S $(GEN_OPT_DEPS) ; \
	else \
		$(MNVPROG) -e -s -u NONE $(NO_INITS) --nofork --gui-dialog-file guidialog -S $(GEN_OPT_DEPS) ; \
	fi
	@if test -f gen_opt_test.log; then \
		cat gen_opt_test.log; \
		exit 1; \
	fi

test_xxd.res:
	if test -n "$${ASAN_OPTIONS}"; then \
		XXD=$(XXDPROG); export XXD; ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv test_xxd.mnv ; \
	else \
		XXD=$(XXDPROG); export XXD; $(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv test_xxd.mnv ; \
	fi

test_bench_regexp.res: test_bench_regexp.mnv
	-rm -rf benchmark.out $(RM_ON_RUN)
	@# Sleep a moment to avoid that the xterm title is messed up.
	@# 200 msec is sufficient, but only modern sleep supports a fraction of
	@# a second, fall back to a second if it fails.
	@-/bin/sh -c "sleep .2 > /dev/null 2>&1 || sleep 1"
	if test -n "$${ASAN_OPTIONS}"; then \
		ASAN_OPTIONS="$${ASAN_OPTIONS}_$*" UBSAN_OPTIONS="$${UBSAN_OPTIONS}_$*" $(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv $*.mnv $(REDIR_TEST_TO_NULL) ; \
	else \
		$(RUN_MNVTEST) $(NO_INITS) -S runtest.mnv $*.mnv $(REDIR_TEST_TO_NULL) ; \
	fi
	@/bin/sh -c "if test -f benchmark.out; then cat benchmark.out; fi"
