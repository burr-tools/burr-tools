SUBDIRS = src

EXTRA_DIST = doc

dist_doc_DATA = examples/README examples/AlPackino.xmpuzzle examples/DraculasDentalDesaster.xmpuzzle \
                examples/PelikanBurr.xmpuzzle examples/CubeInCage.xmpuzzle examples/Bermuda.xmpuzzle \
                examples/BallRoom.xmpuzzle examples/SolidSixPieceBurrs.xmpuzzle \
                examples/12PieceSeparation.xmpuzzle \
                examples/AugmentedSecondStellation.xmpuzzle \
                examples/BrokenSticks.xmpuzzle \
                examples/DiagonalCube.xmpuzzle \
                examples/HexSticks.xmpuzzle \
                examples/PermutatedThirdStellation.xmpuzzle \
                examples/PiecesOfEight.xmpuzzle \
                AUTHORS COPYING README NEWS ChangeLog

abs_builddir = @abs_builddir@
win32dir = $(PACKAGE)-$(VERSION)-win32

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
	makensis $(PACKAGE).nsi
	$(RM) -r $(win32dir)
