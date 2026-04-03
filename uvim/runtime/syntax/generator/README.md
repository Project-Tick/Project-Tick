# Generator of MNV script Syntax File

This directory contains a MNV script generator, that will parse the MNV source file and
generate a mnv.mnv syntax file.

Files in this directory were copied from https://github.com/mnv-jp/syntax-mnv-ex/
and included here on Feb, 13th, 2024 for the MNV Project.

- Maintainer: Hirohito Higashi
- License: MNV License

## How to generate

    $ make

This will generate `../mnv.mnv`

## Files

Name                 |Description
---------------------|------------------------------------------------------
`Makefile`           |Makefile to generate ../mnv.mnv
`README.md`          |This file
`gen_syntax_mnv.mnv` |Script to generate mnv.mnv
`update_date.mnv`    |Script to update "Last Change:"
`mnv.mnv.base`       |Template for mnv.mnv
