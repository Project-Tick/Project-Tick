#!/bin/sh
# Shell script to start MNV with less.mnv.
# Read stdin if no arguments were given and stdin was redirected.

if [ $# -eq 0 ] && [ -t 0 ]; then
  echo "$(basename "$0"): No input." 1>&2
  exit
fi

if [ -t 1 ]; then
  [ $# -eq 0 ] && set -- "-"
  [ "$*" != "-" ] && set -- -- "$@"
  exec mnv --cmd 'let no_plugin_maps=1' -c 'runtime! macros/less.mnv' --not-a-term "$@"
else  # Output is not a terminal.
  exec cat -- "$@"
fi
