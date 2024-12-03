dnl                   C O N F I G U R E . A C
dnl BZFlag
dnl Copyright (c) 1993-2023 Tim Riker
dnl
dnl This package is free software;  you can redistribute it and/or
dnl modify it under the terms of the license found in the file
dnl named COPYING that should have accompanied this file.
dnl
dnl THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
dnl IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
dnl
dnl ******************************************************************
dnl ***                  BZFlag's configure.ac                     ***
dnl ******************************************************************
dnl
dnl Herein lies the venerable GNU build system configure template for
dnl BZFlag.  As best is reasonably possible, proper ordering and
dnl separation of tests and settings should be maintained per the
dnl recommended standard layout.  The tests should be added to the
dnl rather clearly labeled sections below so that they are as
dnl follows:
dnl
dnl     0) information on the package
dnl     1) check command-line arguments
dnl     2) check programs
dnl     3) check libraries
dnl     4) check headers
dnl     5) check types/structures
dnl     6) check compiler characteristics
dnl     7) check functions
dnl     8) check system services
dnl     9) output a summary
dnl
dnl You should use enable/disable arguments for build settings and
dnl optional compilation components that are part of this package.
dnl You specify with/without arguments for components that are not a
dnl part of this package.
dnl
dnl TODO: This mess (still) needs serious cleaning up.
dnl

dnl Minimum version of autoconf required.  Should coincide with the
dnl setting in the autogen.sh script.
AC_PREREQ([2.68])

AC_INIT([BZFlag],[2.4.27],[http://BZFlag.org/],[bzflag])
AC_CONFIG_SRCDIR(src/bzflag/bzflag.cxx)
AC_CONFIG_MACRO_DIR([m4])

# When --libdir has not been used put plugins in bzflag's own directory.
if test "x$libdir" = 'x${exec_prefix}/lib' ; then
    libdir="$libdir/bzflag"
fi

AC_CONFIG_AUX_DIR(misc)

CONF_DATE=`date -u +%Y-%m-%d`
# https://reproducible-builds.org/specs/source-date-epoch/
if test -z "$SOURCE_DATE_EPOCH"; then
    BUILD_DATE=$CONF_DATE
else
    # accept any numeric value
    if expr 0 + "$SOURCE_DATE_EPOCH" > /dev/null ; then
	BUILD_DATE=`date -u -d "@$SOURCE_DATE_EPOCH" +%Y-%m-%d 2> /dev/null`
	# try using perl if the date command fails
	if test -z "$BUILD_DATE"; then
	    BUILD_DATE=`perl -le "use POSIX; print strftime '%Y-%m-%d', gmtime '$SOURCE_DATE_EPOCH'"`
	fi
    else
	AC_MSG_ERROR([Invalid SOURCE_DATE_EPOCH value])
    fi
fi
case "$BUILD_DATE" in
    # double square brackets because of m4
    [[0-9]][[0-9]][[0-9]][[0-9]]-[[01]][[0-9]]-[[0-3]][[0-9]])
	# looks like YYYY-MM-DD
	;;
    *)
	AC_MSG_ERROR([Failed to set BUILD_DATE])
	;;
esac

# print out the title with a pretty box computed to wrap around
title="Configuring $PACKAGE_STRING on $CONF_DATE"
length="`echo x${title}x | wc -c`"
separator=""
while test $length -gt 1 ; do
separator="${separator}*"
length="`expr $length - 1`"
done

BZ_BOLD
AC_MSG_RESULT([***${separator}***])
AC_MSG_RESULT([*** ${title} ***])
AC_MSG_RESULT([***${separator}***])
BZ_UNBOLD

AC_CANONICAL_TARGET

dnl Minimum version of automake required.  Should coincide with the
dnl setting in the autogen.sh script.  Create the other archives
dnl automatically when doing a make dist for convenience.
AM_INIT_AUTOMAKE([1.6 dist-zip dist-bzip2 subdir-objects])

dnl all configured definitions get written here
AC_CONFIG_HEADERS([include/config.h])

AC_SUBST(PACKAGE_STRING)
AC_SUBST(BUILD_DATE)
AC_DEFINE_UNQUOTED(BUILD_DATE, "$BUILD_DATE", [BZFlag build date])


# Let the C++ code know about OS
AC_DEFINE_UNQUOTED(BZ_BUILD_OS, "$host_os", [BZFlag System Environment])

# automatically enable and load our configure cache file if available
BZ_CONFIG_CACHE([config.cache.${host_os}.${ac_hostname}])


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

BZ_CONFIGURE_STAGE([arguments], [1 of 9])

# provide a with-curses option, test for curses
MP_WITH_CURSES

# build the server?
AC_ARG_ENABLE(server, AS_HELP_STRING([--disable-server],[do not compile server]))

# check whether plugins are desired
AC_ARG_ENABLE(plugins, AS_HELP_STRING([--disable-plugins],[do not compile plugins]))
if test "x$enable_plugins" != xno ; then
    if test "x$enable_server" != xno ; then
	AC_DEFINE(BZ_PLUGINS, 1, [Enable plugins])
    else
	enable_plugins=no
	AC_MSG_WARN([skipping plugins for client-only build])
    fi
fi
AM_CONDITIONAL(BUILD_PLUGINS, test "x$enable_plugins" != "xno")

# accept a list of custom plugins to be built
AC_ARG_ENABLE(custom-plugins, AS_HELP_STRING([--enable-custom-plugins=plugin1[[[,plugin2,...]]]],[comma separated list of custom plugins to be built]),[
  CUSTOM_PLUGIN_LIST=`echo $enableval | sed 's/,/ /g'`
  for plugin in $CUSTOM_PLUGIN_LIST ; do
    m1=plugins/$plugin/Makefile
    AC_CONFIG_FILES([$m1])
    (cd $srcdir && automake --no-force $m1)
  done])
