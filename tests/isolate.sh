# Sourced by the test scripts of a plug-in: runs GIMP (or Blender, Godot)
# isolated from the user's own folders, and checks that none of them
# changed. A copy of gimp-devtools/isolate.sh, kept the same in
# every repository that has one (as tests/isolate.sh), so that the tests
# also run without gimp-devtools.
#
# Needs $src, the repository. Uses gimp-devtools next to it (or
# $GIMP_PLUGIN_DEVTOOLS, or $devtools if set before), and sets $devtools.
#
#   gimp_run [--timeout=SECONDS] [gimp-run.sh options] -- <command...>
#       the command as gimp-devtools/gimp-run.sh runs it (see
#       there): GIMP natively, another app (--app=) in its Flatpak if
#       that is installed, with HOME and the XDG folders in the
#       throwaway $GIMP_RUN_HOME (or --home=) and no GVFS. --timeout ends
#       it after that many seconds (exit code 124). Without
#       gimp-devtools the same is done here, for the options --home=,
#       --env=, --filesystem=, --devel, --app=, --native and --flatpak (in
#       that form, and a -- before the command; --env cannot set XDG_*
#       there)
#   snapshot_take <file>
#       lists the user's folders of GIMP, Blender, Godot, Krita and Tiled
#       (gimp-devtools/snapshot.sh) into the file
#   snapshot_check <file> <prefix>
#       compares them with the listing and prints "<prefix>PASS ..." or
#       "<prefix>FAIL ..." (returns 1), or "<prefix>SKIP ..." without
#       gimp-devtools
#
# Copyright 2026 David
# SPDX-License-Identifier: GPL-3.0-or-later

# ($src is set by the script that sources this one)
# shellcheck disable=SC2154
devtools=${devtools:-${GIMP_PLUGIN_DEVTOOLS:-$src/../gimp-devtools}}

gimp_run () {
    gr_timeout=
    case $1 in --timeout=*) gr_timeout=${1#--timeout=}; shift ;; esac
    if [ -x "$devtools/gimp-run.sh" ]; then
        # (also when GIMP_RUN_HOME is not exported)
        ${gr_timeout:+timeout "$gr_timeout"} \
          "$devtools/gimp-run.sh" ${GIMP_RUN_HOME:+--home="$GIMP_RUN_HOME"} "$@"
        return
    fi
    # without gimp-devtools: the same, in short
    gr_home=${GIMP_RUN_HOME:-} gr_app=org.gimp.GIMP gr_mode=
    for gr_a; do
        case $gr_a in
            --home=*) gr_home=${gr_a#--home=} ;;
            --app=*) gr_app=${gr_a#--app=} ;;
            --native) gr_mode=native ;;
            --flatpak) gr_mode=flatpak ;;
            --) break ;;
        esac
    done
    [ -n "$gr_home" ] || { echo "gimp_run: no GIMP_RUN_HOME" >&2; return 2; }
    mkdir -p "$gr_home" || return 2
    gr_home=$(CDPATH='' cd -- "$gr_home" && pwd) || return 2
    if [ "$gr_app" = org.gimp.GIMP ]; then
        gr_mode=native
    elif [ -z "$gr_mode" ]; then
        gr_mode=native
        flatpak info "$gr_app" >/dev/null 2>&1 && gr_mode=flatpak
    fi
    # the arguments are rebuilt behind the old ones, which are shifted away
    gr_n=$#
    gr_opts=1
    [ -n "$gr_timeout" ] && set -- "$@" timeout "$gr_timeout"
    if [ "$gr_mode" = native ]; then
        set -- "$@" env GIO_USE_VFS=local "HOME=$gr_home" "XDG_CONFIG_HOME=$gr_home/.config" \
          "XDG_DATA_HOME=$gr_home/.local/share" "XDG_CACHE_HOME=$gr_home/.cache" \
          "XDG_STATE_HOME=$gr_home/.local/state"
    else
        # (flatpak run itself too, which keeps a cache in ~/.var/app/<app>)
        set -- "$@" env "HOME=$gr_home" "XDG_CONFIG_HOME=$gr_home/.config" \
          "XDG_DATA_HOME=$gr_home/.local/share" "XDG_CACHE_HOME=$gr_home/.cache" \
          "XDG_STATE_HOME=$gr_home/.local/state" \
          "FLATPAK_USER_DIR=${FLATPAK_USER_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/flatpak}" \
          flatpak run --no-documents-portal "--filesystem=$gr_home"
    fi
    while [ "$gr_n" -gt 0 ]; do
        gr_a=$1
        shift
        gr_n=$((gr_n - 1))
        if [ -z "$gr_opts" ]; then
            set -- "$@" "$gr_a"
            continue
        fi
        case $gr_a in
            --)
                gr_opts=
                # the Flatpak sets XDG_* itself: they are set inside
                # shellcheck disable=SC2016
                [ "$gr_mode" = native ] || set -- "$@" --command=sh "$gr_app" -c \
                  'export GIO_USE_VFS=local HOME="$0" XDG_CONFIG_HOME="$0/.config" XDG_DATA_HOME="$0/.local/share" XDG_CACHE_HOME="$0/.cache" XDG_STATE_HOME="$0/.local/state"; exec "$@"' \
                  "$gr_home" ;;
            --env=*)
                if [ "$gr_mode" = native ]; then
                    set -- "$@" "${gr_a#--env=}"
                else
                    set -- "$@" "$gr_a"
                fi ;;
            --filesystem=*|--devel) [ "$gr_mode" = native ] || set -- "$@" "$gr_a" ;;
        esac
    done
    "$@"
}

snapshot_take () {
    if [ -x "$devtools/snapshot.sh" ]; then
        "$devtools/snapshot.sh" >"$1"
    else
        rm -f "$1"
    fi
}

snapshot_check () {
    if [ ! -f "$1" ]; then
        echo "${2}SKIP your folders of GIMP and the other apps not checked (no $devtools/snapshot.sh)"
        return 0
    fi
    if "$devtools/snapshot.sh" --compare "$1" >"$1.diff"; then
        echo "${2}PASS nothing changed in your folders of GIMP, Blender, Godot, Krita and Tiled ($(wc -l <"$1") entries)"
    else
        echo "${2}FAIL your folders of GIMP or the other apps changed (see $1.diff):"
        head -20 "$1.diff"
        return 1
    fi
}
