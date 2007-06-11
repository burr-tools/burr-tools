AC_INIT(burrTools, 0.3.0)

AC_CONFIG_SRCDIR(src/burrTxt.cpp)
AC_CONFIG_AUX_DIR(admin)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_DEFINE_UNQUOTED(TARGET_CPU_$target_cpu)

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(src/config.h)

AC_PROG_RANLIB

AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O

AC_CONFIG_FILES([Makefile src/Makefile])

if [test "$target_os" != "mingw32"]; then
  ACX_PTHREAD(, AC_MSG_ERROR([pthread library required for burrtools]))
fi

AC_CHECK_PROG(fltk_found, fltk-config, 1, 0)

if [test $fltk_found -eq 1]; then
  GUI_CPP_ADD=`fltk-config --cxxflags --use-gl`
  GUI_LD_ADD=`fltk-config --ldflags --use-gl`
else
  AC_MSG_ERROR([FLTK not found, please install that libraray with development package])
fi  

AC_CHECK_PROG(xmlwrapp_found, xmlwrapp-config, 1, 0)

if [test $xmlwrapp_found -eq 1]; then
  XML_CPP_ADD=`xmlwrapp-config --cxxflags`
  XML_LD_ADD=`xmlwrapp-config --libs`
else
  AC_MSG_ERROR([XMLwrapp not found, please install that library])
fi  

AC_CHECK_PROG(libpng_found, libpng-config, 1, 0)

if [test $libpng_found -eq 1]; then
  GUI_CPP_ADD="$GUI_CPP_ADD `libpng-config --cppflags`"
  GUI_LD_ADD="$GUI_LD_ADD `libpng-config --libs`"
else
  AC_MSG_ERROR([libpng not found, please install that library])
fi  

AC_SYS_LARGEFILE

dnl Define largefile options as needed...
LARGEFILE=""
if test x$enable_largefile != xno; then
	LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"

	if test $ac_cv_sys_large_files -a $ac_cv_sys_large_files = 1; then
		LARGEFILE="$LARGEFILE -D_LARGE_FILES"
	fi

	if test $ac_cv_sys_file_offset_bits = 64; then
		LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"
	fi
fi
AC_SUBST(LARGEFILE)


AC_SUBST(GUI_CPP_ADD, $GUI_CPP_ADD)
AC_SUBST(GUI_LD_ADD, $GUI_LD_ADD)
AC_SUBST(XML_CPP_ADD, $XML_CPP_ADD)
AC_SUBST(XML_LD_ADD, $XML_LD_ADD)
AC_SUBST(PTHREAD_CPP_ADD, $PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_LD_ADD, $PTHREAD_LIBS)

AC_OUTPUT
