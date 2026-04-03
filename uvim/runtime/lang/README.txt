Language files for MNV: Translated menus

The contents of each menu file is a sequence of lines with "menutrans"
commands.  Read one of the existing files to get an idea of how this works.

More information in the on-line help:

	:help multilang-menus
	:help :menutrans
	:help 'langmenu'
	:help :language

You can find a couple of helper tools for translating menus on github:
https://github.com/adaext/mnv-menutrans-helper

The "$MNVRUNTIME/menu.mnv" file will search for a menu translation file.  This
depends on the value of the "v:lang" variable.

	"menu_" . v:lang . ".mnv"

When the 'menutrans' option is set, its value will be used instead of v:lang.

The file name is always lower case.  It is the full name as the ":language"
command shows (the LC_MESSAGES value).

For example, to use the Big5 (Taiwan) menus on MS-Windows the $LANG will be

	Chinese(Taiwan)_Taiwan.950

and use the menu translation file:

	$MNVRUNTIME/lang/menu_chinese(taiwan)_taiwan.950.mnv

On Unix you should set $LANG, depending on your shell:

	csh/tcsh:	setenv LANG "zh_TW.Big5"
	sh/bash/ksh:	export LANG="zh_TW.Big5"

and the menu translation file is:

	$MNVRUNTIME/lang/menu_zh_tw.big5.mnv

The menu translation file should set the "did_menu_trans" variable so that MNV
will not load another file.


AUTOMATIC CONVERSION

When MNV was compiled with multi-byte support, conversion between latin1 and
UTF-8 will always be possible.  Other conversions depend on the iconv
library, which is not always available.
For UTF-8 menu files which only use latin1 characters, you can rely on MNV
doing the conversion.  Let the UTF-8 menu file source the latin1 menu file,
and put "scriptencoding latin1" in that one.
Other conversions may not always be available (e.g., between iso-8859-# and
MS-Windows codepages), thus the converted menu file must be available.
