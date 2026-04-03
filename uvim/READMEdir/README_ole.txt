README_ole.txt for version 9.2 of MNV: MNV is not Vim.

This archive contains gmnv.exe with OLE interface.
This version of gmnv.exe can also load a number of interface dynamically (you
can optionally install the .dll files for each interface).
It is only for MS-Windows.

Also see the README_bindos.txt, README_dos.txt and README.txt files.

Be careful not to overwrite the OLE gmnv.exe with the non-OLE gmnv.exe when
unpacking another binary archive!  Check the output of ":version":
	Win32s - "MS-Windows 16/32 bit GUI version"
	 Win32 - "MS-Windows 32 bit GUI version"
Win32 with OLE - "MS-Windows 32 bit GUI version with OLE support"

For further information, type this inside MNV:
	:help if_ole
