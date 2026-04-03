#!/bin/dash
# Issue #17026 (bash highlighting requires space after $())
# https://github.com/Project-Tick/Project-Tick/issues/17026#issuecomment-2774112284

_comp_compgen_split -l -- "$(
    tmux list-commands -F "#{command_list_name}"
    tmux list-commands -F "#{command_list_alias}"
)"

