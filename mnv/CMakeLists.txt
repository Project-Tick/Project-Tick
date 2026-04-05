###############################################################################
# MNV - CMake Build System
#
# Modern, centralized CMake replacement for the autoconf/Makefile build.
#
# Quick start:
#   mkdir build && cd build
#   cmake ..
#   cmake --build . -j$(nproc)
#   ctest
#   cmake --install .
#
# Configuration examples:
#   cmake .. -DMNV_FEATURE=huge -DMNV_GUI=gtk3
#   cmake .. -DMNV_GUI=none -DMNV_LUA=ON -DMNV_PYTHON3=ON
#   cmake .. -DMNV_FEATURE=tiny
###############################################################################

cmake_minimum_required(VERSION 3.15)

project(MNV
    DESCRIPTION "MNV - MNV is not Vim"
    LANGUAGES C
)

# Use GNUInstallDirs for standard install paths
include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckLibraryExists)
include(CMakeDependentOption)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

###############################################################################
# Build options
###############################################################################

set(MNV_FEATURE "huge" CACHE STRING "Feature level: tiny, normal, huge")
set_property(CACHE MNV_FEATURE PROPERTY STRINGS tiny normal huge)

set(MNV_GUI "auto" CACHE STRING "GUI toolkit: auto, gtk3, gtk2, motif, none")
set_property(CACHE MNV_GUI PROPERTY STRINGS auto gtk3 gtk2 motif none)

option(MNV_TERMINAL    "Enable built-in terminal emulator"     ON)
option(MNV_CHANNEL     "Enable job/channel support"            ON)
option(MNV_NETBEANS    "Enable NetBeans interface"             ON)
option(MNV_CSCOPE      "Enable cscope interface"               ON)
option(MNV_SOUND       "Enable sound support (canberra)"       ON)
option(MNV_ACL         "Enable ACL support"                    ON)
option(MNV_GPM         "Enable GPM mouse support"              ON)
option(MNV_SODIUM      "Enable libsodium encryption"           ON)
option(MNV_MULTIBYTE   "Enable multibyte support"              ON)
option(MNV_XIM         "Enable X Input Method support"         ON)

# Language interpreter options
option(MNV_LUA         "Enable Lua interpreter"                OFF)
option(MNV_LUA_DYNAMIC "Load Lua dynamically"                  ON)
option(MNV_PERL        "Enable Perl interpreter"               OFF)
option(MNV_PERL_DYNAMIC "Load Perl dynamically"                ON)
option(MNV_PYTHON3     "Enable Python 3 interpreter"           OFF)
option(MNV_PYTHON3_DYNAMIC "Load Python 3 dynamically"         ON)
option(MNV_RUBY        "Enable Ruby interpreter"               OFF)
option(MNV_RUBY_DYNAMIC "Load Ruby dynamically"                ON)
option(MNV_TCL         "Enable Tcl interpreter"                OFF)
option(MNV_TCL_DYNAMIC "Load Tcl dynamically"                  ON)
option(MNV_MZSCHEME    "Enable MzScheme/Racket interpreter"    OFF)

# Development options
option(MNV_DEBUG       "Enable debug build"                    OFF)
option(MNV_PROFILE     "Enable profiling"                      OFF)
option(MNV_SANITIZE    "Enable address/undefined sanitizers"   OFF)
option(MNV_LEAK_CHECK  "Enable EXITFREE for leak checking"     OFF)
option(MNV_BUILD_TESTS "Build and enable tests"                ON)

set(MNV_COMPILED_BY "" CACHE STRING "Name of the person compiling MNV")

###############################################################################
# Version info
###############################################################################

set(MNV_MAJOR 10)
set(MNV_MINOR 0)
set(MNV_NAME "mnv")
set(MNV_EX_NAME "ex")
set(MNV_VIEW_NAME "view")

###############################################################################
# Compiler flags
###############################################################################

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wall
        -Wno-deprecated-declarations
    )
    if(NOT MNV_DEBUG)
        add_compile_options(-O2 -fno-strength-reduce)
    endif()
endif()

if(MNV_DEBUG)
    add_compile_options(-g -DDEBUG)
endif()

if(MNV_PROFILE)
    add_compile_options(-pg -g -DWE_ARE_PROFILING)
    add_link_options(-pg)
endif()

if(MNV_SANITIZE)
    add_compile_options(
        -fsanitize=address
        -fsanitize=undefined
        -fsanitize-recover=all
        -fno-omit-frame-pointer
    )
    add_link_options(
        -fsanitize=address
        -fsanitize=undefined
    )
endif()

if(MNV_LEAK_CHECK)
    add_compile_definitions(EXITFREE)
endif()

###############################################################################
# System & header detection
###############################################################################

check_include_file(stdint.h     HAVE_STDINT_H)
check_include_file(stdlib.h     HAVE_STDLIB_H)
check_include_file(string.h     HAVE_STRING_H)
check_include_file(strings.h    HAVE_STRINGS_H)
check_include_file(unistd.h     HAVE_UNISTD_H)
check_include_file(sys/wait.h   HAVE_SYS_WAIT_H)
check_include_file(sys/stat.h   HAVE_SYS_STAT_H)
check_include_file(sys/types.h  HAVE_SYS_TYPES_H)
check_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file(termios.h    HAVE_TERMIOS_H)
check_include_file(termio.h     HAVE_TERMIO_H)
check_include_file(sgtty.h      HAVE_SGTTY_H)
check_include_file(sys/ioctl.h  HAVE_SYS_IOCTL_H)
check_include_file(sys/time.h   HAVE_SYS_TIME_H)
check_include_file(sys/sysctl.h HAVE_SYS_SYSCTL_H)
check_include_file(sys/sysinfo.h HAVE_SYS_SYSINFO_H)
check_include_file(sys/systeminfo.h HAVE_SYS_SYSTEMINFO_H)
check_include_file(sys/stream.h HAVE_SYS_STREAM_H)
check_include_file(sys/ptem.h   HAVE_SYS_PTEM_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/select.h HAVE_SYS_SELECT_H)
check_include_file(sys/utime.h  HAVE_SYS_UTIME_H)
check_include_file(sys/acl.h    HAVE_SYS_ACL_H)
check_include_file(sys/access.h HAVE_SYS_ACCESS_H)
check_include_file(sys/param.h  HAVE_SYS_PARAM_H)
check_include_file(sys/mman.h   HAVE_SYS_MMAN_H)
check_include_file(locale.h     HAVE_LOCALE_H)
check_include_file(libgen.h     HAVE_LIBGEN_H)
check_include_file(wchar.h      HAVE_WCHAR_H)
check_include_file(wctype.h     HAVE_WCTYPE_H)
check_include_file(float.h      HAVE_FLOAT_H)
check_include_file(math.h       HAVE_MATH_H)
check_include_file(dirent.h     HAVE_DIRENT_H)
check_include_file(stropts.h    HAVE_STROPTS_H)
check_include_file(fcntl.h      HAVE_FCNTL_H)
check_include_file(pwd.h        HAVE_PWD_H)
check_include_file(grp.h        HAVE_GRP_H)
check_include_file(langinfo.h   HAVE_LANGINFO_H)
check_include_file(libintl.h    HAVE_LIBINTL_H)
check_include_file(errno.h      HAVE_ERRNO_H)
check_include_file(iconv.h      HAVE_ICONV_H)
check_include_file(inttypes.h   HAVE_INTTYPES_H)
check_include_file(poll.h       HAVE_POLL_H)
check_include_file(dlfcn.h      HAVE_DLFCN_H)
check_include_file(setjmp.h     HAVE_SETJMP_H)
check_include_file(pthread.h    HAVE_PTHREAD_H)
check_include_file(pthread_np.h HAVE_PTHREAD_NP_H)
check_include_file(utime.h      HAVE_UTIME_H)

