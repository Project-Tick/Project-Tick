#! /bin/sh
# installman.sh --- install or uninstall manpages for MNV
#
# arguments:
# 1  what: "install", "uninstall" or "xxd"
# 2  target directory			   e.g., "/usr/local/man/it/man1"
# 3  language addition			   e.g., "" or "-it"
# 4  mnv location as used in manual pages  e.g., "/usr/local/share/mnv"
# 5  runtime dir for menu.mnv et al.	   e.g., "/usr/local/share/mnv/mnv81"
# 6  runtime dir for global mnvrc file	   e.g., "/usr/local/share/mnv"
# 7  source dir for help files		   e.g., "../runtime/doc"
# 8  mode bits for manpages		   e.g., "644"
# 9  mnv exe name			   e.g., "mnv"
# 10 name of mnvdiff exe		   e.g., "mnvdiff"
# 11 name of emnv exe			   e.g., "emnv"

errstatus=0

what=$1
destdir=$2
langadd=$3
mnvloc=$4
scriptloc=$5
mnvrcloc=$6
helpsource=$7
manmod=$8
exename=$9
# older shells don't support ${10}
shift
mnvdiffname=$9
shift
emnvname=$9

helpsubloc=$scriptloc/doc
printsubloc=$scriptloc/print
synsubloc=$scriptloc/syntax
tutorsubloc=$scriptloc/tutor

if test $what = "install" -o $what = "xxd"; then
   if test ! -d $destdir; then
      echo creating $destdir
      /bin/sh install-sh -c -d $destdir
      chmod 755 $destdir
   fi
fi

# Note: setting LC_ALL to C is required to avoid illegal byte errors from sed
# on some systems.

if test $what = "install"; then
   # mnv.1
   if test -r $helpsource/mnv$langadd.1; then
      echo installing $destdir/$exename.1
      LC_ALL=C sed -e s+/usr/local/lib/mnv+$mnvloc+ \
	      -e s+'/usr/local/share/mnv/mnv??'+$mnvloc+ \
	      -e s+/usr/local/share/mnv+$mnvloc+ \
	      -e s+$mnvloc/doc+$helpsubloc+ \
	      -e s+$mnvloc/print+$printsubloc+ \
	      -e s+$mnvloc/syntax+$synsubloc+ \
	      -e s+$mnvloc/tutor+$tutorsubloc+ \
	      -e s+$mnvloc/mnvrc+$mnvrcloc/mnvrc+ \
	      -e s+$mnvloc/gmnvrc+$mnvrcloc/gmnvrc+ \
	      -e s+$mnvloc/menu.mnv+$scriptloc/menu.mnv+ \
	      -e s+$mnvloc/bugreport.mnv+$scriptloc/bugreport.mnv+ \
	      -e s+$mnvloc/filetype.mnv+$scriptloc/filetype.mnv+ \
	      -e s+$mnvloc/scripts.mnv+$scriptloc/scripts.mnv+ \
	      -e s+$mnvloc/optwin.mnv+$scriptloc/optwin.mnv+ \
	      $helpsource/mnv$langadd.1 > $destdir/$exename.1
      chmod $manmod $destdir/$exename.1
   fi

   # mnvtutor.1
   if test -r $helpsource/mnvtutor$langadd.1; then
      echo installing $destdir/$exename""tutor.1
      LC_ALL=C sed -e s+/usr/local/lib/mnv+$mnvloc+ \
	      -e s+'/usr/local/share/mnv/mnv??'+$mnvloc+ \
	      -e s+$mnvloc/tutor+$tutorsubloc+ \
	      $helpsource/mnvtutor$langadd.1 > $destdir/$exename""tutor.1
      chmod $manmod $destdir/$exename""tutor.1
   fi

   # mnvdiff.1
   if test -r $helpsource/mnvdiff$langadd.1; then
      echo installing $destdir/$mnvdiffname.1
      cp $helpsource/mnvdiff$langadd.1 $destdir/$mnvdiffname.1
      chmod $manmod $destdir/$mnvdiffname.1
   fi

   # emnv.1
   if test -r $helpsource/emnv$langadd.1; then
      echo installing $destdir/$emnvname.1
      LC_ALL=C sed -e s+/usr/local/lib/mnv+$mnvloc+ \
	      -e s+'/usr/local/share/mnv/mnv??'+$mnvloc+ \
	      -e s+$mnvloc/emnv.mnv+$scriptloc/emnv.mnv+ \
	      $helpsource/emnv$langadd.1 > $destdir/$emnvname.1
      chmod $manmod $destdir/$emnvname.1
   fi
fi

if test $what = "uninstall"; then
   echo Checking for MNV manual pages in $destdir...
   if test -r $destdir/$exename.1; then
      echo deleting $destdir/$exename.1
      rm -f $destdir/$exename.1
   fi
   if test -r $destdir/$exename""tutor.1; then
      echo deleting $destdir/$exename""tutor.1
      rm -f $destdir/$exename""tutor.1
   fi
   if test -r $destdir/$mnvdiffname.1; then
      echo deleting $destdir/$mnvdiffname.1
      rm -f $destdir/$mnvdiffname.1
   fi
   if test -r $destdir/$emnvname.1; then
      echo deleting $destdir/$emnvname.1
      rm -f $destdir/$emnvname.1
   fi
fi

if test $what = "xxd" -a -r "$helpsource/xxd${langadd}.1"; then
   echo installing $destdir/xxd.1
   cp $helpsource/xxd$langadd.1 $destdir/xxd.1
   chmod $manmod $destdir/xxd.1
fi

exit $errstatus

# mnv: set sw=3 sts=3 :
