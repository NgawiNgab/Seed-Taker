#!/usr/bin/env bash
# Host syntax gate (F-056).
#
# Compiles every TU against the REAL preloader headers when they are available
# (PL_INCLUDE points at a preloader-android checkout), falling back to the
# local stubs otherwise. This matters: the stubs mirror signatures from
# memory, and a stub that is *more permissive* than the real header hides
# exactly the errors CI then reports (that is how run 20 failed while this
# gate was green).
#
# NOT a substitute for the NDK build: no codegen, host libc is not bionic.
set -u
cd "$(dirname "$0")/../.." || exit 1
CXX="${CXX:-g++}"

INC="-Iinclude -Isrc"
if [ -n "${PL_INCLUDE:-}" ] && [ -d "${PL_INCLUDE}" ]; then
    INC="$INC -I${PL_INCLUDE}"
    MODE="real preloader headers (${PL_INCLUDE})"
else
    INC="$INC -Itools/hostgate/ndkstub -Itools/hostgate/stubs"
# fmt is a real dependency of the preloader headers; point FMT_INCLUDE at a
# fmt checkout when validating against real headers.
if [ -n "${FMT_INCLUDE:-}" ] && [ -d "${FMT_INCLUDE}" ]; then
    INC="$INC -I${FMT_INCLUDE}"
fi
    MODE="STUBS (approximate; set PL_INCLUDE=/path/to/preloader-android/include for the real thing)"
fi

# The real headers pull the actual EGL/pl ABI; the EGL stub is still needed
# on a host without the NDK sysroot.
INC="$INC -Itools/hostgate/ndkstub -Itools/hostgate/stubs"
# fmt is a real dependency of the preloader headers; point FMT_INCLUDE at a
# fmt checkout when validating against real headers.
if [ -n "${FMT_INCLUDE:-}" ] && [ -d "${FMT_INCLUDE}" ]; then
    INC="$INC -I${FMT_INCLUDE}"
fi

echo "host gate: $MODE"
fail=0
pass=0
for f in $(find src -name '*.cpp' | sort); do
    if "$CXX" -std=c++20 -fsyntax-only -fno-rtti $INC "$f" 2>/tmp/hostgate_err.$$; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $f"
        grep -E 'error' /tmp/hostgate_err.$$ | head -8
    fi
done
rm -f /tmp/hostgate_err.$$
echo "PASS=$pass FAIL=$fail"
[ "$fail" -eq 0 ]