# accept a file containing the custom plugins to be built
AC_ARG_ENABLE(custom-plugins-file, AS_HELP_STRING([--enable-custom-plugins-file=filename],[file of custom plugins to be built, one name per line]),[
  for plugin in `sed '/^#/d' $enableval` ; do
    m2=plugins/$plugin/Makefile
    AC_CONFIG_FILES([$m2])
    (cd $srcdir && automake --no-force $m2)
    CUSTOM_PLUGIN_LIST="$CUSTOM_PLUGIN_LIST $plugin"
  done])
AC_SUBST(CUSTOM_PLUGIN_LIST)

# check whether threads are desired
AC_ARG_ENABLE(threads, [  --disable-threads        build without threading])

# do we want bzadmin?
AC_ARG_ENABLE(bzadmin, [  --disable-bzadmin       do not build text client])

AC_ARG_ENABLE(robots, [  --disable-robots        disable robots])
if test x$enable_robots != xno; then
    AC_DEFINE(ROBOT, 1, [Enabling Robots])
fi

# debugging and profiling
AC_ARG_ENABLE(debug,     [  --enable-debug          turn on debugging])
AC_ARG_ENABLE(profiling, [  --enable-profiling      turn on profiling])
# profiling requires debugging
if test x$enable_profiling = xyes; then
    enable_debug=yes
fi
if test x$enable_debug = xyes; then
    AC_DEFINE(DEBUG, 1, [Debugging])
    AC_DEFINE(DEBUG_RENDERING, 1, [Debug Rendering])
    user_CFLAGS="$ac_save_CFLAGS"
    user_CXXFLAGS="$CXXFLAGS"	# ac_save_CXXFLAGS is not set yet
fi

# do we want the bzflag  client?
AC_ARG_ENABLE(client, [  --disable-client        server-only build])

# enable UPnP for server
AC_ARG_ENABLE(UPnP,       [  --enable-UPnP           Use UPnP for server],
	      , [enable_UPnP=no])

dnl **************************
dnl *** Check for programs ***
dnl **************************

BZ_CONFIGURE_STAGE([programs], [2 of 9])

# cannot override LD directly, so warn (configure will override it)
if test "x$LD" != "x" ; then
    AC_MSG_WARN([LD cannot be set directly yet it seems to be set ($LD)])
fi

#Checks for programs.
AC_PROG_CXX
if test -z "$CXX"; then
    AC_MSG_ERROR([A c++ compiler is required to build BZFlag])
fi
AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
AC_PROG_CC
AC_PROG_LN_S
AC_CHECK_PROG(AR, ar, ar)
AC_SUBST(AR)
AC_SUBST(AR_FLAGS)

# Enable Objective-C compilations for OS X
case $host_os in
    darwin*)
        AC_PROG_OBJC
        ;;
    *)
        AM_CONDITIONAL(am__fastdepOBJC, false)
        ;;
esac

AC_PATH_PROG(CCACHE, ccache)
if test "$CCACHE" ; then
    CC="ccache $CC"
    CXX="ccache $CXX"
fi

AC_CHECK_TOOL(WINDRES, windres, :)

# libtool shouldn't be generated until after LD is set.
LT_INIT([dlopen])
AC_SUBST(LIBTOOL_DEPS)

# quell the verbosity
LIBTOOLFLAGS=--silent
AC_SUBST(LIBTOOLFLAGS)
if test "x$LIBTOOL" != "x" ; then
    LIBTOOL="$LIBTOOL --silent"
fi

AC_CHECK_PROG(HASPOD2MAN, pod2man, yes, no)
if test $HASPOD2MAN = no; then
    AC_MSG_WARN([some man pages will not be generated])
fi
AM_CONDITIONAL(HASPOD2MAN, test $HASPOD2MAN = yes)


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

BZ_CONFIGURE_STAGE([libraries], [3 of 9])

AC_LANG(C++)

dnl explicitly link with the dl library when available
AC_CHECK_LIB(dl, dlopen, LIBDL="-ldl", LIBDL="")
AC_SUBST(LIBDL)

dnl detect a usable system zlib compression library
AC_CHECK_LIB(z, compressBound, LIBZ="-lz", AC_MSG_ERROR([working zlib was not found]))
AC_SUBST(LIBZ)


dnl detect a usable system c-ares lookup library
AC_CHECK_LIB(cares, ares_init, LIBCARES="-lcares", AC_MSG_ERROR([working c-ares was not found]))
AC_SUBST(LIBCARES)

# see if our c-ares callbacks match (they won't on c-ares <1.5)
AC_MSG_CHECKING(for c-ares DNS lookup functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBCARES"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <ares.h>
  void callback(void * /*arg*/, int /*callbackStatus*/,
              int /*timeouts*/, struct hostent * /*hostent*/) {}
]], [[
  ares_channel aresChannel;
  in_addr requestedAddress;

  ares_gethostbyaddr(aresChannel, &requestedAddress,
		     sizeof(in_addr), AF_INET, callback, NULL);
]])],[AC_MSG_RESULT([yes])],[AC_MSG_FAILURE([working c-ares library was not found])])
LIBS="$PRELIBS"

AC_CHECK_LIB(cares, ares_getaddrinfo, AC_DEFINE([HAVE_ARES_GETADDRINFO],[1],[Define if libcares includes ares_getaddrinfo.]))



dnl detect a usable system curl library
LIBCURL=""
LIBCURL_CHECK_CONFIG(, 7.55.0)
AC_MSG_CHECKING(checking for curl library)
if test "x$LIBCURL" = "x" ; then
    AC_MSG_ERROR([working curl library was not found])
else
    AC_MSG_RESULT([yes])
fi

AC_MSG_CHECKING(checking for SSL support in curl)
if test "x$libcurl_feature_SSL" = "xyes" ; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_ERROR([curl library missing SSL support])
fi


