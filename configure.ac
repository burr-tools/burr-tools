AC_INIT(burrTools, 0.1.0)

AC_CONFIG_SRCDIR(src/tester.cpp)
AC_CONFIG_AUX_DIR(admin)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(src/config.h)

AC_PROG_RANLIB

AC_PROG_CXX

AC_CONFIG_FILES([Makefile src/Makefile src/lib/Makefile src/gui/Makefile])

AC_CHECK_PROG(fltk_found, fltk-config, 1, 0)

if [test $fltk_found -eq 1]; then
  GUI_CPP_ADD=`fltk-config --cxxflags --use-gl`
  GUI_LD_ADD=`fltk-config --ldflags --use-gl`
else
  AC_MSG_WARN([FLTK not found, do not build gui])
fi  

AC_CHECK_PROG(flu_found, flu-config, 1, 0)

if [test $flu_found -eq 1]; then
  GUI_CPP_ADD="$GUI_CPP_ADD `flu-config --cxxflags --use-gl`"
  GUI_LD_ADD="$GUI_LD_ADD `flu-config --ldflags --use-gl`"
  AC_DEFINE(HAVE_FLU)
fi

AC_CHECK_PROG(xmlwrapp_found, xmlwrapp-config, 1, 0)

if [test $xmlwrapp_found -eq 1]; then
  XML_CPP_ADD=`xmlwrapp-config --cxxflags`
  XML_LD_ADD=`xmlwrapp-config --libs`
else
  AC_MSG_WARN([XMLwrapp not found, required for the library])
fi  

AC_SUBST(GUI_CPP_ADD, $GUI_CPP_ADD)
AC_SUBST(GUI_LD_ADD, $GUI_LD_ADD)
AC_SUBST(XML_CPP_ADD, $XML_CPP_ADD)
AC_SUBST(XML_LD_ADD, $XML_LD_ADD)

AC_OUTPUT
