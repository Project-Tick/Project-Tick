#!/bin/sh

if [ "${CGIT_TEST_NO_GIT_VERSION}" = "YesPlease" ]; then
	exit 0
fi

test_description='Check Git version is correct'
CGIT_TEST_NO_CREATE_REPOS=YesPlease
. ./setup.sh

test_expect_success 'extract Git version from Makefile' '
	sed -n -e "/^GIT_VER[ 	]*=/ {
		s/^GIT_VER[ 	]*=[ 	]*//
		p
	}" ../../Makefile >makefile_version &&
	makefile_ver=$(cat makefile_version) &&
	if (
		cd ../../git &&
		git rev-parse --verify -q "$makefile_ver^{commit}" >/dev/null
	)
	then
		(
			cd ../../git &&
			git rev-parse "$makefile_ver^{commit}"
		) >makefile_git_oid &&
		(
			cd ../../git &&
			git describe --match "v[0-9]*" "$makefile_ver"
		) | sed -e "s/^v//" -e "s/-/./g" >makefile_git_version
	else
		sed -e "s/-/./g" makefile_version >makefile_git_version
	fi
'

# Note that Git's GIT-VERSION-GEN script applies "s/-/./g" to the version
# string to produce the internal version in the GIT-VERSION-FILE. The cgit
# Makefile may carry either such a version string or an exact git commit-ish,
# so normalize to the internal version form before comparing them.
test_expect_success 'test Git version matches Makefile' '
	( cat ../../git/GIT-VERSION-FILE || echo "No GIT-VERSION-FILE" ) |
	sed -e "s/GIT_VERSION[ 	]*=[ 	]*//" -e "s/\\.dirty$//" >git_version &&
	test_cmp git_version makefile_git_version
'

test_expect_success 'test submodule version matches Makefile' '
	if ! test -e ../../git/.git
	then
		echo "git/ is not a Git repository" >&2
	else
		if test -f makefile_git_oid
		then
			(
				cd ../.. &&
				git ls-files --stage -- git |
				sed -n -e "s/^[0-9]* \\([0-9a-f]*\\) [0-9]	.*$/\\1/p"
			) >sm_oid &&
			test_cmp sm_oid makefile_git_oid
		else
			(
				cd ../.. &&
				sm_oid=$(git ls-files --stage -- git |
					sed -e "s/^[0-9]* \\([0-9a-f]*\\) [0-9]	.*$/\\1/") &&
				cd git &&
				git describe --match "v[0-9]*" $sm_oid
			) | sed -e "s/^v//" -e "s/-/./g" >sm_version &&
			test_cmp sm_version makefile_version
		fi
	fi
'

test_done