dnl detect a usable system regular expression library
AC_CHECK_LIB(c, regcomp, LIBREGEX="-lc",
[AC_CHECK_LIB(regex, regcomp, LIBREGEX="-lregex",
    [AC_CHECK_LIB(compat, regcomp, LIBREGEX="-lcompat", AC_MSG_ERROR([working regex library was not found]))]
)]
)
AC_SUBST(LIBREGEX)

AC_CHECK_HEADER(regex.h, AC_DEFINE(HAVE_REGEX_H,1,[Define to 1 if you have the '<regex.h>' header file]), AC_MSG_ERROR([working regex.h was not found]))

# see if we have regex functionality
AC_MSG_CHECKING(for regular expression functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBREGEX"
AC_RUN_IFELSE([
AC_LANG_PROGRAM([
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <regex.h>
],[
    regex_t re;
    regcomp(&re, "abc", REG_EXTENDED);
    regexec(&re, "", 0, 0, 0);
    regfree(&re);
    return 0;
])
], [AC_MSG_RESULT([yes])],
[AC_MSG_ERROR([regex test program failed to build or run])],
[AC_MSG_WARN([regex test program run skipped due to cross-compiling])] )
LDFLAGS="$PRELDFLAGS"



dnl detect a usable system -lrt library for shm_open()
AC_CHECK_LIB(rt, shm_open, LIBRT="-lrt", LIBRT="")
AC_SUBST(LIBRT)

# Check for the math lib
AC_CHECK_LIB(m, sqrtf)

# for BeOS - old network stack don't have those libs ( move it in the case switch ?)
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([socket], [gethostent], [], AC_CHECK_LIB([bind], [gethostent]))

# see if pthreads are in libc_r (as on FreeBSD) or libpthread
AC_CHECK_LIB([c_r], [pthread_create], LIBPTHREAD="-lc_r",
    [AC_CHECK_LIB([pthread], [pthread_create], LIBPTHREAD="-lpthread",
     LIBPTHREAD="")]
)
AC_SUBST(LIBPTHREAD)

# check for various IEEE and c99 standard math functions in libm
AC_CHECK_LIB([m], [acosf], [AC_DEFINE([HAVE_ACOSF],[1],[libm includes acosf])])
AC_CHECK_LIB([m], [asinf], [AC_DEFINE([HAVE_ASINF],[1],[libm includes asinf])])
AC_CHECK_LIB([m], [atan2f], [AC_DEFINE([HAVE_ATAN2F],[1],[libm includes atan2f])])
AC_CHECK_LIB([m], [atanf], [AC_DEFINE([HAVE_ATANF],[1],[libm includes atanf])])
AC_CHECK_LIB([m], [cosf], [AC_DEFINE([HAVE_COSF],[1],[libm includes cosf])])
AC_CHECK_LIB([m], [expf], [AC_DEFINE([HAVE_EXPF],[1],[libm includes expf])])
AC_CHECK_LIB([m], [fabsf], [AC_DEFINE([HAVE_FABSF],[1],[libm includes fabsf])])
AC_CHECK_LIB([m], [floorf], [AC_DEFINE([HAVE_FLOORF],[1],[libm includes floorf])])
AC_CHECK_LIB([m], [fmodf], [AC_DEFINE([HAVE_FMODF],[1],[libm includes fmodf])])
AC_CHECK_LIB([m], [hypotf], [AC_DEFINE([HAVE_HYPOTF],[1],[libm includes hypotf])])
AC_CHECK_LIB([m], [logf], [AC_DEFINE([HAVE_LOGF],[1],[libm includes logf])])
AC_CHECK_LIB([m], [log10f], [AC_DEFINE([HAVE_LOG10F],[1],[libm includes log10f])])
AC_CHECK_LIB([m], [powf], [AC_DEFINE([HAVE_POWF],[1],[libm includes powf])])
AC_CHECK_LIB([m], [sinf], [AC_DEFINE([HAVE_SINF],[1],[libm includes sinf])])
AC_CHECK_LIB([m], [sqrtf], [AC_DEFINE([HAVE_SQRTF],[1],[libm includes sqrtf])])
AC_CHECK_LIB([m], [tanf], [AC_DEFINE([HAVE_TANF],[1],[libm includes tanf])])

AC_MSG_CHECKING([for numeric type sizes])
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(short int)
AC_CHECK_SIZEOF(long int)
AC_CHECK_SIZEOF(long long int)
AC_CHECK_SIZEOF(float)
AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)
AC_DEFINE_UNQUOTED(SIZEOF_INT,           $ac_cv_sizeof_int,           [sizeof int])
AC_DEFINE_UNQUOTED(SIZEOF_SHORT_INT,     $ac_cv_sizeof_short_int,     [sizeof short int])
AC_DEFINE_UNQUOTED(SIZEOF_LONG_INT,      $ac_cv_sizeof_long_int,      [sizeof long int])
AC_DEFINE_UNQUOTED(SIZEOF_LONG_LONG_INT, $ac_cv_sizeof_long_long_int, [sizeof long long int])
AC_DEFINE_UNQUOTED(SIZEOF_FLOAT,         $ac_cv_sizeof_float,         [sizeof float])
AC_DEFINE_UNQUOTED(SIZEOF_DOUBLE,        $ac_cv_sizeof_double,        [sizeof double])
AC_DEFINE_UNQUOTED(SIZEOF_LONG_DOUBLE,   $ac_cv_sizeof_long_double,   [sizeof long double])


dnl *************************
dnl *** Check for headers ***
dnl *************************

BZ_CONFIGURE_STAGE([headers], [4 of 9])

