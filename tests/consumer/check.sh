#!/bin/sh
# Builds the project in this folder against this checkout the way the User Guide says a project uses rocketplot, and
# checks what such a project relies on. CI runs it in the Linux asan job; to run it by hand (GCC or Clang, Ninja, Qt):
#   tests/consumer/check.sh [build folder, default build/consumer]
# 1. With its own -Werror, AddressSanitizer and UBSan, and ROCKETPLOT_SANITIZERS set to match: it runs clean, and
#    `cmake --install` puts rocketplot's licence files in share/doc/rocketplot.
# 2. With ROCKETPLOT_BUNDLE_MATH_FONT=OFF: it runs (math shown as text), and no font licence is installed.
set -eu

source=$(cd "$(dirname "$0")" && pwd)
build=${1:-build/consumer}
export QT_QPA_PLATFORM=offscreen
export LSAN_OPTIONS="suppressions=$source/../lsan.supp"
doc=share/doc/rocketplot

cmake -S "$source" -B "$build/sanitized" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCONSUMER_SANITIZE=ON
cmake --build "$build/sanitized"
"$build/sanitized/consumer" "$build/sanitized"
rm -rf "$build/sanitized-install"
cmake --install "$build/sanitized" --prefix "$build/sanitized-install"
for file in LICENSE latinmodern-math/GUST-FONT-LICENSE.txt latinmodern-math/README-latinmodern-math.txt; do
    if [ ! -f "$build/sanitized-install/$doc/$file" ]; then
        echo "check.sh: $doc/$file was not installed" >&2
        exit 1
    fi
done

cmake -S "$source" -B "$build/no-math-font" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DROCKETPLOT_BUNDLE_MATH_FONT=OFF
cmake --build "$build/no-math-font"
"$build/no-math-font/consumer" "$build/no-math-font"
rm -rf "$build/no-math-font-install"
cmake --install "$build/no-math-font" --prefix "$build/no-math-font-install"
if [ ! -f "$build/no-math-font-install/$doc/LICENSE" ] || [ -e "$build/no-math-font-install/$doc/latinmodern-math" ]; then
    echo "check.sh: without the math font, $doc should hold LICENSE alone" >&2
    exit 1
fi
echo "check.sh: all checks passed"
