@rem
@rem To use this with Visual Studio .Net
@rem Tools->External Tools...
@rem Add
@rem      Title     - MNV
@rem      Command   - d:\files\util\mnv_vs_net.cmd
@rem      Arguments - +$(CurLine) $(ItemPath)
@rem      Init Dir  - Empty
@rem
@rem Courtesy of Brian Sturk
@rem
@rem --remote-silent +%1 is a command +954, move ahead 954 lines
@rem --remote-silent %2 full path to file
@rem In MNV
@rem    :h --remote-silent for more details
@rem
@rem --servername VS_NET
@rem This will create a new instance of mnv called VS_NET.  So if you open
@rem multiple files from VS, they will use the same instance of MNV.
@rem This allows you to have multiple copies of MNV running, but you can
@rem control which one has VS files in it.
@rem
start /b gmnv.exe --servername VS_NET --remote-silent "%1"  "%2"