AC_LANG(C++)
AC_CHECK_HEADERS( \
	cmath \
	cstdlib \
	cstdio \
	cstring
)
AC_LANG(C)
AC_CHECK_HEADERS( \
	SDL2/SDL.h \
	bstring.h \
	dlfcn.h \
	dsound.h \
	fcntl.h \
	inttypes.h \
	limits.h \
	linux/input.h \
	process.h \
	sched.h \
	stdint.h \
	sys/param.h \
	sys/socket.h \
	netdb.h \
	unistd.h \
	values.h \
)

if test "x$enable_UPnP" = "xyes"; then
    AC_CHECK_HEADERS(
	[miniupnpc/miniupnpc.h],
	[LIBMINIUPNPC=-lminiupnpc],
	[AC_MSG_ERROR([You need to install miniupnpc for UPnP to work])])
fi
AC_SUBST(LIBMINIUPNPC)

dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

BZ_CONFIGURE_STAGE([types], [5 of 9])

#AM_CHECK_TYPES(socklen_t)
#AC_CHECK_TYPES(socklen_t)
AC_C_BIGENDIAN
AC_C_CONST

AC_MSG_CHECKING([for socklen_t type])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
		#include <sys/socket.h>]],
 [[socklen_t len = 42; return 0;]])],
 [ac_cv_type_socklen_t=yes],[ac_cv_type_socklen_t=no])
AC_MSG_RESULT([$ac_cv_type_socklen_t])
if test $ac_cv_type_socklen_t != yes; then
    AC_MSG_CHECKING([elsewhere for socklen_t type])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <winsock2.h>
		#include <ws2tcpip.h>]],
     [[socklen_t len = 42; return 0;]])],
     [ac_cv_type_socklen_t=yes],[ac_cv_type_socklen_t=no])
    AC_MSG_RESULT([$ac_cv_type_socklen_t])
fi
if test $ac_cv_type_socklen_t != yes; then
    AC_MSG_CHECKING([for socklen_t equivalent])
    AC_CACHE_VAL([ac_cv_socklen_t_equiv],
    [
	# Systems have either "struct sockaddr *" or
	# "void *" as the second argument to getpeername
	ac_cv_socklen_t_equiv=
	for arg2 in "struct sockaddr" void; do
	  for t in int size_t unsigned long "unsigned long"; do
	    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
		#include <sys/types.h>
		#include <sys/socket.h>
		int getpeername (int, $arg2 *, $t *);
	    ]], [[
		$t len;	getpeername(0,0,&len);
	    ]])],[
		ac_cv_socklen_t_equiv="$t"
		break
	    ],[])
	  done
	done
	if test "x$ac_cv_socklen_t_equiv" = x; then
	  AC_MSG_ERROR([Cannot find a type to use in place of socklen_t])
	fi
    ])
    AC_MSG_RESULT($ac_cv_socklen_t_equiv)
    AC_DEFINE_UNQUOTED(socklen_t, $ac_cv_socklen_t_equiv,
		       [type to use in place of socklen_t if not defined])
else
    AC_DEFINE(HAVE_SOCKLEN_T, 1, [if socklen_t is defined, make note of it])
fi

#CONFIG is only used for irix packaging
case $host_os in
    irix*)
	CONFIG=irix-mips3;
	AC_SUBST(CONFIG)
	;;
esac

AC_LANG(C++)
AC_CHECK_TYPES([std::shared_ptr<int>],
	[# BZFlag expects std::shared_ptr support by default],
	[AC_CHECK_TYPES([[std::tr1::shared_ptr<int>]],
		AC_DEFINE([USE_TR1], [1], [Define to 1 to use C++0X TR1]),
		AC_MSG_ERROR([[The C++11 std::shared_ptr type is required to build BZFlag]]),
		[[#include <tr1/memory>]])],
	[#include <memory>])

have_gl=yes
savedLIBS=$LIBS

case $host_os in
    solaris*)
	if test -d /usr/demo/SOUND/lib ; then
		GLIBS="-laudio $GLIBS"
		LDFLAGS="$LDFLAGS -L/usr/demo/SOUND/lib"
	fi
	GLIBS="-lresolv $GLIBS"
	LIBS="$LIBS -lsocket -lnsl -lresolv"
	LDFLAGS="$LDFLAGS -lresolv"	# for hstrerror()
	;;
    irix*)
	GLIBS="-lXsgivc -lX11 -laudio $GLIBS"
	;;
    beos)
	GLIBS=" -lmedia -lgame $GLIBS"
	LIBS="-lbe"
	;;
    darwin*)
        GLIBS="-framework OpenGL $GLIBS"
        ;;
    *)
	;;
esac

case $host_os in
    darwin*)
        AC_CHECK_HEADER([OpenGL/gl.h], [ac_cv_search_glBegin="none required"], [have_gl=no ; ac_cv_search_glBegin=no])
        AC_CHECK_HEADER([OpenGL/glu.h], [ac_cv_search_gluScaleImage="none required"], [have_gl=no ; ac_cv_search_gluScaleImage=no])
        AC_CHECK_HEADER([GL/glew.h], AC_SEARCH_LIBS(glewInit, [GLEW], [test $ac_cv_search_glewInit = "none required" || GLIBS="$ac_cv_search_glewInit $GLIBS"], [have_gl=no], $GLIBS), [have_gl=no ; ac_cv_search_glewInit=no])

        ;;
    *)
        AC_CHECK_HEADER([GL/gl.h], AC_SEARCH_LIBS(glBegin, [opengl32 GL GL2], [test $ac_cv_search_glBegin = "none required" || GLIBS="$ac_cv_search_glBegin $GLIBS"], [have_gl=no]), [have_gl=no ; ac_cv_search_glBegin=no])
        AC_CHECK_HEADER([GL/glu.h], AC_SEARCH_LIBS(gluScaleImage, [glu32 GL GLU], [test $ac_cv_search_gluScaleImage = "none required" || GLIBS="$ac_cv_search_gluScaleImage $GLIBS"], [have_gl=no]), [have_gl=no ; ac_cv_search_gluScaleImage=no])
        AC_CHECK_HEADER([GL/glew.h], AC_SEARCH_LIBS(glewInit, [GLEW], [test $ac_cv_search_glewInit = "none required" || GLIBS="$ac_cv_search_glewInit $GLIBS"], [have_gl=no]), [have_gl=no ; ac_cv_search_glewInit=no])
        ;;
