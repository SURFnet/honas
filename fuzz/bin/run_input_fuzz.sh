#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")"; pwd)"
cd "$SCRIPT_DIR/../.."

INPUT_MODULE="$1"
if [ -z "$INPUT_MODULE" ]; then
	echo "Usage: $(basename "$0") <input-module-name>" >&2
	exit 1
elif [ ! -d "fuzz/input/$INPUT_MODULE/testcases" ]; then
	echo "Unknown input module '$INPUT_MODULE' or no initial test cases prepared!" >&2
	exit 1
fi
shift
mkdir -p "fuzz/input/$INPUT_MODULE/findings"

# Make sure 'input_fuzz' program is build and up to date
[ -d fuzz/build ] || CC=afl-gcc meson fuzz/build
ninja -C fuzz/build input_fuzz

exec afl-fuzz -i "fuzz/input/$INPUT_MODULE/testcases/" -o "fuzz/input/$INPUT_MODULE/findings/" "$@" -- "fuzz/build/input_fuzz" "$INPUT_MODULE"
