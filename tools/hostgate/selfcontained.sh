#!/usr/bin/env bash
# Header self-containment check (F-095).
#
# CI compiles with libc++ (NDK); this host uses libstdc++. They differ in what
# they leak transitively: libstdc++'s <cstdint> pulls in std::size_t, libc++'s
# does not. That divergence let SeedCapture.hpp compile here and fail in CI
# with "no type named 'size_t' in namespace 'std'".
#
# This compiles each header FIRST, alone, so a missing include cannot be
# papered over by whatever the .cpp happened to include before it.
set -u
cd "$(dirname "$0")/../.." || exit 1
CXX="${CXX:-g++}"
INC="-Iinclude -Isrc -Itools/hostgate/ndkstub -Itools/hostgate/stubs"
[ -n "${PL_INCLUDE:-}" ] && INC="$INC -I${PL_INCLUDE}"
[ -n "${FMT_INCLUDE:-}" ] && INC="$INC -I${FMT_INCLUDE}"

fail=0
checked=0
for h in $(git ls-files 'include/*.hpp' 'include/**/*.hpp' 'src/**/*.hpp' 'src/*.hpp'); do
    checked=$((checked + 1))
    printf '#include "%s"\nint main(){return 0;}\n' "$PWD/$h" > /tmp/hdrcheck.$$.cpp
    if ! "$CXX" -std=c++20 -fsyntax-only -fno-rtti $INC /tmp/hdrcheck.$$.cpp 2>/tmp/hdrerr.$$; then
        fail=$((fail + 1))
        echo "FAIL (not self-contained): $h"
        grep -E 'error' /tmp/hdrerr.$$ | head -3
    fi
done
rm -f /tmp/hdrcheck.$$.cpp /tmp/hdrerr.$$
echo "headers checked=$checked fail=$fail"
[ "$fail" -eq 0 ]