esac

if test "$have_gl" != no; then
    if test "$ac_cv_search_glBegin" = -lGL2; then
	AC_DEFINE(BEOS_USE_GL2, 1, [Use new GL Kit for BeOS])
    fi
fi

LIBS=$savedLIBS
AC_SUBST(GLIBS)

# try out pthreads if it's enabled, disable it if we don't have it
AC_MSG_CHECKING([for working pthreads])
if test "x$enable_threads" != "xno" ; then
    tmpLIBS=$LIBS
    LIBS=$LIBPTHREAD
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <pthread.h>]],
				    [[pthread_t thread;
				      pthread_create(&thread, NULL, NULL, NULL);]])],
		   [pthread_check="yes"], [pthread_check="no"])
    if test $pthread_check = "yes"; then
	AC_DEFINE(HAVE_PTHREADS, 1, [posix-compliant threading is available])
	AC_DEFINE(_REENTRANT, 1, [Enable reentrant code])
	LIBS="$tmpLIBS $LIBS"
    else
	LIBS=$tmpLIBS
    fi
    AC_MSG_RESULT([$pthread_check])
fi

# Use OS X frameworks (required for all three executables; bzflag, bzfs, and bzadmin)
case $host_os in
    darwin*)
        LIBS="${LIBS} -framework Carbon -framework CoreFoundation"
        ;;
esac

case $host_os in
    mingw32*)
	AC_CHECK_LIB(ws2_32, closesocket)
	AC_CHECK_LIB(winmm, timeGetTime)

	GLIBS="-mwindows -lgdi32 -ldxguid -ldsound -ldinput $GLIBS"
	dnl MingW does some funny stuff with DirectX.
	dnl Test for a working DirectInput.
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>
    ]], [[
IDirectInput7* directInput;
HINSTANCE hinst = GetModuleHandle(NULL);
HRESULT success = DirectInputCreateEx(hinst, DIRECTINPUT_VERSION, IID_IDirectInput7, (void**)&directInput, NULL);
]])],[],[AC_DEFINE(BROKEN_DINPUT, 1, [Define to 1 if your DirectInput headers and libraries are broken or mismatched.])])
	;;
esac

# Remove ogg/vorbis dependencies until we actually need them.
#
# AC_CHECK_LIB(ogg, ogg_stream_init, [ALIBS="-logg $ALIBS"], [], $ALIBS)
# AC_CHECK_LIB(vorbis, vorbis_info_init, [ALIBS="-lvorbis $ALIBS"], [], $ALIBS)
# AC_CHECK_LIB(vorbisfile, ov_open, [ALIBS="-lvorbisfile $ALIBS"], [], $ALIBS)
# AC_SUBST(ALIBS)

if test $prefix = NONE; then
    prefix=$ac_default_prefix
fi
BZFLAG_DATA=`eval eval echo $datadir/bzflag`
AC_SUBST(BZFLAG_DATA)
AC_DEFINE_UNQUOTED(BZFLAG_DATA, "$BZFLAG_DATA", [Data file directory])

savedLIBS=$LIBS
LIBS="$GLIBS $LIBS"
AC_LANG(C)
if test "x$enable_client" != xno; then
    # Test for SDL 2
    AM_PATH_SDL2(2.0.9, with_SDL2=yes, with_SDL2=no)

    if test "x$with_SDL2" != "xyes"; then
        AC_MSG_WARN(
        [Client build is enabled, but SDL2 does not seem to be fully available ... disabling BZFlag client])
        AM_CONDITIONAL(CLIENT_INCLUDED, false)
        enable_client=no
    fi
fi
AC_LANG(C++)
LIBS=$savedLIBS

# dumb temp hacks to add missing linkage stuff
case $host_os in
     irix)
	if test x$host_cpu = xmips64; then
	   LDFLAGS="$LDFLAGS -L$(ROOT)/usr/lib32"
	else
	   LDFLAGS="$LDFLAGS -L$(ROOT)/usr/lib"
	fi
	;;
esac


dnl ******************************************
dnl *** Check for compiler characteristics ***
dnl ******************************************

BZ_CONFIGURE_STAGE([compiler], [6 of 9])

dnl On SGI systems, figure out if we are using MIPSPro compiler
using_mipspro=no
if test "$build_vendor" = "sgi" ; then
    AC_MSG_CHECKING([whether we are using the MIPSPro compiler])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
		      [[#if defined(sgi) && !defined(__GNUC__) && defined(_COMPILER_VERSION)
this line should cause a compile failure when building with the MIPSpro compiler;
#endif
int main() { return 0; }
    ]])],[using_mipspro=no],[using_mipspro=yes])
    AC_MSG_RESULT([$using_mipspro])
fi
AM_CONDITIONAL(BUGGY_MIPSPRO, test "x$using_mipspro" = "xyes")