###############################################################################
# Function detection
###############################################################################

check_function_exists(bcmp          HAVE_BCMP)
check_function_exists(bcopy         HAVE_BCOPY)
check_function_exists(fchdir        HAVE_FCHDIR)
check_function_exists(fchown        HAVE_FCHOWN)
check_function_exists(fchmod        HAVE_FCHMOD)
check_function_exists(fsync         HAVE_FSYNC)
check_function_exists(fdatasync     HAVE_FDATASYNC)
check_function_exists(fseeko        HAVE_FSEEKO)
check_function_exists(ftruncate     HAVE_FTRUNCATE)
check_function_exists(getcwd        HAVE_GETCWD)
check_function_exists(getpwent      HAVE_GETPWENT)
check_function_exists(getpwnam      HAVE_GETPWNAM)
check_function_exists(getpwuid      HAVE_GETPWUID)
check_function_exists(getrlimit     HAVE_GETRLIMIT)
check_function_exists(gettimeofday  HAVE_GETTIMEOFDAY)
check_function_exists(getwd         HAVE_GETWD)
check_function_exists(iconv         HAVE_ICONV)
check_function_exists(lstat         HAVE_LSTAT)
check_function_exists(memcmp        HAVE_MEMCMP)
check_function_exists(memset        HAVE_MEMSET)
check_function_exists(mkdtemp       HAVE_MKDTEMP)
check_function_exists(nanosleep     HAVE_NANOSLEEP)
check_function_exists(opendir       HAVE_OPENDIR)
check_function_exists(posix_openpt  HAVE_POSIX_OPENPT)
check_function_exists(putenv        HAVE_PUTENV)
check_function_exists(qsort         HAVE_QSORT)
check_function_exists(readlink      HAVE_READLINK)
check_function_exists(realpath      HAVE_REALPATH)
check_function_exists(rename        HAVE_RENAME)
check_function_exists(select        HAVE_SELECT)
check_function_exists(setenv        HAVE_SETENV)
check_function_exists(setpgid       HAVE_SETPGID)
check_function_exists(setsid        HAVE_SETSID)
check_function_exists(sigaction     HAVE_SIGACTION)
check_function_exists(sigaltstack   HAVE_SIGALTSTACK)
check_function_exists(sigset        HAVE_SIGSET)
check_function_exists(sigsetjmp     HAVE_SIGSETJMP)
check_function_exists(sigstack      HAVE_SIGSTACK)
check_function_exists(sigvec        HAVE_SIGVEC)
check_function_exists(strerror      HAVE_STRERROR)
check_function_exists(strftime      HAVE_STRFTIME)
check_function_exists(stricmp       HAVE_STRICMP)
check_function_exists(strncasecmp   HAVE_STRNCASECMP)
check_function_exists(strcasecmp    HAVE_STRCASECMP)
check_function_exists(strpbrk       HAVE_STRPBRK)
check_function_exists(strtol        HAVE_STRTOL)
check_function_exists(sysconf       HAVE_SYSCONF)
check_function_exists(sysctl        HAVE_SYSCTL)
check_function_exists(sysinfo       HAVE_SYSINFO)
check_function_exists(towlower      HAVE_TOWLOWER)
check_function_exists(towupper      HAVE_TOWUPPER)
check_function_exists(iswupper      HAVE_ISWUPPER)
check_function_exists(usleep        HAVE_USLEEP)
check_function_exists(utime         HAVE_UTIME)
check_function_exists(utimes        HAVE_UTIMES)
check_function_exists(bind_textdomain_codeset HAVE_BIND_TEXTDOMAIN_CODESET)
check_function_exists(timer_create  HAVE_TIMER_CREATE)
check_function_exists(dlopen        HAVE_DLOPEN)
check_function_exists(dlsym         HAVE_DLSYM)

###############################################################################
# Type sizes
###############################################################################

check_type_size(int     MNV_SIZEOF_INT)
check_type_size(long    MNV_SIZEOF_LONG)
check_type_size(off_t   SIZEOF_OFF_T)
check_type_size(time_t  SIZEOF_TIME_T)

# Check wchar_t size
check_type_size(wchar_t SIZEOF_WCHAR_T)
if(SIZEOF_WCHAR_T EQUAL 2)
    set(SMALL_WCHAR_T 1)
endif()

# Memory move detection
if(HAVE_MEMSET)
    if(HAVE_BCOPY)
        set(USEMEMMOVE 1)
    else()
        set(USEMEMCPY 1)
    endif()
elseif(HAVE_BCOPY)
    set(USEBCOPY 1)
else()
    set(USEMEMMOVE 1)
endif()

