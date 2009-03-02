AC_INIT(burrtools, 0.4.2)

AC_SUBST(FULLNAME,"Burr Tools")
AC_SUBST(URL,"http://burrtools.sourceforge.net/")
AC_DEFINE_UNQUOTED([FULLNAME],["$FULLNAME"],[Define to the full name of this package.])
AC_DEFINE_UNQUOTED([URL],["$URL"],[Define to the URL of this package's homepage.])

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

AC_CONFIG_FILES([Makefile src/Makefile burrtools.nsi])

case "$target_os" in
  mingw*)
    ;;
  *)
    ACX_PTHREAD(, AC_MSG_ERROR([pthread library required for burrtools]))
    ;;
esac

AC_ARG_WITH(fltk,
        [  --with-fltk             prefix of FLTK     (where to find bin/fltk-config)],
        FLTK_CONFIG_PATH=$withval/bin,
        FLTK_CONFIG_PATH=$PATH)
AC_PATH_PROG(FLTK_CONFIG, fltk-config, [], [$FLTK_CONFIG_PATH])
test -z "$FLTK_CONFIG" && AC_MSG_ERROR([unable to find FLTK])
GUI_CPP_ADD=`$FLTK_CONFIG --cxxflags --use-gl`
GUI_LD_ADD=`$FLTK_CONFIG --ldflags --use-gl`

GUI_HELP_CPP_ADD=`$FLTK_CONFIG --cxxflags --use-images`
GUI_HELP_LD_ADD=`$FLTK_CONFIG --ldflags --use-images`

AC_ARG_WITH(libpng,
        [  --with-libpng           prefix of libpng   (where to find bin/libpng-config)],
        LIBPNG_CONFIG_PATH=$withval/bin,
        LIBPNG_CONFIG_PATH=$PATH)
AC_PATH_PROG(LIBPNG_CONFIG, libpng-config, [], [$LIBPNG_CONFIG_PATH])
test -z "$LIBPNG_CONFIG" && AC_MSG_ERROR([unable to find libpng])
GUI_CPP_ADD="$GUI_CPP_ADD `$LIBPNG_CONFIG --cppflags`"
GUI_LD_ADD="$GUI_LD_ADD `$LIBPNG_CONFIG --libs`"

case "$target_os" in
  mingw*)
    ;;
  darwin*)
   AX_CHECK_GL
    ;;

  *)
    MDL_HAVE_OPENGL

    if test x$have_GL != xyes -o x$have_GLU != xyes; then
      AC_MSG_ERROR([no openGL found, openGL is needed for burrtools])
    fi
    ;;
esac

AC_CHECK_TOOL([NMPROG], [nm], [nm])

dnl fltk file function 64 bit usage
AC_MSG_CHECKING([whether FLTK file function use 64 bit structures])
which nm > /dev/null

if test $? != 0
then
  AC_MSG_RESULT([undeterminded, assuming yes])
else

  # check the libraries by searching for scandir64 symbol within them
  $NMPROG `fltk-config --libs` 2>/dev/null | grep scandir64 > /dev/null
  if test $? = 0
  then
    AC_MSG_RESULT([yes])

    AC_SYS_LARGEFILE

    dnl Define largefile options as needed...
    LARGEFILE=""
    if test x$enable_largefile != xno
    then
      LARGEFILE="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"

      if test $ac_cv_sys_large_files -a $ac_cv_sys_large_files = 1
      then
        LARGEFILE="$LARGEFILE -D_LARGE_FILES"
      fi

      if test $ac_cv_sys_file_offset_bits = 64
      then
        LARGEFILE="$LARGEFILE -D_FILE_OFFSET_BITS=64"
      fi
    fi
  else
    AC_MSG_RESULT([no])
    LARGEFILE=""
  fi
fi

AC_SUBST(LARGEFILE)

AC_SUBST(GUI_CPP_ADD, ["$GUI_CPP_ADD $GL_CFLAGS"])
AC_SUBST(GUI_LD_ADD, ["$GUI_LD_ADD $GL_LIBS"])
AC_SUBST(GUI_HELP_CPP_ADD, ["$GUI_HELP_CPP_ADD"])
AC_SUBST(GUI_HELP_LD_ADD, ["$GUI_HELP_LD_ADD"])
AC_SUBST(XML_CPP_ADD, $XML_CPP_ADD)
AC_SUBST(XML_LD_ADD, $XML_LD_ADD)
AC_SUBST(PTHREAD_CPP_ADD, $PTHREAD_CFLAGS)
AC_SUBST(PTHREAD_LD_ADD, ["$PTHREAD_CFLAGS $PTHREAD_LIBS"])

AC_OUTPUT
