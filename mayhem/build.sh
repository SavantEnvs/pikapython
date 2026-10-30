#!/usr/bin/env bash
#
# mayhem/build.sh — builds the PikaPython core + a trimmed module set into a workspace, then the
# sanitized fuzz harnesses (+ standalone reproducers) and the normal-flags test runner.
#
# PikaScript's C glue (pikascript-api/) is generated from the .pyi files by the pika compiler, a Rust
# program in the tree. The Dockerfile builds it once into $PIKA_RUST_MSC, so this script needs no
# network, no cargo and no $HOME (air-gapped re-run).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
W="$B/ws"
LINUX="$SRC/port/linux/package/pikascript"
RUST_MSC="${PIKA_RUST_MSC:-/opt/toolchains/pikacompiler/rust-msc}"
[ -x "$RUST_MSC" ] || { echo "pika compiler not found at $RUST_MSC (the Dockerfile builds it)" >&2; exit 1; }

rm -rf "$B"
mkdir -p "$W/pikascript-lib" "$B/obj-san" "$B/obj-test"

# 1) Workspace: the core, the .pyi/.py of the modules we bind, and the C sources of their libraries.
cp -r "$SRC/src" "$W/pikascript-core"
rm -f "$W/pikascript-core/CMakeLists.txt" "$W/pikascript-core/PikaObj.pyi"
for f in PikaObj.pyi builtins.pyi PikaStdLib.pyi PikaStdData.pyi PikaStdTask.pyi PikaDebug.pyi \
         _json.pyi json.py re.pyi math.py _math.pyi _struct.pyi struct.py pika_cjson.pyi binascii.pyi; do
  cp "$LINUX/$f" "$W/$f"
done
for d in PikaStdLib json re math struct pika_cjson binascii; do
  cp -r "$LINUX/pikascript-lib/$d" "$W/pikascript-lib/$d"
done
cat > "$W/main.py" <<'PY'
import PikaStdLib
import json
import re
import math
import struct
import pika_cjson
import binascii
PY

# 2) Generate the C glue from the .pyi files.
(cd "$W" && "$RUST_MSC" >/dev/null)
# We start the VM ourselves (newRootObj), so the generated pikaScript.c is not needed.
rm -f "$W/pikascript-api/pikaScript.c"

INC=(-I"$W/pikascript-core" -I"$W/pikascript-api" -I"$W/pikascript-lib/PikaStdLib" -I"$W/pikascript-lib/json"
     -I"$W/pikascript-lib/re" -I"$W/pikascript-lib/pika_cjson")
mapfile -t LIB_SRCS < <(find "$W/pikascript-core" "$W/pikascript-lib" "$W/pikascript-api" -name '*.c' | LC_ALL=C sort)

# 3) Module bytecode (json.py, struct.py, math.py -> pikaModules_py_a), via a small tool built with
#    the project's normal flags.
$CC -w -O1 "${INC[@]}" "${LIB_SRCS[@]}" "$SRC/mayhem/pikamaker.c" "$SRC/mayhem/port_hooks.c" -lm -lpthread -o "$B/pikamaker"
(cd "$W" && "$B/pikamaker" >/dev/null)
ASSET="$W/pikascript-api/__asset_pikaModules_py_a.c"
[ -f "$ASSET" ] || { echo "module bytecode was not generated" >&2; exit 1; }
LIB_SRCS+=("$ASSET")

build_objs() { # <objdir> <flags...>
  local od="$1"; shift
  local i=0 src n
  for src in "${LIB_SRCS[@]}"; do
    n=$i; i=$((i+1))
    "$CC" -w -c "$@" "${INC[@]}" "$src" -o "$od/$n.o" &
    while [ "$(jobs -rp | wc -l)" -ge "$MAYHEM_JOBS" ]; do wait -n || true; done
  done
  wait || true
  [ "$(find "$od" -name '*.o' | wc -l)" -eq "${#LIB_SRCS[@]}" ] || { echo "compile of the PikaPython objects failed in $od" >&2; return 1; }
}

# 4) The sanitized project (DWARF <= 3), as an archive the harnesses link.
# The parser reads sLine[strlen(sLine) - 1] on every empty line (PikaParser.c, "check connection"), so
# UBSan's pointer-overflow fires on every script and masks everything behind it. Object destructors are
# stored as `void (*)(void*)` and called through casts (dataArg.c), which trips the `function` check on
# every VM run. Only those two checks are relaxed; ASan and the rest of UBSan stay halting.
RELAX="-fno-sanitize=pointer-overflow,function"
# Two archives: one with libFuzzer coverage instrumentation for the fuzz binaries (so the fuzzer sees the
# project's edges, not just the harness's), one without it for the run-once standalone reproducers.
mkdir -p "$B/obj-fuzz"
build_objs "$B/obj-fuzz" $SANITIZER_FLAGS $RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link -O1
ar rcs "$B/libpika-fuzz.a" "$B"/obj-fuzz/*.o
build_objs "$B/obj-san" $SANITIZER_FLAGS $RELAX $DEBUG_FLAGS -O1
ar rcs "$B/libpika-san.a" "$B"/obj-san/*.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.c" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS "${INC[@]}" -c "$SRC/mayhem/port_hooks.c" -o "$B/port_hooks.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

# 5) Each harness twice: libFuzzer binary + run-once standalone reproducer.
for h in fuzz_parser fuzz_vm fuzz_json fuzz_cjson fuzz_re fuzz_struct; do
  src="$SRC/mayhem/harnesses/$h.c"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 "${INC[@]}" $LIB_FUZZING_ENGINE "$src" \
      "$B/port_hooks.o" "$B/libpika-fuzz.a" "$B/lsan_off.o" -lm -lpthread -o "$B/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS -O1 "${INC[@]}" "$src" "$B/standalone_main.o" \
      "$B/port_hooks.o" "$B/libpika-san.a" "$B/lsan_off.o" -lm -lpthread -o "$B/$h-standalone"
done

# 6) The test runner, with the project's normal flags (no sanitizers, no fuzz engine).
build_objs "$B/obj-test" -O2 $COVERAGE_FLAGS
$CC -w -O2 $COVERAGE_FLAGS "${INC[@]}" "$SRC/mayhem/pikatest.c" "$SRC/mayhem/port_hooks.c" "$B"/obj-test/*.o -lm -lpthread $COVERAGE_FLAGS -o "$B/pikatest"