# FIXME: these checks below are not compiler characteristics other
# than the FLAGS they set.
#
# the GCC version is known to be at least 4.3 because it has C++0x support
# possible future warnings: -Wconversion (1894) -Wfloat-equal (317)
FLAGS="-Wall -Wextra -Wcast-qual -Wredundant-decls -Wshadow -Wundef -pedantic"
case $host_os in
     linux*)
	AC_DEFINE(HALF_RATE_AUDIO, 1, [Half rate Audio])
	CONF_CFLAGS="$CONF_CFLAGS $FLAGS"
	CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS"
	case $host_vendor in
	     pc)
               FLAGS=-mtune=native
	       CONF_CFLAGS="$CONF_CFLAGS $FLAGS";
	       CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS";
	       CONF_CXXFLAGS="$CONF_CXXFLAGS -fsigned-char";;
	     ppc)
	       FLAGS="-mcpu=$host_cpu"
	       CONF_CFLAGS="$CONF_CFLAGS $FLAGS";
	       CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS";;
	esac;;
     cygwin)
	AC_DEFINE(HALF_RATE_AUDIO, 1, [Half rate Audio])
	FLAGS="-Wall -W -mcpu=$host_cpu"
	CONF_CFLAGS="$CONF_CFLAGS $FLAGS";
	CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS";
	CONF_CXXFLAGS="$CONF_CXXFLAGS -fsigned-char";;
     mingw32*)
	AC_DEFINE(HALF_RATE_AUDIO, 1, [Half rate Audio])
	FLAGS="-Wall -W"
	FLAGS="$FLAGS -mtune=$host_cpu"
	CONF_CFLAGS="$CONF_CFLAGS $FLAGS";
	CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS";
	CONF_CXXFLAGS="$CONF_CXXFLAGS -fsigned-char";;
     solaris*)
	AC_DEFINE(ETC_INET, [], [hosts is in /etc/inet/])
	AC_DEFINE(SUN_OGL_NO_VERTEX_MACROS, [], [Sun OpenGL No Macro Vertex])
	if test "$GCC" = yes ; then
	    FLAGS="$FLAGS -Wno-unknown-pragmas"	# *silently* ignore "#pragma ident"
	    CONF_CFLAGS="$CONF_CFLAGS $FLAGS"
	    CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS"
	fi
	;;
     irix)
	FLAGS="-Wall";
	if test x$host_cpu = xmips64; then
	   FLAGS="FLAGS -march=mips3";
	else
	   FLAGS="FLAGS -march=mips2";
	fi
	CONF_CFLAGS="$CONF_CFLAGS $FLAGS";
	CONF_CXXFLAGS="$CONF_CXXFLAGS $FLAGS";;
     beos*)
	AC_DEFINE(HAVE_DEFINED_TOLOWER, 1, [tolower and toupper are not functions])
	;;
esac

if test x$enable_profiling = xyes ; then
    if test "$GCC" = yes ; then
	CONF_CFLAGS="$CONF_CFLAGS -pg"
	CONF_CXXFLAGS="$CONF_CXXFLAGS -pg"
	if expr `$CC -dumpversion | sed 's/\..*//'` \>= "6" > /dev/null ; then
           CONF_CFLAGS="$CONF_CFLAGS -no-pie"
           CONF_CXXFLAGS="$CONF_CXXFLAGS -no-pie"
        fi
    else
	AC_MSG_WARN([profiling is disabled with non-GNU compiler])
    fi
fi

if test x$enable_debug = xyes ; then
    CONF_CFLAGS="$CONF_CFLAGS -Werror -g -O0"
    CONF_CXXFLAGS="$CONF_CXXFLAGS -Werror -g -O0"
    AC_DEFINE(DEBUG, 1, [Debugging enabled])
else
    if test "$GCC" = yes ; then
	case "`$CC --version 2>&1`" in
	    *LLVM*|lcc*|clang*)
		# Apple LLVM and MCST LCC falsely claim to be GCC
		;;
	    *)
		OPTIMIZE="-fexpensive-optimizations"
		CONF_CFLAGS="$CONF_CFLAGS $OPTIMIZE"
		CONF_CXXFLAGS="$CONF_CXXFLAGS $OPTIMIZE"
		;;
	esac
    fi
    CONF_CFLAGS="$CONF_CFLAGS -O2"
    CONF_CXXFLAGS="$CONF_CXXFLAGS -O2"
    AC_DEFINE(NDEBUG, 1, [Debugging disabled])
fi

# revert automatic setting of CFLAGS and CXXFLAGS to prevent
# override of debug or optimization settings in CONF_CFLAGS
# and CONF_CXXFLAGS above
CFLAGS="$user_CFLAGS"
CXXFLAGS="$user_CXXFLAGS"

dnl check for -search_paths_first linker flag when making dynamic libraries
search_paths_first_flag="-Wl,-search_paths_first -mdynamic-no-pic"
AC_MSG_CHECKING([if the compiler understands $search_paths_first_flag])
search_paths_first_flag_works=yes
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $search_paths_first_flag"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[],
	       [search_paths_first_flag_works=no])
AC_MSG_RESULT($search_paths_first_flag_works)
if test "x$search_paths_first_flag_works" = "xno" ; then
    LDFLAGS="$PRELDFLAGS"
fi

dnl ***************************
dnl *** Check for functions ***
dnl ***************************

BZ_CONFIGURE_STAGE([functions], [7 of 9])

ac_func_search_save_LIBS=$LIBS
case $host_os in
    darwin*)
LIBS="-framework OpenGL $ac_func_search_save_LIBS"
	;;
esac
AC_CHECK_FUNCS(\
	CGLGetCurrentContext \
	Sleep \
	WaitForSingleObject \
	_stricmp \
	_strnicmp \
	_vsnprintf \
	atexit \
	hstrerror \
	sched_setaffinity \
	select \
	snooze \
	usleep \
	vsnprintf \
	wglGetCurrentContext \
)
LIBS=$ac_func_search_save_LIBS

dnl test for isnan() presence
BZ_FUNC_ISNAN

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <algorithm>
]], [[
int i = std::min(0, 1); i = i;
]])],[AC_DEFINE(HAVE_STD__MIN, 1, [Define to 1 if you have a conforming std::min])],[])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <algorithm>
]], [[
int i = std::max(0, 1); i = i;
]])],[AC_DEFINE(HAVE_STD__MAX, 1, [Define to 1 if you have a conforming std::max])],[])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <algorithm>
]], [[
char array[] = "test";
int i = std::count(array, array + 5, 't'); i = i;
]])],[AC_DEFINE(HAVE_STD__COUNT, 1, [Define to 1 if you have a conforming
std::count, otherwise old version of count template is assumed])],[])


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

