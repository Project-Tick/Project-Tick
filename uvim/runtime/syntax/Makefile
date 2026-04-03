# Portable Makefile for running syntax tests.

# To run the test manually:
# ../../src/mnv -u 'testdir/runtest.mnv' --cmd 'breakadd func RunTest'

# Override this if needed, the default assumes MNV was built in the src dir.
#MNVPROG = mnv
MNVPROG = ../../src/mnv

# "runtime" relative to "runtime/syntax/testdir"
MNVRUNTIME = ../..

# Uncomment this line to use valgrind for memory leaks and extra warnings.
# VALGRIND = valgrind --tool=memcheck --leak-check=yes --num-callers=45 --log-file=valgrind.$*

# Trace ruler liveness on demand.
# MNV_SYNTAX_TEST_LOG = `pwd`/testdir/failed/00-TRACE_LOG

# ENVVARS = LC_ALL=C MNV_SYNTAX_TEST_LOG="$(MNV_SYNTAX_TEST_LOG)"
# ENVVARS = LC_ALL=C LANG=C LANGUAGE=C
# Run the syntax tests with a C locale
ENVVARS = LC_ALL=C

RUN_MNVTEST = MNVRUNTIME=$(MNVRUNTIME) $(VALGRIND) $(ENVVARS) ../$(MNVPROG) -f $(GUI_FLAG)

# Uncomment this line for debugging
# DEBUGLOG = --log testlog

# All initial phony targets; these names may clash with file types.
phonies = clean executables test testclean
.PHONY: $(phonies)

# Run the tests that didn't run yet or failed previously.
# If a test succeeds a testdir/done/{name} file will be written.
# If a test fails a testdir/failed/{name}.dump file will be written.
# Progress and error messages can be found in "testdir/messages".
# Comment out the "> /dev/null" part to see the in-progress MNV behaviors.
test:
	@# the "mnvcmd" file is used by the screendump utils
	@echo "../$(MNVPROG)" > testdir/mnvcmd
	@echo "$(RUN_MNVTEST)" >> testdir/mnvcmd
	@# Trace ruler liveness on demand.
	@#mkdir -p testdir/failed
	@#touch "$(MNV_SYNTAX_TEST_LOG)"
	MNVRUNTIME=$(MNVRUNTIME) $(ENVVARS) $(MNVPROG) --clean --not-a-term $(DEBUGLOG) -u testdir/runtest.mnv > /dev/null
	@# FIXME: Temporarily show the whole file to find out what goes wrong
	@#if [ -f testdir/messages ]; then tail -n 6 testdir/messages; fi
	@if [ -f testdir/messages ]; then cat testdir/messages; fi

# add dependence on syntax files
testdir/testdeps.mk:
	./testdir/tools/maketestdeps $(phonies) > $@

-include testdir/testdeps.mk

executables:
	@! find testdir/input -type f -perm /a+x -exec echo Executable syntax test found: \{\} \; | grep :

clean testclean:
	rm -f testdir/failed/* testdir/done/* testdir/mnvcmd testdir/messages testdir/Xtestscript testdir/Xfilter testdir/testdeps.mk

# Collect all input filenames and their file types.
testnames != set +f; \
awk 'BEGIN { \
    for (i = 1; i < ARGC; i++) { \
        split(ARGV[i], names, /\//); \
        split(names[3], parts, /[._]/); \
        types[parts[1]]; \
        print names[3]; \
    } \
    delete types["mnv9"]; \
    split("$(phonies)", scratch); \
    for (phony in scratch) \
        phonies[scratch[phony]]; \
    for (type in types) \
        print type ((type in phonies) ? "_" : ""); \
}' testdir/input/*.*

.PHONY: self-testing $(testnames)

$(testnames)::
	@echo $@ >> testdir/Xfilter

self-testing:: $(testnames)
	@echo self-testing > testdir/Xfilter
