# MSVC warning flags for CI builds.
# Ignored MSVC warnings:
# /wd4514  Unreferenced inline function has been removed.
# /wd4625  Copy constructor was implicitly defined as deleted.
# /wd4626  Assignment operator was implicitly defined as deleted.
# /wd4710  Function not inlined.
# /wd4711  Function selected for automatic inline expansion.
# /wd4820  Padding added after data member.
# /wd5026  Move constructor was implicitly defined as deleted.
# /wd5027  Move assignment operator was implicitly defined as deleted.
# /wd5045  Compiler will insert Spectre mitigation for memory load.
# /wd4868  Compiler may not enforce left-to-right evaluation order in braced initializer list.

set(MSVC_CXXFLAGS
    /W4
    /WX
    /permissive-
    /Zc:__cplusplus
    /EHsc
    /wd4514
    /wd4625
    /wd4626
    /wd4710
    /wd4711
    /wd4820
    /wd5026
    /wd5027
    /wd5045
    /wd4868
)
