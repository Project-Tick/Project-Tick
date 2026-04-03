@echo off
rem batch file to start MNV with less.mnv.
rem Read stdin if no arguments were given.
rem Written by Ken Takata.

if "%1"=="" (
  mnv --cmd "let no_plugin_maps = 1" -c "runtime! macros/less.mnv" -
) else (
  mnv --cmd "let no_plugin_maps = 1" -c "runtime! macros/less.mnv" %*
)