# Check __attribute__((unused))
check_c_source_compiles("
    int x __attribute__((unused));
    int main() { return 0; }
" HAVE_ATTRIBUTE_UNUSED)

# Check __DATE__ __TIME__
check_c_source_compiles("
    #include <stdio.h>
    int main() { printf(\"%s\", __DATE__ \" \" __TIME__); return 0; }
" HAVE_DATE_TIME)

# Check for st_blksize in struct stat
check_c_source_compiles("
    #include <sys/stat.h>
    int main() { struct stat s; s.st_blksize = 0; return 0; }
" HAVE_ST_BLKSIZE)

# Check for sockaddr_un
check_c_source_compiles("
    #include <sys/un.h>
    int main() { struct sockaddr_un s; return 0; }
" HAVE_SOCKADDR_UN)

###############################################################################
# Terminal library detection
###############################################################################

set(_term_lib_found FALSE)

# Try ncurses first
find_package(Curses QUIET)
if(CURSES_FOUND)
    set(_term_lib_found TRUE)
    set(TERM_LIBS ${CURSES_LIBRARIES})
    set(TERM_INCLUDE_DIRS ${CURSES_INCLUDE_DIRS})
    check_include_file(ncurses.h HAVE_NCURSES_H)
    check_include_file(ncurses/ncurses.h HAVE_NCURSES_NCURSES_H)
    check_include_file(ncurses/term.h HAVE_NCURSES_TERM_H)
    check_include_file(curses.h HAVE_CURSES_H)
    check_include_file(term.h HAVE_TERM_H)
    check_include_file(termcap.h HAVE_TERMCAP_H)

    # Prefer terminfo
    set(TERMINFO 1)
else()
    # Try termcap
    find_library(TERMCAP_LIB NAMES termcap tinfo)
    if(TERMCAP_LIB)
        set(_term_lib_found TRUE)
        set(TERM_LIBS ${TERMCAP_LIB})
        check_include_file(termcap.h HAVE_TERMCAP_H)
    endif()
endif()

if(NOT _term_lib_found)
    message(WARNING "No terminal library found (ncurses/curses/termcap). Build may fail.")
endif()

# Check for tgetent now that we have the terminal library
if(TERM_LIBS)
    set(CMAKE_REQUIRED_LIBRARIES ${TERM_LIBS})
    include(CheckSymbolExists)
    if(HAVE_TERM_H)
        check_symbol_exists(tgetent "term.h" HAVE_TGETENT)
    elseif(HAVE_NCURSES_TERM_H)
        check_symbol_exists(tgetent "ncurses/term.h" HAVE_TGETENT)
    elseif(HAVE_CURSES_H)
        check_symbol_exists(tgetent "curses.h" HAVE_TGETENT)
    else()
        check_function_exists(tgetent HAVE_TGETENT)
    endif()
    set(CMAKE_REQUIRED_LIBRARIES)
endif()

###############################################################################
# Math library
###############################################################################

find_library(MATH_LIB m)

###############################################################################
# pthreads
###############################################################################

find_package(Threads QUIET)

###############################################################################
# Dynamic loading library
###############################################################################

if(NOT HAVE_DLOPEN)
    find_library(DL_LIB dl)
    if(DL_LIB)
        set(HAVE_DLOPEN 1)
        set(HAVE_DLSYM 1)
    endif()
endif()

###############################################################################
# X11 detection
###############################################################################

find_package(X11 QUIET)
if(X11_FOUND AND NOT MNV_GUI STREQUAL "none")
    set(HAVE_X11 1)
endif()

###############################################################################
# Wayland detection
###############################################################################

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(WAYLAND QUIET wayland-client)
    if(WAYLAND_FOUND)
        set(HAVE_WAYLAND 1)

        # Generate wayland protocol files using wayland-scanner
        find_program(WAYLAND_SCANNER wayland-scanner)
        if(WAYLAND_SCANNER)
            set(WAYLAND_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/auto/wayland")
            set(WAYLAND_PROTO_XML_DIR "${WAYLAND_PROTO_DIR}/protocols")
            set(WAYLAND_GEN_DIR "${WAYLAND_PROTO_DIR}")

            set(WAYLAND_PROTOCOLS
                ext-data-control-v1
                primary-selection-unstable-v1
                wlr-data-control-unstable-v1
                xdg-shell
            )

            foreach(_proto ${WAYLAND_PROTOCOLS})
                set(_xml "${WAYLAND_PROTO_XML_DIR}/${_proto}.xml")
                set(_c "${WAYLAND_GEN_DIR}/${_proto}.c")
                set(_h "${WAYLAND_GEN_DIR}/${_proto}.h")

                if(EXISTS "${_xml}")
                    add_custom_command(
                        OUTPUT "${_c}"
                        COMMAND ${WAYLAND_SCANNER} private-code "${_xml}" "${_c}"
                        DEPENDS "${_xml}"
                        COMMENT "Generating ${_proto}.c"
                    )
                    add_custom_command(
                        OUTPUT "${_h}"
                        COMMAND ${WAYLAND_SCANNER} client-header "${_xml}" "${_h}"
                        DEPENDS "${_xml}"
                        COMMENT "Generating ${_proto}.h"
                    )
                    list(APPEND WAYLAND_GENERATED_SRC "${_c}")
                    list(APPEND WAYLAND_GENERATED_HDR "${_h}")
                endif()
            endforeach()

            add_custom_target(wayland_protocols
                DEPENDS ${WAYLAND_GENERATED_SRC} ${WAYLAND_GENERATED_HDR}
            )
        else()
            message(WARNING "wayland-scanner not found, disabling Wayland")
            set(HAVE_WAYLAND 0)
            set(FEAT_WAYLAND 0)
            set(FEAT_WAYLAND_CLIPBOARD 0)
        endif()
    endif()
endif()

###############################################################################
# GUI detection
###############################################################################

set(_gui_found FALSE)
set(GUI_SRC "")
set(GUI_LIBS "")
set(GUI_INCLUDE_DIRS "")

if(MNV_GUI STREQUAL "auto" OR MNV_GUI STREQUAL "gtk3")
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GTK3 QUIET gtk+-3.0)
        if(GTK3_FOUND)
            set(_gui_found TRUE)
            set(FEAT_GUI 1)
            set(FEAT_GUI_GTK 1)
            set(USE_GTK3 1)
            set(GUI_SRC
                gui.c
                gui_gtk.c
                gui_gtk_f.c
                gui_gtk_x11.c
                gui_beval.c
            )
            set(GUI_LIBS ${GTK3_LIBRARIES})
            set(GUI_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS})
            set(GUI_CFLAGS ${GTK3_CFLAGS_OTHER})
            message(STATUS "GUI: GTK3 found")
        endif()
    endif()
endif()

if(NOT _gui_found AND (MNV_GUI STREQUAL "auto" OR MNV_GUI STREQUAL "gtk2"))
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GTK2 QUIET gtk+-2.0)
        if(GTK2_FOUND)
            set(_gui_found TRUE)
            set(FEAT_GUI 1)
            set(FEAT_GUI_GTK 1)
            set(GUI_SRC
                gui.c
                gui_gtk.c
                gui_gtk_f.c
                gui_gtk_x11.c
                gui_beval.c
            )
            set(GUI_LIBS ${GTK2_LIBRARIES})
            set(GUI_INCLUDE_DIRS ${GTK2_INCLUDE_DIRS})
            set(GUI_CFLAGS ${GTK2_CFLAGS_OTHER})
            message(STATUS "GUI: GTK2 found")
        endif()
    endif()
endif()

if(NOT _gui_found AND (MNV_GUI STREQUAL "auto" OR MNV_GUI STREQUAL "motif"))
    find_package(Motif QUIET)
    if(MOTIF_FOUND)
        set(_gui_found TRUE)
        set(FEAT_GUI 1)
        set(FEAT_GUI_MOTIF 1)
        set(GUI_SRC
            gui.c
            gui_motif.c
            gui_x11.c
            gui_beval.c
            gui_xmdlg.c
            gui_xmebw.c
        )
        set(GUI_LIBS ${MOTIF_LIBRARIES})
        set(GUI_INCLUDE_DIRS ${MOTIF_INCLUDE_DIR})
        message(STATUS "GUI: Motif found")
    endif()
endif()

if(NOT _gui_found)
    if(MNV_GUI STREQUAL "none" OR MNV_GUI STREQUAL "auto")
        message(STATUS "GUI: disabled")
    else()
        message(FATAL_ERROR "Requested GUI '${MNV_GUI}' not found")
    endif()
endif()

###############################################################################
# Sound support (libcanberra)
###############################################################################

if(MNV_SOUND AND PKG_CONFIG_FOUND)
    pkg_check_modules(CANBERRA QUIET libcanberra)
    if(CANBERRA_FOUND)
        set(FEAT_SOUND 1)
        set(HAVE_CANBERRA 1)
    endif()
endif()

###############################################################################
# ACL support
###############################################################################

if(MNV_ACL AND HAVE_SYS_ACL_H)
    find_library(ACL_LIB acl)
    if(ACL_LIB)
        check_library_exists(acl acl_get_file "" HAVE_POSIX_ACL)
    endif()
endif()

###############################################################################
# GPM mouse support
###############################################################################

if(MNV_GPM)
    check_include_file(gpm.h HAVE_GPM_H)
    if(HAVE_GPM_H)
        find_library(GPM_LIB gpm)
        if(GPM_LIB)
            set(FEAT_MOUSE_GPM 1)
        endif()
    endif()
endif()

###############################################################################
# libsodium encryption
###############################################################################

if(MNV_SODIUM AND PKG_CONFIG_FOUND)
    pkg_check_modules(SODIUM QUIET libsodium)
    if(SODIUM_FOUND)
        set(HAVE_SODIUM 1)
    endif()
endif()

###############################################################################
# Language interpreter detection
###############################################################################

# Lua
if(MNV_LUA)
    find_package(Lua QUIET)
    if(LUA_FOUND)
        set(FEAT_LUA 1)
        if(MNV_LUA_DYNAMIC)
            set(DYNAMIC_LUA 1)
            # Determine the shared library filename for dynamic loading
            foreach(_lua_lib ${LUA_LIBRARIES})
                if(_lua_lib MATCHES "\\.(so|dylib)")
                    get_filename_component(_lua_realpath "${_lua_lib}" REALPATH)
                    get_filename_component(DYNAMIC_LUA_DLL "${_lua_realpath}" NAME)
                    break()
                endif()
            endforeach()
            if(NOT DYNAMIC_LUA_DLL)
                set(DYNAMIC_LUA_DLL "liblua.so")
            endif()
        endif()
        message(STATUS "Lua: ${LUA_VERSION_STRING}")
    else()
        message(WARNING "Lua requested but not found")
    endif()
endif()

# Perl
if(MNV_PERL)
    find_package(Perl QUIET)
    find_package(PerlLibs QUIET)
    if(PERL_FOUND AND PERLLIBS_FOUND)
        set(FEAT_PERL 1)
        if(MNV_PERL_DYNAMIC)
            set(DYNAMIC_PERL 1)
            # Determine the shared library filename for dynamic loading
            if(PERL_LIBRARY)
                get_filename_component(_perl_realpath "${PERL_LIBRARY}" REALPATH)
                get_filename_component(DYNAMIC_PERL_DLL "${_perl_realpath}" NAME)
            else()
                foreach(_perl_lib ${PERLLIBS_LIBRARIES})
                    if(_perl_lib MATCHES "\\.(so|dylib)")
                        get_filename_component(_perl_realpath "${_perl_lib}" REALPATH)
                        get_filename_component(DYNAMIC_PERL_DLL "${_perl_realpath}" NAME)
                        break()
                    endif()
                endforeach()
            endif()
            if(NOT DYNAMIC_PERL_DLL)
                set(DYNAMIC_PERL_DLL "libperl.so")
            endif()
        endif()
        message(STATUS "Perl: ${PERL_VERSION_STRING}")
    else()
        message(WARNING "Perl requested but not found")
    endif()
endif()

# Python 3
if(MNV_PYTHON3)
    find_package(Python3 COMPONENTS Development QUIET)
    if(Python3_FOUND)
        set(FEAT_PYTHON3 1)
        if(MNV_PYTHON3_DYNAMIC)
            set(DYNAMIC_PYTHON3 1)
            # Determine the shared library filename for dynamic loading
            if(Python3_LIBRARIES)
                foreach(_py3_lib ${Python3_LIBRARIES})
                    if(_py3_lib MATCHES "\\.(so|dylib)")
                        get_filename_component(_py3_realpath "${_py3_lib}" REALPATH)
                        get_filename_component(DYNAMIC_PYTHON3_DLL "${_py3_realpath}" NAME)
                        break()
                    endif()
                endforeach()
            endif()
            if(NOT DYNAMIC_PYTHON3_DLL)
                set(DYNAMIC_PYTHON3_DLL "libpython${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}.so")
            endif()
        endif()
        message(STATUS "Python3: ${Python3_VERSION}")
    else()
        message(WARNING "Python3 requested but not found")
    endif()
endif()

# Ruby
if(MNV_RUBY)
    find_package(Ruby QUIET)
    if(RUBY_FOUND)
        set(FEAT_RUBY 1)
        if(MNV_RUBY_DYNAMIC)
            set(DYNAMIC_RUBY 1)
            # Determine the shared library filename for dynamic loading
            if(RUBY_LIBRARY)
                get_filename_component(_ruby_realpath "${RUBY_LIBRARY}" REALPATH)
                get_filename_component(DYNAMIC_RUBY_DLL "${_ruby_realpath}" NAME)
            endif()
            if(NOT DYNAMIC_RUBY_DLL)
                set(DYNAMIC_RUBY_DLL "libruby.so")
            endif()
        endif()
        message(STATUS "Ruby: ${RUBY_VERSION}")
    else()
        message(WARNING "Ruby requested but not found")
    endif()
endif()

# Tcl
if(MNV_TCL)
    find_package(TCL QUIET)
    if(TCL_FOUND)
        set(FEAT_TCL 1)
        if(MNV_TCL_DYNAMIC)
            set(DYNAMIC_TCL 1)
            # Determine the shared library filename for dynamic loading
            if(TCL_LIBRARY)
                get_filename_component(_tcl_realpath "${TCL_LIBRARY}" REALPATH)
                get_filename_component(DYNAMIC_TCL_DLL "${_tcl_realpath}" NAME)
            endif()
            if(NOT DYNAMIC_TCL_DLL)
                set(DYNAMIC_TCL_DLL "libtcl.so")
            endif()
        endif()
        message(STATUS "Tcl: found")
    else()
        message(WARNING "Tcl requested but not found")
    endif()
endif()

###############################################################################
# Feature level
###############################################################################

if(MNV_FEATURE STREQUAL "huge")
    set(FEAT_HUGE 1)
    set(FEAT_EVAL 1)
    set(FEAT_BEVAL 1)
    set(FEAT_CSCOPE 1)
    set(FEAT_NETBEANS_INTG 1)
    set(FEAT_JOB_CHANNEL 1)
    set(FEAT_TERMINAL 1)
    set(FEAT_IPV6 1)
    set(WANT_SOCKETSERVER 1)
    if(HAVE_X11)
        set(FEAT_CLIENTSERVER 1)
        set(FEAT_CLIPBOARD 1)
        set(FEAT_XIM 1)
        set(FEAT_XSMP 1)
    elseif(HAVE_SOCKADDR_UN)
        # Socket-based clientserver on Unix
        set(FEAT_CLIENTSERVER 1)
    endif()
    if(HAVE_WAYLAND)
        set(FEAT_WAYLAND 1)
    endif()
elseif(MNV_FEATURE STREQUAL "normal")
    set(FEAT_NORMAL 1)
    set(FEAT_EVAL 1)
    set(WANT_SOCKETSERVER 1)
    if(MNV_CSCOPE)
        set(FEAT_CSCOPE 1)
    endif()
    if(MNV_CHANNEL)
        set(FEAT_JOB_CHANNEL 1)
    endif()
    if(HAVE_WAYLAND)
        set(FEAT_WAYLAND 1)
    endif()
elseif(MNV_FEATURE STREQUAL "tiny")
    set(FEAT_TINY 1)
    set(FEAT_TERMINAL 0)
    set(FEAT_JOB_CHANNEL 0)
    set(FEAT_NETBEANS_INTG 0)
endif()

# Terminal requires channel support
if(FEAT_TERMINAL AND NOT FEAT_JOB_CHANNEL)
    message(STATUS "Terminal disabled: requires channel support")
    set(FEAT_TERMINAL 0)
endif()

###############################################################################
# Generate config files
###############################################################################

set(HAVE_CONFIG_H 1)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/auto/config.h
    @ONLY
)

# Generate pathdef.c
# Collect info for pathdef
set(MNV_INSTALL_RCLOC "${CMAKE_INSTALL_FULL_DATADIR}/mnv")
set(MNV_INSTALL_RTLOC "${CMAKE_INSTALL_FULL_DATADIR}/mnv/mnv${MNV_MAJOR}${MNV_MINOR}")
set(MNV_ALL_CFLAGS "${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS}")
set(MNV_ALL_LFLAGS "${CMAKE_C_COMPILER} ${CMAKE_EXE_LINKER_FLAGS}")

if(MNV_COMPILED_BY)
    set(MNV_COMPILED_USER "${MNV_COMPILED_BY}")
else()
    execute_process(
        COMMAND whoami
        OUTPUT_VARIABLE MNV_COMPILED_USER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE MNV_COMPILED_SYS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pathdef.c.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/auto/pathdef.c
    @ONLY
)

###############################################################################
# Generate osdef.h (simplified - use empty file, let headers handle it)
###############################################################################

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/auto/osdef.h")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/auto/osdef.h"
        "/* osdef.h - generated by CMake */\n"
        "/* On modern systems most definitions come from system headers */\n"
    )
endif()

# Create a symlink so that #include "../auto/config.h" from xdiff works
# The xdiff sources use a hardcoded relative path
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/auto/config.h")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/auto")
endif()
# Copy generated config.h and osdef.h to where the sources expect them
configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/auto/config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/auto/config.h
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_BINARY_DIR}/auto/osdef.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/auto/osdef.h
    COPYONLY
)

