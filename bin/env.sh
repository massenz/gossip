#!/bin/bash

set -ex

# Prints the absolute path of the file given as $1
#
function abspath {
    echo $(python -c "import os; print(os.path.abspath(\"$1\"))")
}

BASEDIR="$(abspath $(dirname $0)/..)"
BUILDDIR="${BASEDIR}/build"
CLANG=$(which clang++)

if [[ -d ${COMMON_UTILS_DIR} ]]; then
  UTILS="-DCOMMON_UTILS_DIR=${COMMON_UTILS_DIR}"
fi

DYLD_LIBRARY_PATH="${BUILDDIR}/lib":${DYLD_LIBRARY_PATH}
