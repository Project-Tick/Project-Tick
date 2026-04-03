README_dos.txt for version 9.2 of MNV: MNV is not Vim.

This file explains the installation of MNV on MS-Windows systems.
See "README.txt" for general information about MNV.

There are two ways to install MNV:
A. Use the self-installing .exe file.
B. Unpack .zip files and run the install.exe program.


A. Using the self-installing .exe
---------------------------------

This is mostly self-explaining.  Just follow the prompts and make the
selections.  A few things to watch out for:

- When an existing installation is detected, you are offered to first remove
  this.  The uninstall program is then started while the install program waits
  for it to complete.  Sometimes the windows overlap each other, which can be
  confusing.  Be sure the complete the uninstalling before continuing the
  installation.  Watch the taskbar for uninstall windows.

- When selecting a directory to install MNV, use the same place where other
  versions are located.  This makes it easier to find your _mnvrc file.  For
  example "C:\Program Files\mnv" or "D:\mnv".  A name ending in "mnv" is
  preferred.

- After selecting the directory where to install MNV, clicking on "Next" will
  start the installation.


B. Using .zip files
-------------------

These are the normal steps to install MNV from the .zip archives:

1. Go to the directory where you want to put the MNV files.  Examples:
	cd C:\
	cd D:\editors
   If you already have a "mnv" directory, go to the directory in which it is
   located.  Check the $MNV setting to see where it points to:
	set MNV
   For example, if you have
	C:\mnv\mnv92
   do
	cd C:\
   Binary and runtime MNV archives are normally unpacked in the same location,
   on top of each other.

2. Unpack the zip archives.  This will create a new directory "mnv\mnv92",
   in which all the distributed MNV files are placed.  Since the directory
   name includes the version number, it is unlikely that you overwrite
   existing files.
   Examples:
	pkunzip -d gmnv92.zip
	unzip mnv92w32.zip

   You need to unpack the runtime archive and at least one of the binary
   archives.  When using more than one binary version, be careful not to
   overwrite one version with the other, the names of the executables
   "mnv.exe" and "gmnv.exe" are the same.

   After you unpacked the files, you can still move the whole directory tree
   to another location.  That is where they will stay, the install program
   won't move or copy the runtime files.

3. Change to the new directory:
	cd mnv\mnv92
   Run the "install.exe" program.  It will ask you a number of questions about
   how you would like to have your MNV setup.  Among these are:
   - You can tell it to write a "_mnvrc" file with your preferences in the
     parent directory.
   - It can also install an "Edit with MNV" entry in the Windows Explorer
     popup menu.
   - You can have it create batch files, so that you can run MNV from the
     console or in a shell.  You can select one of the directories in your
     $PATH.  If you skip this, you can add MNV to the search path manually:
     The simplest is to add a line to your autoexec.bat.  Examples:
	set path=%path%;C:\mnv\mnv92
	set path=%path%;D:\editors\mnv\mnv92
   - Create entries for MNV on the desktop and in the Start menu.

That's it!


Remarks:

- If MNV can't find the runtime files, ":help" won't work and the GUI version
  won't show a menubar.  Then you need to set the $MNV environment variable to
  point to the top directory of your MNV files.  Example:
    set MNV=C:\editors\mnv
  MNV version 9.2 will look for your mnvrc file in $MNV, and for the runtime
  files in $MNV/mnv92.  See ":help $MNV" for more information.

- To avoid confusion between distributed files of different versions and your
  own modified mnv scripts, it is recommended to use this directory layout:
  ("C:\mnv" is used here as the root, replace it with the path you use)
  Your own files:
	C:\mnv\_mnvrc			Your personal mnvrc.
	C:\mnv\_mnvinfo			Dynamic info for 'mnvinfo'.
	C:\mnv\mnvfiles\ftplugin\*.mnv	Filetype plugins
	C:\mnv\...			Other files you made.
  Distributed files:
	C:\mnv\mnv92\mnv.exe		The MNV version 9.2 executable.
	C:\mnv\mnv92\doc\*.txt		The version 9.2 documentation files.
	C:\mnv\mnv92\bugreport.mnv	A MNV version 9.2 script.
	C:\mnv\mnv92\...		Other version 9.2 distributed files.
  In this case the $MNV environment variable would be set like this:
	set MNV=C:\mnv
  Then $MNVRUNTIME will automatically be set to "$MNV\mnv92".  Don't add
  "mnv92" to $MNV, that won't work.

- You can put your MNV executable anywhere else.  If the executable is not
  with the other MNV files, you should set $MNV. The simplest is to add a line
  to your autoexec.bat.  Examples:
	set MNV=c:\mnv
	set MNV=d:\editors\mnv

- If you have told the "install.exe" program to add the "Edit with MNV" menu
  entry, you can remove it by running the "uninstall.exe".  See
  ":help win32-popup-menu".

For further information, type one of these inside MNV:
	:help dos
	:help win32
