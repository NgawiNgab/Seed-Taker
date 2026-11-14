#!/usr/bin/env bash
# Host syntax gate (F-056): every TU must pass -fsyntax-only against the
# stubs in tools/hostgate/stubs. NOT a substitute for the NDK build — no
# codegen, host libc is not bionic. CI remains the real compile gate.
set -u
cd "$(dirname "$0")/../.." || exit 1
CXX="${CXX:-g++}"
INC="-Iinclude -Isrc -Itools/hostgate/stubs"
fail=0
pass=0
for f in $(find src -name '*.cpp' | sort); do
    if "$CXX" -std=c++20 -fsyntax-only -fno-rtti $INC "$f" 2>/tmp/hostgate_err.$$; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $f"
        head -15 /tmp/hostgate_err.$$
    fi
done
rm -f /tmp/hostgate_err.$$
echo "PASS=$pass FAIL=$fail"
[ "$fail" -eq 0 ]