BZ_CONFIGURE_STAGE([services], [8 of 9])

# FIXME: system service checks seem to be spread out everywhere above,
# need to consolidate them to here.

case $host_os in
    beos*)
	beos=true
	;;
    darwin*)
        apple=true
	;;
    hpux*)
	hpux=true
	;;
    irix*)
	irix=true
	;;
    linux*|kfreebsd*|freebsd*|netbsd*|openbsd*|dragonfly*|gnu*)
	linux=true
	;;
    solaris*)
	solaris=true
	;;
    cygwin|mingw32*|windows**)
	win32=true
	;;
esac
AM_CONDITIONAL(HPUX, test x$hpux = xtrue)
AM_CONDITIONAL(BEOS, test x$beos = xtrue)
AM_CONDITIONAL(APPLE, test x$apple = xtrue)
AM_CONDITIONAL(IRIX, test x$irix = xtrue)
AM_CONDITIONAL(LINUX, test x$linux = xtrue)
AM_CONDITIONAL(SOLARIS, test x$solaris = xtrue)
AM_CONDITIONAL(WIN32, test x$win32 = xtrue)

# wtf are these for?
AM_CONDITIONAL(PC,    test x$host_vendor = xpc)
AM_CONDITIONAL(PPC,   test x$host_vendor = xppc)

# Check for working FF_RUMBLE support in linux/input.h
AC_MSG_CHECKING([for linux rumble joystick support])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <linux/input.h>]],
	[[struct ff_effect x; x.u.rumble.weak_magnitude = 42; return 0;]])],
	[ac_ff_effect_rumble=yes],[ac_ff_effect_rumble=no])
if test x$ac_ff_effect_rumble = xyes; then
    AC_DEFINE(HAVE_FF_EFFECT_RUMBLE, 1, [we appear to have working support for rumble force feedback effects])
fi
AC_MSG_RESULT([$ac_ff_effect_rumble])

# Check for working directional FF support in linux/input.h
AC_MSG_CHECKING([for linux force feedback joystick support])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <linux/input.h>]],
	[[struct ff_effect x; x.direction = 0x4000; return 0;]])],
	[ac_ff_effect_directional=yes],[ac_ff_effect_directional=no])
if test x$ac_ff_effect_directional = xyes; then
    AC_DEFINE(HAVE_FF_EFFECT_DIRECTIONAL, 1, [we appear to have working support for directional force feedback effects])
fi
AC_MSG_RESULT([$ac_ff_effect_directional])

# if the client was enabled, make sure we have GL
if test x$enable_client != xno; then
    if test "$have_gl" = no; then
	AC_MSG_WARN(
	[Client build is enabled, but OpenGL does not seem to be fully available ... disabling BZFlag client])
	AM_CONDITIONAL(CLIENT_INCLUDED, false)
	enable_client=no
    else
	AM_CONDITIONAL(CLIENT_INCLUDED, true)
    fi
else
    AM_CONDITIONAL(CLIENT_INCLUDED, false)
fi


# Build the server, or do not.  There is no "try."
if test x$enable_server != xno; then
    AM_CONDITIONAL(SERVER_INCLUDED, true)
else
    AM_CONDITIONAL(SERVER_INCLUDED, false)
fi


# if bzadmin was enabled, make sure we have curses.
if test x$enable_bzadmin != xno; then
    if test "x$CURSES_LIB" = x; then
      AC_MSG_WARN([could not find a curses library, will build bzadmin without curses])
    fi
    AM_CONDITIONAL(BZADMIN_INCLUDED, true)
else
    AM_CONDITIONAL(BZADMIN_INCLUDED, false)
fi
AM_CONDITIONAL(HAVE_CURSES, test "x$CURSES_LIB" != x)
AC_SUBST(CURSES_LIB)


# make sure ECHO and ECHO_N got defined and substituted
if test "x$ECHO" = "x" ; then
    ECHO=echo
    AC_MSG_NOTICE([ECHO was not defined by configure so defining manually])
fi
AC_SUBST(ECHO)
AC_SUBST(ECHO_N)


dnl **************************************
dnl *** Configure Makefiles and output ***
dnl **************************************

BZ_CONFIGURE_STAGE([output], [9 of 9])

AC_SUBST(CONF_CPPFLAGS)
AC_SUBST(CONF_CFLAGS)
AC_SUBST(CONF_CXXFLAGS)

LIBDIR="\$(top_srcdir)/lib"
AC_SUBST(LIBDIR)

