#! /bin/sh
# installml.sh --- install or uninstall manpage links for MNV
#
# arguments:
# 1  what: "install" or "uninstall"
# 2  also do GUI pages: "yes" or ""
# 3  target directory		     e.g., "/usr/local/man/it/man1"
# 4  mnv exe name		     e.g., "mnv"
# 5  mnvdiff exe name		     e.g., "mnvdiff"
# 6  emnv exe name		     e.g., "emnv"
# 7  ex exe name		     e.g., "ex"
# 8  view exe name		     e.g., "view"
# 9  rmnv exe name		     e.g., "rmnv"
# 10 rview exe name		     e.g., "rview"
# 11 gmnv exe name		     e.g., "gmnv"
# 12 gview exe name		     e.g., "gview"
# 13 rgmnv exe name		     e.g., "rgmnv"
# 14 rgview exe name		     e.g., "rgview"
# 15 gmnvdiff exe name		     e.g., "gmnvdiff"
# 16 eview exe name		     e.g., "eview"

errstatus=0

what=$1
gui=$2
destdir=$3
mnvname=$4
mnvdiffname=$5
emnvname=$6
exname=$7
viewname=$8
rmnvname=$9
# old shells don't understand ${10}
shift
rviewname=$9
shift
gmnvname=$9
shift
gviewname=$9
shift
rgmnvname=$9
shift
rgviewname=$9
shift
gmnvdiffname=$9
shift
eviewname=$9

if test $what = "install" -a \( -f $destdir/$mnvname.1 -o -f $destdir/$mnvdiffname.1 -o -f $destdir/$eviewname.1 \); then
   if test ! -d $destdir; then
      echo creating $destdir
      /bin/sh install-sh -c -d $destdir
   fi

   # ex
   if test ! -f $destdir/$exname.1 -a -f $destdir/$mnvname.1; then
      echo creating link $destdir/$exname.1
      cd $destdir; ln -s $mnvname.1 $exname.1
   fi

   # view
   if test ! -f $destdir/$viewname.1 -a -f $destdir/$mnvname.1; then
      echo creating link $destdir/$viewname.1
      cd $destdir; ln -s $mnvname.1 $viewname.1
   fi

   # rmnv
   if test ! -f $destdir/$rmnvname.1 -a -f $destdir/$mnvname.1; then
      echo creating link $destdir/$rmnvname.1
      cd $destdir; ln -s $mnvname.1 $rmnvname.1
   fi

   # rview
   if test ! -f $destdir/$rviewname.1 -a -f $destdir/$mnvname.1; then
      echo creating link $destdir/$rviewname.1
      cd $destdir; ln -s $mnvname.1 $rviewname.1
   fi

   # GUI targets are optional
   if test "$gui" = "yes"; then
      # gmnv
      if test ! -f $destdir/$gmnvname.1 -a -f $destdir/$mnvname.1; then
	 echo creating link $destdir/$gmnvname.1
	 cd $destdir; ln -s $mnvname.1 $gmnvname.1
      fi

      # gview
      if test ! -f $destdir/$gviewname.1 -a -f $destdir/$mnvname.1; then
	 echo creating link $destdir/$gviewname.1
	 cd $destdir; ln -s $mnvname.1 $gviewname.1
      fi

      # rgmnv
      if test ! -f $destdir/$rgmnvname.1 -a -f $destdir/$mnvname.1; then
	 echo creating link $destdir/$rgmnvname.1
	 cd $destdir; ln -s $mnvname.1 $rgmnvname.1
      fi

      # rgview
      if test ! -f $destdir/$rgviewname.1 -a -f $destdir/$mnvname.1; then
	 echo creating link $destdir/$rgviewname.1
	 cd $destdir; ln -s $mnvname.1 $rgviewname.1
      fi

      # gmnvdiff
      if test ! -f $destdir/$gmnvdiffname.1 -a -f $destdir/$mnvdiffname.1; then
	 echo creating link $destdir/$gmnvdiffname.1
	 cd $destdir; ln -s $mnvdiffname.1 $gmnvdiffname.1
      fi

      # eview
      if test ! -f $destdir/$eviewname.1 -a -f $destdir/$emnvname.1; then
	 echo creating link $destdir/$eviewname.1
	 cd $destdir; ln -s $emnvname.1 $eviewname.1
      fi
   fi
fi

if test $what = "uninstall"; then
   echo Checking for MNV manual page links in $destdir...

   if test -L $destdir/$exname.1; then
      echo deleting $destdir/$exname.1
      rm -f $destdir/$exname.1
   fi
   if test -L $destdir/$viewname.1; then
      echo deleting $destdir/$viewname.1
      rm -f $destdir/$viewname.1
   fi
   if test -L $destdir/$rmnvname.1; then
      echo deleting $destdir/$rmnvname.1
      rm -f $destdir/$rmnvname.1
   fi
   if test -L $destdir/$rviewname.1; then
      echo deleting $destdir/$rviewname.1
      rm -f $destdir/$rviewname.1
   fi

   # GUI targets are optional
   if test "$gui" = "yes"; then
      if test -L $destdir/$gmnvname.1; then
	 echo deleting $destdir/$gmnvname.1
	 rm -f $destdir/$gmnvname.1
      fi
      if test -L $destdir/$gviewname.1; then
	 echo deleting $destdir/$gviewname.1
	 rm -f $destdir/$gviewname.1
      fi
      if test -L $destdir/$rgmnvname.1; then
	 echo deleting $destdir/$rgmnvname.1
	 rm -f $destdir/$rgmnvname.1
      fi
      if test -L $destdir/$rgviewname.1; then
	 echo deleting $destdir/$rgviewname.1
	 rm -f $destdir/$rgviewname.1
      fi
      if test -L $destdir/$gmnvdiffname.1; then
	 echo deleting $destdir/$gmnvdiffname.1
	 rm -f $destdir/$gmnvdiffname.1
      fi
      if test -L $destdir/$eviewname.1; then
	 echo deleting $destdir/$eviewname.1
	 rm -f $destdir/$eviewname.1
      fi
   fi
fi

exit $errstatus

# mnv: set sw=3 :
