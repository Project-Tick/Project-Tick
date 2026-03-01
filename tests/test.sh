#!/bin/sh

# ps tests
#
# Assumptions:
# - PS_BIN points to the newly built ps
# - TEST_HELPER points to tests/spin_helper.c compiled into out/tests/spin_helper

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

fail() {
    printf "${RED}FAIL: %s${NC}\n" "$1" >&2
    exit 1
}

pass() {
    printf "${GREEN}PASS: %s${NC}\n" "$1"
}

# Compile helper if not already there
HELPER_DIR="$(dirname "$0")"
OUT_DIR="$(dirname "${PS_BIN}")"
HELPER_BIN="${OUT_DIR}/spin_helper"

if [ ! -f "${HELPER_BIN}" ]; then
    cc -O2 "${HELPER_DIR}/spin_helper.c" -o "${HELPER_BIN}" || fail "failed to compile spin_helper"
fi

# 1. Basic sanity: run without arguments
# Should output at least headers and some processes.
${PS_BIN} > /tmp/ps_out || fail "ps failed to run"
grep -q "PID" /tmp/ps_out || fail "ps output lacks PID header"
grep -q "COMMAND" /tmp/ps_out || fail "ps output lacks COMMAND header"
pass "Basic sanity (no args)"

# 2. Process selection: -p
# Start a helper, then find it with ps.
${HELPER_BIN} test-ps-select &
HELPER_PID=$!
sleep 0.1
${PS_BIN} -p ${HELPER_PID} > /tmp/ps_p_out || fail "ps -p failed"
grep -q "${HELPER_PID}" /tmp/ps_p_out || fail "ps -p did not find helper pid ${HELPER_PID}"
kill ${HELPER_PID}
pass "Process selection (-p pid)"

# 3. Format selection: -j
${HELPER_BIN} test-ps-fmt-j &
HELPER_PID=$!
sleep 0.1
${PS_BIN} -j -p ${HELPER_PID} > /tmp/ps_j_out || fail "ps -j failed"
grep -q "USER" /tmp/ps_j_out || fail "ps -j output lacks USER header"
grep -q "PPID" /tmp/ps_j_out || fail "ps -j output lacks PPID header"
grep -q "SID" /tmp/ps_j_out || fail "ps -j output lacks SID header"
kill ${HELPER_PID}
pass "Format selection (-j)"

# 4. Format selection: -u
${HELPER_BIN} test-ps-fmt-u &
HELPER_PID=$!
sleep 0.1
${PS_BIN} -u -p ${HELPER_PID} > /tmp/ps_u_out || fail "ps -u failed"
grep -q "%CPU" /tmp/ps_u_out || fail "ps -u output lacks %CPU header"
grep -q "%MEM" /tmp/ps_u_out || fail "ps -u output lacks %MEM header"
grep -q "VSZ" /tmp/ps_u_out || fail "ps -u output lacks VSZ header"
grep -q "RSS" /tmp/ps_u_out || fail "ps -u output lacks RSS header"
kill ${HELPER_PID}
pass "Format selection (-u)"

# 5. Format selection: -o (custom)
${HELPER_BIN} test-ps-fmt-o &
HELPER_PID=$!
sleep 0.1
${PS_BIN} -o pid,comm,nice -p ${HELPER_PID} > /tmp/ps_o_out || fail "ps -o failed"
grep -q "PID" /tmp/ps_o_out || fail "ps -o output lacks PID header"
grep -q "COMMAND" /tmp/ps_o_out || fail "ps -o output lacks COMMAND header"
grep -q "NI" /tmp/ps_o_out || fail "ps -o output lacks NI header"
# First line is header, second line should contain the PID.
grep -q "${HELPER_PID}" /tmp/ps_o_out || fail "ps -o did not find helper pid"
kill ${HELPER_PID}
pass "Format selection (-o pid,comm,nice)"

# 6. Sorting: -m (by memory) and -r (by cpu)
${PS_BIN} -m -A > /dev/null || fail "ps -m -A failed"
${PS_BIN} -r -A > /dev/null || fail "ps -r -A failed"
pass "Sorting (-m, -r)"

# 7. Negative test: invalid pid
${PS_BIN} -p 999999 > /tmp/ps_neg_out 2>&1
# FreeBSD ps should just print header and exits 1 if no matches found?
# Actually, standard behavior varies. Let's check status.
EXIT_CODE=$?
if [ ${EXIT_CODE} -ne 1 ]; then
    # In some versions, no match means exit 1. In others, exit 0.
    # FreeBSD ps.c: main() calls exit(1) if nkept == 0.
    fail "ps -p invalid_pid should exit 1 (got ${EXIT_CODE})"
fi
pass "Negative test (invalid pid)"

# 8. Negative test: invalid flag
${PS_BIN} -Y > /dev/null 2>&1
if [ $? -eq 0 ]; then
    fail "ps -Y should fail"
fi
pass "Negative test (invalid flag)"

# 9. Width test: -w
${HELPER_BIN} test-ps-width-very-long-name-to-check-truncation &
HELPER_PID=$!
sleep 0.1
# Record output without -w (should truncate or at least be limited)
${PS_BIN} -u -p ${HELPER_PID} > /tmp/ps_w1
# Record output with -ww (no limit)
${PS_BIN} -u -ww -p ${HELPER_PID} > /tmp/ps_w2
# Compare -w2 should be longer or equal to -w1 in terms of line length
LEN1=$(awk '{ print length }' /tmp/ps_w1 | sort -nr | head -1)
LEN2=$(awk '{ print length }' /tmp/ps_w2 | sort -nr | head -1)
if [ "${LEN2}" -lt "${LEN1}" ]; then
    fail "ps -ww output shorter than normal output"
fi
kill ${HELPER_PID}
pass "Width control (-w, -ww)"

# 10. Headers control: -h
# Start 30 helpers safely.
I=1
while [ ${I} -le 30 ]; do
    ${HELPER_BIN} test-ps-h-${I} &
    H_PIDS="${H_PIDS} $!"
    I=$((I+1))
done
sleep 0.1
# Run ps -h -A (repeat headers every 22 lines or so)
${PS_BIN} -h -A | grep "PID" | wc -l > /tmp/ps_h_count
COUNT=$(cat /tmp/ps_h_count)
if [ "${COUNT}" -lt 2 ]; then
    fail "ps -h should repeat headers (got ${COUNT} headers for 30+ processes)"
fi
kill ${H_PIDS}
pass "Header repetition (-h)"

# Clean up
rm -f /tmp/ps_*
printf "${GREEN}All tests passed!${NC}\n"
exit 0
