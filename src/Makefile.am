SUBDIRS=lib gui

bin_PROGRAMS=tester burrTxt

XML_CPP_ADD=@XML_CPP_ADD@
XML_LD_ADD=@XML_LD_ADD@


tester_SOURCES=tester.cpp
tester_LDADD=lib/libburr.a

tester_CPPFLAGS=$(XML_CPP_ADD)
tester_LDFLAGS=$(XML_LD_ADD)


burrTxt_SOURCES=burrTxt.cpp
burrTxt_LDADD=lib/libburr.a

burrTxt_CPPFLAGS=$(XML_CPP_ADD)
burrTxt_LDFLAGS=$(XML_LD_ADD)