AC_CONFIG_FILES([
	MSVC/Makefile
	MSVC/build/Makefile
	Xcode/Makefile
	Xcode/BZFlag.xcodeproj/Makefile
	Makefile
	bzflag.spec
	data/Makefile
	data/fonts/Makefile
	data/l10n/Makefile
	include/Makefile
	m4/Makefile
	man/Makefile
	misc/Makefile
	package/Makefile
	package/irix/Makefile
	package/linux/Makefile
	package/rpm/Makefile
	package/win32/Makefile
	package/win32/nsis/Makefile
	plugins/HoldTheFlag/Makefile
	plugins/Makefile
	plugins/Phoenix/Makefile
	plugins/RogueGenocide/Makefile
	plugins/SAMPLE_PLUGIN/Makefile
	plugins/airspawn/Makefile
	plugins/autoFlagReset/Makefile
	plugins/bzfscron/Makefile
	plugins/chathistory/Makefile
	plugins/customflagsample/Makefile
	plugins/customPollTypeSample/Makefile
	plugins/CustomZoneSample/Makefile
	plugins/fairCTF/Makefile
	plugins/fastmap/Makefile
	plugins/flagStay/Makefile
	plugins/keepaway/Makefile
	plugins/killall/Makefile
	plugins/koth/Makefile
	plugins/logDetail/Makefile
	plugins/nagware/Makefile
	plugins/playHistoryTracker/Makefile
	plugins/plugin_utils/Makefile
	plugins/rabbitTimer/Makefile
	plugins/rabidRabbit/Makefile
	plugins/recordmatch/Makefile
	plugins/regFlag/Makefile
	plugins/serverControl/Makefile
	plugins/serverSidePlayerSample/Makefile
	plugins/shockwaveDeath/Makefile
	plugins/superUser/Makefile
	plugins/teamflagreset/Makefile
	plugins/thiefControl/Makefile
	plugins/timedctf/Makefile
	plugins/TimeLimit/Makefile
	plugins/wwzones/Makefile
	src/3D/Makefile
	src/Makefile
	src/bzadmin/Makefile
	src/bzflag/Makefile
	src/bzfs/Makefile
	src/common/Makefile
	src/date/Makefile
	src/game/Makefile
	src/geometry/Makefile
	src/geometry/models/Makefile
	src/geometry/models/tank/Makefile
	src/mediafile/Makefile
	src/net/Makefile
	src/obstacle/Makefile
	src/ogl/Makefile
	src/platform/Makefile
	src/scene/Makefile
	tools/Makefile
	tools/TextTool2/Makefile
	tools/TextTool2/TextTool2.xcodeproj/Makefile
	tools/bzwtransform/Makefile
	tools/modeltool/Makefile
	tools/modeltool/MSVC/Makefile
])
AC_OUTPUT

dnl # AC_OUTPUT won't update the config.h.in template timestamp if the
dnl # file is unchanged.  forcibly update the timestamp so autoheader
dnl # doesn't try to invoke.
dnl if test ! "x`cat include/config.h.in`" = "x" ; then
dnl     touch include/config.h.in
dnl fi



dnl **********************
dnl *** Report Summary ***
dnl **********************

if test "x$enable_client" = "xno"; then
    build_bzflag="no bzflag client binary!"

    if test "x$have_gl" = "xno" ; then
        if test "x$ac_cv_search_glBegin" = xno ; then
            build_bzflag="$build_bzflag (missing OpenGL)"
        else
            if test "x$ac_cv_search_gluScaleImage" = xno ; then
                build_bzflag="$build_bzflag (missing GLU)"
            fi

            if test "x$ac_cv_search_glewInit" = xno ; then
                build_bzflag="$build_bzflag (missing GLEW)"
            fi
        fi
    fi

    if test "x$with_SDL2" != "xyes" ; then
        build_bzflag="$build_bzflag (missing SDL2)"
    fi
else
    build_bzflag="yes"
fi
if test "x$enable_server" != xno ; then
    if test "x$enable_plugins" != xno ; then
	build_bzfs="yes (with plugins)"
    else
	build_bzfs="yes (without plugins)"
    fi
else
	build_bzfs="no (without plugins)"
fi

if test "x$enable_bzadmin" = "xno"; then
    build_bzadmin="no bzadmin binary!"
else
    build_bzadmin="yes"
    if test "x$CURSES_LIB" != x; then
	build_bzadmin="$build_bzadmin (with curses)"
    fi
fi

dnl
dnl Expand the variables for summary reporting
dnl
prefix=`eval "echo $prefix"`
prefix=`eval "echo $prefix"`
bindir=`eval "echo $bindir"`
bindir=`eval "echo $bindir"`
libdir=`eval "echo $libdir"`
libdir=`eval "echo $libdir"`
mandir=`eval "echo $mandir"`
mandir=`eval "echo $mandir"`

dnl use AC_MSG_RESULT instead of AC_MSG_NOTIFY to support old autoconf
AC_MSG_RESULT([Done.])
AC_MSG_RESULT([])
BZ_BOLD
AC_MSG_RESULT([$PACKAGE_STRING configured on $CONF_DATE with the following:])
BZ_UNBOLD
AC_MSG_RESULT([])
AC_MSG_RESULT([             Prefix: ${prefix}])
AC_MSG_RESULT([           Binaries: ${bindir}])
AC_MSG_RESULT([            Plugins: ${libdir}])
AC_MSG_RESULT([       Manual pages: ${mandir}])
AC_MSG_RESULT([])
AC_MSG_RESULT([BUILD_DATE    = ${BUILD_DATE}])
AC_MSG_RESULT([CC            = ${CC}])
AC_MSG_RESULT([CXX           = ${CXX}])
AC_MSG_RESULT([CFLAGS        = ${CFLAGS}])
AC_MSG_RESULT([CONF_CFLAGS   = ${CONF_CFLAGS}])
AC_MSG_RESULT([CXXFLAGS      = ${CXXFLAGS}])
AC_MSG_RESULT([CONF_CXXFLAGS = ${CONF_CXXFLAGS}])
AC_MSG_RESULT([CPPFLAGS      = ${CPPFLAGS}])
AC_MSG_RESULT([CONF_CPPFLAGS = ${CONF_CPPFLAGS}])
AC_MSG_RESULT([LDFLAGS       = ${LDFLAGS}])
AC_MSG_RESULT([LIBS          = ${LIBS}])
AC_MSG_RESULT([GLIBS         = ${GLIBS}])
AC_MSG_RESULT([])
AC_MSG_RESULT([BZFlag client .....: $build_bzflag])
AC_MSG_RESULT([BZFlag server .....: $build_bzfs])
AC_MSG_RESULT([BZAdmin client ....: $build_bzadmin])
AC_MSG_RESULT([])
AC_MSG_RESULT([---])
AC_MSG_RESULT([$0 complete, type 'make' to begin building])
AC_MSG_RESULT([])

# Local Variables:
# tab-width: 8
# mode: autoconf
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
