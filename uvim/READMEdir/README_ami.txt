README_ami.txt for version 9.2 of MNV: MNV is not Vim.

This file explains the installation of MNV on Amiga systems.
See README.txt for general information about MNV.


Unpack the distributed files in the place where you want to keep them.  It is
wise to have a "mnv" directory to keep your mnvrc file and any other files you
change.  The distributed files go into a subdirectory.  This way you can
easily upgrade to a new version.  For example:

  dh0:editors/mnv		contains your mnvrc and modified files
  dh0:editors/mnv/mnv54		contains the MNV version 5.4 distributed files
  dh0:editors/mnv/mnv55		contains the MNV version 5.5 distributed files

You would then unpack the archives like this:

  cd dh0:editors
  tar xf t:mnv92bin.tar
  tar xf t:mnv92rt.tar

Set the $MNV environment variable to point to the top directory of your MNV
files.  For the above example:

  set MNV=dh0:editors/mnv

MNV version 5.4 will look for your mnvrc file in $MNV, and for the runtime
files in $MNV/mnv54.  See ":help $MNV" for more information.

Make sure the MNV executable is in your search path.  Either copy the MNV
executable to a directory that is in your search path, or (preferred) modify
the search path to include the directory where the MNV executable is.
