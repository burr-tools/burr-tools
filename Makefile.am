SUBDIRS = src

EXTRA_DIST =
EXTRA_DIST += doc_src
EXTRA_DIST += doxygen.cfg.in
EXTRA_DIST += src/lib/tabs_0/generator_0.cpp src/lib/tabs_0/generator_mesh_0.cpp
EXTRA_DIST += src/lib/tabs_1/generator_1.cpp src/lib/tabs_1/generator_mesh_1.cpp
EXTRA_DIST += src/lib/tabs_2/generator_2.cpp
EXTRA_DIST += src/lib/tabs_3/generator_mesh_3.cpp
EXTRA_DIST += src/lib/tabs_4/generator_mesh_4.cpp
EXTRA_DIST += doc/convert.sh

dist_doc_DATA =
dist_doc_DATA += examples/README examples/AlPackino.xmpuzzle examples/DraculasDentalDesaster.xmpuzzle
dist_doc_DATA += examples/PelikanBurr.xmpuzzle examples/CubeInCage.xmpuzzle examples/Bermuda.xmpuzzle
dist_doc_DATA += examples/BallRoom.xmpuzzle examples/SolidSixPieceBurrs.xmpuzzle
dist_doc_DATA += examples/12PieceSeparation.xmpuzzle
dist_doc_DATA += examples/AugmentedSecondStellation.xmpuzzle
dist_doc_DATA += examples/BrokenSticks.xmpuzzle
dist_doc_DATA += examples/DiagonalCube.xmpuzzle
dist_doc_DATA += examples/HexSticks.xmpuzzle
dist_doc_DATA += examples/PermutatedThirdStellation.xmpuzzle
dist_doc_DATA += examples/PiecesOfEight.xmpuzzle
dist_doc_DATA += examples/Prisgon.xmpuzzle
dist_doc_DATA += examples/FourPieceTetrahedron.xmpuzzle
dist_doc_DATA += examples/DemoMirrorParadox.xmpuzzle
dist_doc_DATA += examples/DemoPieceGenerator.xmpuzzle
dist_doc_DATA += burricons.ico
dist_doc_DATA += AUTHORS COPYING README NEWS ChangeLog COPYRIGHT

abs_builddir = @abs_builddir@
win32dir = $(PACKAGE)-$(VERSION)-win32

dist-hook:
	rm -rf $(distdir)/doc_src/*~

dist-win32: all
	$(RM) -r $(win32dir)
	$(INSTALL) -d                        $(win32dir)
	$(INSTALL) -d                        $(win32dir)/examples
	$(INSTALL) -m755 src/gui/burrGui.exe $(win32dir)/
	$(STRIP) $(win32dir)/*.exe
	upx --lzma $(win32dir)/*.exe
	cd $(srcdir) && for FILE in $(dist_doc_DATA); do case $$FILE in \
	    *.xmpuzzle) $(INSTALL) -m644 $$FILE   $(abs_builddir)/$(win32dir)/examples/ ;; \
	    *.*)        $(INSTALL) -m644 $$FILE   $(abs_builddir)/$(win32dir)/ ;; \
	    *)          sed 's,$$,\r,' < $$FILE > $(abs_builddir)/$(win32dir)/$$FILE.txt ;; \
	    esac; done
	$(RM)   $(win32dir).zip
	zip -r9 $(win32dir).zip $(win32dir)
	$(RM) -r $(win32dir)

Makefile: Makefile.am configure.ac src/Makefile.am
	./bootstrap
