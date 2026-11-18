#!/bin/sh
# Builds the plug-in into tests/output and runs tests/cases.py inside GIMP
# without a window. GIMP uses a throwaway profile in tests/output/profile
# (GIMP3_DIRECTORY) whose plug-in path adds the freshly built plug-in, so
# the installed plug-ins and the user's settings are not touched. GIMP
# and the build run isolated from the user's folders (tests/isolate.sh,
# with gimp-devtools/gimp-run.sh if it is there): HOME and the XDG
# folders point into tests/output/gimp-home. With gimp-devtools, a
# listing of the user's folders of GIMP and the other apps (its
# snapshot.sh) before and after checks that nothing there changed.
#
# GIMP is the native GIMP 3 on the PATH, with its development files
# (libgimp-3.0-dev); with gimp-devtools next to this repo, its gimp-env.sh
# finds it and checks the version. GIMP_CONSOLE sets the gimp-console to
# use, e.g. GIMP_CONSOLE=gimp-console-3.2.
#
# Extra meson options can be given, e.g. for a sanitizer build:
#   tests/run.sh -Db_sanitize=address,undefined
# WT_ONLY=<text> runs only the cases whose name contains the text.
#
# Prints PASS or FAIL for each case and exits non-zero on any failure.
set -e
here=$(cd "$(dirname "$0")" && pwd)
top=$(dirname "$here")
out="$here/output"
devtools="$top/../gimp-devtools"
console=${GIMP_CONSOLE:-}
GIMP_SERIES=
if [ -f "$devtools/gimp-env.sh" ]; then
  . "$devtools/gimp-env.sh"
  [ -z "$GIMP_ENV_ERROR" ] || { echo "$0: $GIMP_ENV_ERROR" >&2; exit 1; }
fi
# gimp-console-3.2, or gimp-console if there is no such command
if [ -z "$console" ]; then
  console=gimp-console
  if [ -n "$GIMP_SERIES" ] && command -v "gimp-console-$GIMP_SERIES" > /dev/null; then
    console=gimp-console-$GIMP_SERIES
  fi
fi

# runs a build command in the source folder
build () {
  (cd "$top" && "$@")
}

rm -rf "$out"; mkdir -p "$out/profile"
src=$top
GIMP_RUN_HOME=$out/gimp-home
export GIMP_RUN_HOME
# shellcheck source=SCRIPTDIR/isolate.sh
. "$here/isolate.sh"
snapshot_take "$out/snapshot-before.txt"
build meson setup "$out/build" -Dplugindir="$out/plug-ins" "$@" > "$out/build.log" 2>&1 \
  || { cat "$out/build.log"; exit 1; }
build ninja -C "$out/build" install >> "$out/build.log" 2>&1 \
  || { cat "$out/build.log"; exit 1; }
printf '(plug-in-path "${gimp_dir}/plug-ins:${gimp_plug_in_dir}/plug-ins:%s")\n' "$out/plug-ins" > "$out/profile/gimprc"

# the environment of GIMP; the cases check the Polish translation
env="GIMP3_DIRECTORY=$out/profile WT_ONLY=$WT_ONLY LANGUAGE=pl"
# a sanitizer build writes the reports of the plug-in to
# tests/output/sanitizer.<pid>
sanitize=
case "$*" in
  *b_sanitize=*)
    sanitize=1
    env="$env ASAN_OPTIONS=log_path=$out/sanitizer:detect_leaks=1"
    env="$env UBSAN_OPTIONS=log_path=$out/sanitizer:print_stacktrace=1" ;;
esac
# shellcheck disable=SC2046 # one --env= per word of $env
set -- gimp_run $(for e in $env; do echo "--env=$e"; done) -- "$console"
set +e
"$@" --no-interface --no-data \
  --batch-interpreter python-fu-eval -b "exec(open('$here/cases.py').read())" --quit \
  > "$out/gimp.log" 2>&1
set -e
grep -E '^(PASS|FAIL|SKIP|RESULT)|Traceback|Error' "$out/gimp.log"
result=0
grep -q '^RESULT [0-9]* passed, 0 failed' "$out/gimp.log" || result=1
if [ -n "$sanitize" ]; then
  python3 "$here/sanitizer.py" "$out" || result=1
fi
snapshot_check "$out/snapshot-before.txt" "" || result=1
exit $result