###############################################################################
# Subdirectory builds
###############################################################################

# libvterm (terminal emulation library)
if(FEAT_TERMINAL)
    add_subdirectory(src/libvterm)
endif()

# xxd (hex dump utility)
add_subdirectory(src/xxd)

###############################################################################
# xdiff object library
###############################################################################

add_library(xdiff OBJECT
    src/xdiff/xdiffi.c
    src/xdiff/xemit.c
    src/xdiff/xprepare.c
    src/xdiff/xutils.c
    src/xdiff/xhistogram.c
    src/xdiff/xpatience.c
)

target_include_directories(xdiff PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xdiff
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(xdiff PRIVATE
    HAVE_CONFIG_H
    UNIX
)

if(FEAT_GUI)
    target_include_directories(xdiff PRIVATE ${GUI_INCLUDE_DIRS})
    if(GUI_CFLAGS)
        target_compile_options(xdiff PRIVATE ${GUI_CFLAGS})
    endif()
endif()

###############################################################################
# MNV core source files
###############################################################################

set(MNV_CORE_SRC
    src/alloc.c
    src/arabic.c
    src/arglist.c
    src/autocmd.c
    src/beval.c
    src/blob.c
    src/blowfish.c
    src/buffer.c
    src/bufwrite.c
    src/change.c
    src/charset.c
    src/cindent.c
    src/clientserver.c
    src/clipboard.c
    src/cmdexpand.c
    src/cmdhist.c
    src/crypt.c
    src/crypt_zip.c
    src/debugger.c
    src/dict.c
    src/diff.c
    src/digraph.c
    src/drawline.c
    src/drawscreen.c
    src/edit.c
    src/eval.c
    src/evalbuffer.c
    src/evalfunc.c
    src/evalvars.c
    src/evalwindow.c
    src/ex_cmds.c
    src/ex_cmds2.c
    src/ex_docmd.c
    src/ex_eval.c
    src/ex_getln.c
    src/fileio.c
    src/filepath.c
    src/findfile.c
    src/float.c
    src/fold.c
    src/fuzzy.c
    src/gc.c
    src/getchar.c
    src/gui_xim.c
    src/hardcopy.c
    src/hashtab.c
    src/help.c
    src/highlight.c
    src/if_cscope.c
    src/if_xcmdsrv.c
    src/indent.c
    src/insexpand.c
    src/json.c
    src/linematch.c
    src/list.c
    src/locale.c
    src/logfile.c
    src/main.c
    src/map.c
    src/mark.c
    src/match.c
    src/mbyte.c
    src/memfile.c
    src/memline.c
    src/menu.c
    src/message.c
    src/misc1.c
    src/misc2.c
    src/mouse.c
    src/move.c
    src/normal.c
    src/ops.c
    src/option.c
    src/optionstr.c
    src/os_unix.c
    src/popupmenu.c
    src/popupwin.c
    src/profiler.c
    src/pty.c
    src/quickfix.c
    src/regexp.c
    src/register.c
    src/screen.c
    src/scriptfile.c
    src/search.c
    src/session.c
    src/sha256.c
    src/sign.c
    src/sound.c
    src/spell.c
    src/spellfile.c
    src/spellsuggest.c
    src/strings.c
    src/syntax.c
    src/tabpanel.c
    src/tag.c
    src/term.c
    src/terminal.c
    src/testing.c
    src/textformat.c
    src/textobject.c
    src/textprop.c
    src/time.c
    src/tuple.c
    src/typval.c
    src/ui.c
    src/undo.c
    src/usercmd.c
    src/userfunc.c
    src/version.c
    src/mnv9class.c
    src/mnv9cmds.c
    src/mnv9compile.c
    src/mnv9execute.c
    src/mnv9expr.c
    src/mnv9generics.c
    src/mnv9instr.c
    src/mnv9script.c
    src/mnv9type.c
    src/mnvinfo.c
    src/window.c
    ${CMAKE_CURRENT_BINARY_DIR}/auto/pathdef.c
)

# Channel/job sources
if(FEAT_JOB_CHANNEL)
    list(APPEND MNV_CORE_SRC
        src/channel.c
        src/job.c
    )
endif()

# NetBeans interface
if(FEAT_NETBEANS_INTG AND FEAT_JOB_CHANNEL)
    list(APPEND MNV_CORE_SRC
        src/netbeans.c
    )
endif()

# GUI sources
if(FEAT_GUI AND GUI_SRC)
    foreach(_gui_src ${GUI_SRC})
        list(APPEND MNV_CORE_SRC "src/${_gui_src}")
    endforeach()
endif()

# Wayland source
if(HAVE_WAYLAND)
    list(APPEND MNV_CORE_SRC src/wayland.c)
    if(WAYLAND_GENERATED_SRC)
        list(APPEND MNV_CORE_SRC ${WAYLAND_GENERATED_SRC})
    endif()
endif()

# Language interpreter sources
if(FEAT_LUA)
    list(APPEND MNV_CORE_SRC src/if_lua.c)
endif()

if(FEAT_PERL)
    # Perl .xs files need preprocessing with xsubpp to generate C code
    find_program(PERL_EXECUTABLE perl)
    if(PERL_EXECUTABLE)
        # Find xsubpp
        execute_process(
            COMMAND ${PERL_EXECUTABLE} -MConfig -e "print \$Config{privlibexp}"
            OUTPUT_VARIABLE PERL_PRIVLIB
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        find_program(XSUBPP xsubpp HINTS ${PERL_PRIVLIB}/ExtUtils /usr/bin/core_perl)

        if(XSUBPP)
            set(_perl_generated "${CMAKE_CURRENT_BINARY_DIR}/auto/if_perl.c")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/auto")
            add_custom_command(
                OUTPUT "${_perl_generated}"
                COMMAND ${PERL_EXECUTABLE} ${XSUBPP} -prototypes -typemap
                    "${PERL_PRIVLIB}/ExtUtils/typemap"
                    "${CMAKE_CURRENT_SOURCE_DIR}/src/if_perl.xs"
                    > "${_perl_generated}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/if_perl.xs"
                COMMENT "Generating if_perl.c from if_perl.xs"
            )
            list(APPEND MNV_CORE_SRC "${_perl_generated}")

            # Get Perl compile flags
            execute_process(
                COMMAND ${PERL_EXECUTABLE} -MExtUtils::Embed -e ccopts
                OUTPUT_VARIABLE PERL_CFLAGS
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            # Extract include dirs from Perl ccopts
            string(REGEX MATCHALL "-I[^ ]+" PERL_INCLUDE_FLAGS "${PERL_CFLAGS}")
            foreach(_flag ${PERL_INCLUDE_FLAGS})
                string(REGEX REPLACE "^-I" "" _dir "${_flag}")
                list(APPEND PERL_INCLUDE_DIRS "${_dir}")
            endforeach()
            # Extract compile definitions from Perl ccopts (-D flags)
            string(REGEX MATCHALL "-D[^ ]+" PERL_DEFINE_FLAGS "${PERL_CFLAGS}")
            # Extract other compile options (-f flags etc.)
            string(REGEX MATCHALL "-f[^ ]+" PERL_COMPILE_OPTIONS "${PERL_CFLAGS}")
        else()
            message(WARNING "xsubpp not found, Perl support disabled")
            set(FEAT_PERL 0)
        endif()
    else()
        message(WARNING "perl not found, Perl support disabled")
        set(FEAT_PERL 0)
    endif()
endif()

if(FEAT_PYTHON3)
    list(APPEND MNV_CORE_SRC src/if_python3.c)
endif()

if(FEAT_RUBY)
    list(APPEND MNV_CORE_SRC src/if_ruby.c)

    # Compute numeric Ruby version (e.g. 34 for Ruby 3.4.x)
    string(REGEX REPLACE "\\." "" _ruby_ver_nodots "${RUBY_VERSION}")
    string(SUBSTRING "${_ruby_ver_nodots}" 0 2 RUBY_VERSION_NUM)
endif()

if(FEAT_TCL)
    list(APPEND MNV_CORE_SRC src/if_tcl.c)
endif()

if(FEAT_MZSCHEME)
    list(APPEND MNV_CORE_SRC src/if_mzsch.c)
endif()

###############################################################################
# MNV executable
###############################################################################

add_executable(mnv ${MNV_CORE_SRC} $<TARGET_OBJECTS:xdiff>)

target_include_directories(mnv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xdiff
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(mnv PRIVATE
    HAVE_CONFIG_H
    UNIX
)

# Note: Feature-level defines (FEAT_HUGE, FEAT_TERMINAL, etc.) are set
# in auto/config.h and consumed by feature.h. Do NOT duplicate them here
# as compile definitions - feature.h auto-computes derived features.

# Terminal library
if(TERM_LIBS)
    target_link_libraries(mnv PRIVATE ${TERM_LIBS})
endif()
if(TERM_INCLUDE_DIRS)
    target_include_directories(mnv PRIVATE ${TERM_INCLUDE_DIRS})
endif()

# Math library
if(MATH_LIB)
    target_link_libraries(mnv PRIVATE ${MATH_LIB})
endif()

# pthreads
if(Threads_FOUND)
    target_link_libraries(mnv PRIVATE Threads::Threads)
endif()

# Dynamic loading
if(DL_LIB)
    target_link_libraries(mnv PRIVATE ${DL_LIB})
endif()

# libvterm
if(FEAT_TERMINAL)
    target_link_libraries(mnv PRIVATE vterm)
    target_include_directories(mnv PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libvterm/include
    )
endif()

# X11
if(HAVE_X11)
    target_link_libraries(mnv PRIVATE ${X11_LIBRARIES})
    target_include_directories(mnv PRIVATE ${X11_INCLUDE_DIR})
    if(X11_Xt_FOUND)
        target_link_libraries(mnv PRIVATE ${X11_Xt_LIB})
    endif()
    if(X11_SM_FOUND AND FEAT_XSMP)
        target_link_libraries(mnv PRIVATE ${X11_SM_LIB} ${X11_ICE_LIB})
    endif()
endif()

# Wayland
if(HAVE_WAYLAND)
    target_link_libraries(mnv PRIVATE ${WAYLAND_LIBRARIES})
    target_include_directories(mnv PRIVATE ${WAYLAND_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/auto/wayland
    )
    if(TARGET wayland_protocols)
        add_dependencies(mnv wayland_protocols)
    endif()
endif()

# GUI
if(FEAT_GUI)
    target_link_libraries(mnv PRIVATE ${GUI_LIBS})
    target_include_directories(mnv PRIVATE ${GUI_INCLUDE_DIRS})
    if(GUI_CFLAGS)
        target_compile_options(mnv PRIVATE ${GUI_CFLAGS})
    endif()
endif()

# Sound
if(FEAT_SOUND AND CANBERRA_FOUND)
    target_link_libraries(mnv PRIVATE ${CANBERRA_LIBRARIES})
    target_include_directories(mnv PRIVATE ${CANBERRA_INCLUDE_DIRS})
endif()

# ACL
if(ACL_LIB AND HAVE_POSIX_ACL)
    target_link_libraries(mnv PRIVATE ${ACL_LIB})
endif()

# GPM
if(FEAT_MOUSE_GPM AND GPM_LIB)
    target_link_libraries(mnv PRIVATE ${GPM_LIB})
endif()

# Sodium
if(HAVE_SODIUM)
    target_link_libraries(mnv PRIVATE ${SODIUM_LIBRARIES})
    target_include_directories(mnv PRIVATE ${SODIUM_INCLUDE_DIRS})
endif()

# Lua
if(FEAT_LUA)
    if(NOT DYNAMIC_LUA)
        target_link_libraries(mnv PRIVATE ${LUA_LIBRARIES})
    endif()
    target_include_directories(mnv PRIVATE ${LUA_INCLUDE_DIR})
endif()

# Perl
if(FEAT_PERL)
    if(NOT DYNAMIC_PERL AND PERLLIBS_LIBRARIES)
        target_link_libraries(mnv PRIVATE ${PERLLIBS_LIBRARIES})
    endif()
    if(PERL_INCLUDE_DIRS)
        target_include_directories(mnv PRIVATE ${PERL_INCLUDE_DIRS})
    endif()
    if(PERL_INCLUDE_PATH)
        target_include_directories(mnv PRIVATE ${PERL_INCLUDE_PATH})
    endif()
    # Apply Perl-specific compile definitions to the generated if_perl.c
    if(_perl_generated AND PERL_DEFINE_FLAGS)
        set_source_files_properties("${_perl_generated}" PROPERTIES
            COMPILE_OPTIONS "${PERL_DEFINE_FLAGS}"
        )
    endif()
endif()

# Python 3
if(FEAT_PYTHON3)
    if(NOT DYNAMIC_PYTHON3)
        target_link_libraries(mnv PRIVATE Python3::Python)
    endif()
    target_include_directories(mnv PRIVATE ${Python3_INCLUDE_DIRS})
endif()

# Ruby
if(FEAT_RUBY)
    if(NOT DYNAMIC_RUBY)
        target_link_libraries(mnv PRIVATE ${RUBY_LIBRARY})
    endif()
    target_include_directories(mnv PRIVATE ${RUBY_INCLUDE_DIRS})
    target_compile_definitions(mnv PRIVATE RUBY_VERSION=${RUBY_VERSION_NUM})
endif()

# Tcl
if(FEAT_TCL)
    if(NOT DYNAMIC_TCL)
        target_link_libraries(mnv PRIVATE ${TCL_LIBRARY})
    else()
        # Dynamic Tcl still requires the stubs library for Tcl_InitStubs
        find_library(TCL_STUB_LIBRARY NAMES tclstub tclstub8.6 tclstub8.5
            HINTS "${TCL_LIBRARY_DIR}" "${TCL_PREFIX}/lib"
        )
        if(TCL_STUB_LIBRARY)
            target_link_libraries(mnv PRIVATE ${TCL_STUB_LIBRARY})
        else()
            message(WARNING "Tcl stubs library not found, dynamic Tcl may fail")
        endif()
    endif()
    target_include_directories(mnv PRIVATE ${TCL_INCLUDE_PATH})
endif()

# Set output name
set_target_properties(mnv PROPERTIES
    OUTPUT_NAME ${MNV_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

###############################################################################
# Unit test executables
###############################################################################

if(MNV_BUILD_TESTS)

    # Helper function to create a unit test executable with the same
    # configuration as the main mnv target.
    # REPLACED_SRC: source file to replace (e.g. json.c)
    # TEST_SRC: test source file (e.g. json_test.c)
    function(mnv_add_unit_test TEST_NAME REPLACED_SRC TEST_SRC)
        set(_srcs ${MNV_CORE_SRC})
        list(REMOVE_ITEM _srcs "src/main.c" "src/${REPLACED_SRC}")
        list(APPEND _srcs "src/${TEST_SRC}")

        add_executable(${TEST_NAME} ${_srcs} $<TARGET_OBJECTS:xdiff>)

        target_include_directories(${TEST_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/proto
            ${CMAKE_CURRENT_SOURCE_DIR}/src/xdiff
            ${CMAKE_CURRENT_BINARY_DIR}
        )
        target_compile_definitions(${TEST_NAME} PRIVATE HAVE_CONFIG_H UNIX)

        # Link same libraries as mnv
        target_link_libraries(${TEST_NAME} PRIVATE ${TERM_LIBS})
        if(MATH_LIB)
            target_link_libraries(${TEST_NAME} PRIVATE ${MATH_LIB})
        endif()
        if(FEAT_TERMINAL)
            target_link_libraries(${TEST_NAME} PRIVATE vterm)
            target_include_directories(${TEST_NAME} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src/libvterm/include
            )
        endif()
        if(Threads_FOUND)
            target_link_libraries(${TEST_NAME} PRIVATE Threads::Threads)
        endif()
        if(DL_LIB)
            target_link_libraries(${TEST_NAME} PRIVATE ${DL_LIB})
        endif()
        if(HAVE_X11)
            target_link_libraries(${TEST_NAME} PRIVATE ${X11_LIBRARIES})
            target_include_directories(${TEST_NAME} PRIVATE ${X11_INCLUDE_DIR})
            if(X11_Xt_FOUND)
                target_link_libraries(${TEST_NAME} PRIVATE ${X11_Xt_LIB})
            endif()
            if(X11_SM_FOUND AND FEAT_XSMP)
                target_link_libraries(${TEST_NAME} PRIVATE ${X11_SM_LIB} ${X11_ICE_LIB})
            endif()
        endif()
        if(TERM_INCLUDE_DIRS)
            target_include_directories(${TEST_NAME} PRIVATE ${TERM_INCLUDE_DIRS})
        endif()
        if(HAVE_WAYLAND)
            target_link_libraries(${TEST_NAME} PRIVATE ${WAYLAND_LIBRARIES})
            target_include_directories(${TEST_NAME} PRIVATE
                ${WAYLAND_INCLUDE_DIRS}
                ${CMAKE_CURRENT_SOURCE_DIR}/src/auto/wayland
            )
            if(TARGET wayland_protocols)
                add_dependencies(${TEST_NAME} wayland_protocols)
            endif()
        endif()
        if(HAVE_SODIUM)
            target_link_libraries(${TEST_NAME} PRIVATE ${SODIUM_LIBRARIES})
            target_include_directories(${TEST_NAME} PRIVATE ${SODIUM_INCLUDE_DIRS})
        endif()
        if(ACL_LIB AND HAVE_POSIX_ACL)
            target_link_libraries(${TEST_NAME} PRIVATE ${ACL_LIB})
        endif()
        if(FEAT_SOUND AND CANBERRA_FOUND)
            target_link_libraries(${TEST_NAME} PRIVATE ${CANBERRA_LIBRARIES})
        endif()
        if(FEAT_MOUSE_GPM AND GPM_LIB)
            target_link_libraries(${TEST_NAME} PRIVATE ${GPM_LIB})
        endif()
        if(FEAT_GUI)
            target_link_libraries(${TEST_NAME} PRIVATE ${GUI_LIBS})
            target_include_directories(${TEST_NAME} PRIVATE ${GUI_INCLUDE_DIRS})
            if(GUI_CFLAGS)
                target_compile_options(${TEST_NAME} PRIVATE ${GUI_CFLAGS})
            endif()
        endif()

        # Language interpreter support for tests
        if(FEAT_LUA)
            if(NOT DYNAMIC_LUA)
                target_link_libraries(${TEST_NAME} PRIVATE ${LUA_LIBRARIES})
            endif()
            target_include_directories(${TEST_NAME} PRIVATE ${LUA_INCLUDE_DIR})
        endif()
        if(FEAT_PERL)
            if(NOT DYNAMIC_PERL AND PERLLIBS_LIBRARIES)
                target_link_libraries(${TEST_NAME} PRIVATE ${PERLLIBS_LIBRARIES})
            endif()
            if(PERL_INCLUDE_DIRS)
                target_include_directories(${TEST_NAME} PRIVATE ${PERL_INCLUDE_DIRS})
            endif()
            if(PERL_INCLUDE_PATH)
                target_include_directories(${TEST_NAME} PRIVATE ${PERL_INCLUDE_PATH})
            endif()
        endif()
        if(FEAT_PYTHON3)
            if(NOT DYNAMIC_PYTHON3)
                target_link_libraries(${TEST_NAME} PRIVATE Python3::Python)
            endif()
            target_include_directories(${TEST_NAME} PRIVATE ${Python3_INCLUDE_DIRS})
        endif()
        if(FEAT_RUBY)
            if(NOT DYNAMIC_RUBY)
                target_link_libraries(${TEST_NAME} PRIVATE ${RUBY_LIBRARY})
            endif()
            target_include_directories(${TEST_NAME} PRIVATE ${RUBY_INCLUDE_DIRS})
            target_compile_definitions(${TEST_NAME} PRIVATE RUBY_VERSION=${RUBY_VERSION_NUM})
        endif()
        if(FEAT_TCL)
            if(NOT DYNAMIC_TCL)
                target_link_libraries(${TEST_NAME} PRIVATE ${TCL_LIBRARY})
            else()
                if(TCL_STUB_LIBRARY)
                    target_link_libraries(${TEST_NAME} PRIVATE ${TCL_STUB_LIBRARY})
                endif()
            endif()
            target_include_directories(${TEST_NAME} PRIVATE ${TCL_INCLUDE_PATH})
        endif()

        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_tests_properties(${TEST_NAME} PROPERTIES
            TIMEOUT 120
            ENVIRONMENT "DISPLAY=;WAYLAND_DISPLAY="
        )
    endfunction()

    # Create unit test executables
    # Each replaces main.c and one other source with a test variant
    mnv_add_unit_test(json_test    json.c     json_test.c)
    mnv_add_unit_test(kword_test   charset.c  kword_test.c)
    mnv_add_unit_test(memfile_test memfile.c  memfile_test.c)
    mnv_add_unit_test(message_test message.c  message_test.c)

    # Script tests (invoke via the existing test Makefile)
    add_test(
        NAME script_tests
        COMMAND ${CMAKE_COMMAND} -E env
            "MNVPROG=$<TARGET_FILE:mnv>"
            "MNVRUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/runtime"
            "LINES=24"
            "COLUMNS=80"
            ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/src/testdir
                -f Makefile nongui
                MNVPROG=$<TARGET_FILE:mnv>
                SCRIPTSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/testdir
    )
    set_tests_properties(script_tests PROPERTIES
        TIMEOUT 3600
        LABELS "scripts"
    )

    # Indent tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/indent/Makefile")
        add_test(
            NAME indent_tests
            COMMAND ${CMAKE_COMMAND} -E env
                "MNVRUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/runtime"
                "LINES=24"
                "COLUMNS=80"
                ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/runtime/indent
                    test MNVPROG=$<TARGET_FILE:mnv>
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/indent
        )
        set_tests_properties(indent_tests PROPERTIES
            TIMEOUT 600
            LABELS "indent"
        )
    endif()

    # Syntax tests
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/syntax/Makefile")
        add_test(
            NAME syntax_tests
            COMMAND ${CMAKE_COMMAND} -E env
                "MNVRUNTIME=${CMAKE_CURRENT_SOURCE_DIR}/runtime"
                "LINES=24"
                "COLUMNS=80"
                ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR}/runtime/syntax
                    test MNVPROG=$<TARGET_FILE:mnv>
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/syntax
        )
        set_tests_properties(syntax_tests PROPERTIES
            TIMEOUT 600
            LABELS "syntax"
        )
    endif()

    enable_testing()
endif()

###############################################################################
# Installation
###############################################################################

# Install the main executable
install(TARGETS mnv
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create symlinks for ex, view, etc.
install(CODE "
    set(_bindir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}\")
    message(STATUS \"Creating symlinks in \${_bindir}\")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/ex)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/view)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/rmnv)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/rview)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/mnvdiff)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/vi)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/vim)
")

if(FEAT_GUI)
    install(CODE "
        set(_bindir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/gmnv)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/gview)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/gmnvdiff)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/rgmnv)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/rgview)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/emnv)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/eview)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/gvi)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv \${_bindir}/gvim)
    ")
endif()

# Install runtime files
set(MNV_RUNTIME_DIR "${CMAKE_INSTALL_DATADIR}/mnv/mnv${MNV_MAJOR}${MNV_MINOR}")

# Runtime directories to install
set(RUNTIME_SUBDIRS
    colors
    compiler
    doc
    ftplugin
    import
    indent
    keymap
    lang
    macros
    pack
    plugin
    print
    spell
    syntax
    tools
    tutor
    autoload
)

foreach(_subdir ${RUNTIME_SUBDIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/runtime/${_subdir}")
        install(DIRECTORY "runtime/${_subdir}"
            DESTINATION ${MNV_RUNTIME_DIR}
            PATTERN ".gitignore" EXCLUDE
            PATTERN "testdir" EXCLUDE
            PATTERN "generator" EXCLUDE
            PATTERN "Makefile" EXCLUDE
            PATTERN "Make_all.mak" EXCLUDE
        )
    endif()
endforeach()

# Install individual runtime files
set(RUNTIME_SCRIPTS
    defaults.mnv
    emnv.mnv
    filetype.mnv
    ftoff.mnv
    ftplugin.mnv
    ftplugof.mnv
    indent.mnv
    indoff.mnv
    menu.mnv
    mswin.mnv
    optwin.mnv
    bugreport.mnv
    scripts.mnv
    synmenu.mnv
    delmenu.mnv
)

foreach(_script ${RUNTIME_SCRIPTS})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/${_script}")
        install(FILES "runtime/${_script}"
            DESTINATION ${MNV_RUNTIME_DIR}
        )
    endif()
endforeach()

# Install mnvrc template
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/mnvrc_example.mnv")
    install(FILES "runtime/mnvrc_example.mnv"
        DESTINATION ${MNV_RUNTIME_DIR}
    )
endif()

# Install man pages
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/doc/mnv.1")
    install(FILES runtime/doc/mnv.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()

# Man page symlinks
install(CODE "
    set(_mandir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_MANDIR}/man1\")
    file(MAKE_DIRECTORY \${_mandir})
    if(EXISTS \"\${_mandir}/mnv.1\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv.1 \${_mandir}/ex.1)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv.1 \${_mandir}/view.1)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv.1 \${_mandir}/rmnv.1)
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink mnv.1 \${_mandir}/rview.1)
    endif()
")

# Desktop file and icons
if(FEAT_GUI)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/mnv.desktop")
        install(FILES runtime/mnv.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        )
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/gmnv.desktop")
        install(FILES runtime/gmnv.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        )
    endif()
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pixmaps")
        # Install hicolor icons at standard sizes
        foreach(_size 16 32 48 128)
            set(_icon "${CMAKE_CURRENT_SOURCE_DIR}/pixmaps/mnv${_size}x${_size}.png")
            if(EXISTS "${_icon}")
                install(FILES "${_icon}"
                    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps
                    RENAME gmnv.png
                )
            endif()
        endforeach()
        # Install scalable SVG icon
        set(_icon_svg "${CMAKE_CURRENT_SOURCE_DIR}/pixmaps/mnvlogo.svg")
        if(EXISTS "${_icon_svg}")
            install(FILES "${_icon_svg}"
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
                RENAME gmnv.svg
            )
        endif()
    endif()
endif()

###############################################################################
# Build summary
###############################################################################

message(STATUS "")
message(STATUS "=== MNV ${PROJECT_VERSION} Build Configuration ===")
message(STATUS "  Feature level:    ${MNV_FEATURE}")
message(STATUS "  GUI:              ${MNV_GUI} (found: ${_gui_found})")
message(STATUS "  Terminal:         ${FEAT_TERMINAL}")
message(STATUS "  Channel/Job:      ${FEAT_JOB_CHANNEL}")
message(STATUS "  X11:              ${HAVE_X11}")
message(STATUS "  Wayland:          ${HAVE_WAYLAND}")
message(STATUS "  Terminal lib:     ${TERM_LIBS}")
if(FEAT_SOUND)
    message(STATUS "  Sound:            canberra")
endif()
if(HAVE_SODIUM)
    message(STATUS "  Encryption:       libsodium")
endif()
if(FEAT_LUA)
    message(STATUS "  Lua:              ${LUA_VERSION_STRING}")
endif()
if(FEAT_PYTHON3)
    message(STATUS "  Python 3:         ${Python3_VERSION}")
endif()
if(FEAT_RUBY)
    message(STATUS "  Ruby:             ${RUBY_VERSION}")
endif()
if(FEAT_PERL)
    message(STATUS "  Perl:             ${PERL_VERSION_STRING}")
endif()
if(FEAT_TCL)
    message(STATUS "  Tcl:              yes")
endif()
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Compiled by:      ${MNV_COMPILED_USER}@${MNV_COMPILED_SYS}")
message(STATUS "=============================================")
message(STATUS "")
