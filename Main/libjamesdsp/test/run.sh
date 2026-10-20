#!/bin/bash
# Host test for accumulate mode. Builds the library at a base commit and at HEAD, then compares them.
# Usage: Main/libjamesdsp/test/run.sh [base commit, default c608c8c]
set -e
BASE=${1:-c608c8c}
ROOT=$(git rev-parse --show-toplevel)
T=$(mktemp -d)
trap 'git -C "$ROOT" worktree remove --force "$T/base" >/dev/null 2>&1; rm -rf "$T"' EXIT
build() { # <jni/jamesdsp dir> <out.so>
	(cd "$1" && gcc -O2 -fpermissive -fPIC -shared -w -include stdint.h -include stdlib.h -include stddef.h -include unistd.h \
		-DJAMESDSP_REFERENCE_IMPL -o "$2" $(sed -n '/LOCAL_SRC_FILES/,/terminator/p' Android.mk | grep -oE '[A-Za-z0-9_/.-]+\.c') -lm -lpthread)
}
git -C "$ROOT" worktree add -q --detach "$T/base" "$BASE"
build "$T/base/Main/libjamesdsp/jni/jamesdsp" "$T/base.so"
build "$ROOT/Main/libjamesdsp/jni/jamesdsp" "$T/head.so"
gcc -O2 -w -I "$ROOT/Main/libjamesdsp/jni/jamesdsp" -o "$T/accum_test" "$ROOT/Main/libjamesdsp/test/accum_test.c" -ldl -lm
"$T/accum_test" "$T/base.so" "$T/head.so"
