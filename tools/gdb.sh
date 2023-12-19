#!/bin/bash

ROOT=$(realpath "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../")
gdb-multiarch -ix ${ROOT}/tools/gdbinit $@

