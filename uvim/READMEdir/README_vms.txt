README_vms.txt for version 9.2 of MNV: MNV is not Vim.

This file explains the installation of MNV on VMS systems.
See "README.txt" in the runtime archive for information about MNV.


Most information can be found in the on-line documentation.  Use ":help vms"
inside MNV.  Or get the runtime files and read runtime/doc/os_vms.txt to find
out how to install and configure MNV with runtime files etc.

To compile MNV yourself you need three archives:
  mnv-X.X-rt.tar.gz	runtime files
  mnv-X.X-src.tar.gz	source files
  mnv-X.X-extra.tar.gz	extra source files

Compilation is recommended, in order to make sure that the correct
libraries are used for your specific system.  Read about compiling in
src/INSTALLvms.txt.

To use the binary version, you need one of these archives:

  mnv-XX-exe-x86-gui.zip	X86_64 GUI/Motif executables
  mnv-XX-exe-x86-term.zip	X86_64 console executables
  mnv-XX-exe-ia64-gui.zip	IA64 GUI/Motif executables
  mnv-XX-exe-ia64-gtk.zip	IA64 GUI/GTK executables
  mnv-XX-exe-ia64-term.zip	IA64 console executables
  mnv-XX-exe-axp-gui.zip	Alpha GUI/Motif executables
  mnv-XX-exe-axp-gtk.zip	Alpha GUI/GTK executables
  mnv-XX-exe-axp-term.zip	Alpha console executables
  mnv-XX-exe-vax-gui.zip	VAX GUI executables
  mnv-XX-exe-vax-term.zip	VAX console executables

and of course
  mnv-XX-runtime.zip		runtime files

The binary archives contain: mnv.exe, ctags.exe, xxd.exe files,
but there are also prepared "deploy ready" archives:

mnv-XX-x86.zip			GUI and console executables with runtime and
				help files for X86_64 systems
mnv-XX-ia64.zip			GUI and console executables with runtime and
				help files for IA64 systems
mnv-XX-axp.zip			GUI and console executables with runtime and
				help files for Alpha systems
mnv-XX-vax.zip			GUI and console executables with runtime and
				help files for VAX systems

GTK builds need LIBGTK library installed.

These executables and up to date patches for OpenVMS system are downloadable
from http://www.polarhome.com/mnv/ or ftp://ftp.polarhome.com/pub/mnv/

