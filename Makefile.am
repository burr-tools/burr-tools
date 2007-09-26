SUBDIRS = src

EXTRA_DIST = doc/UserGuide.tm \
             doc/Pics/Button_Constraints_Colour.png doc/Pics/Button_Constraints_Fixed.png \
             doc/Pics/Button_Constraints_Variable.png doc/Pics/Button_Size_Grid_Align.png \
             doc/Pics/Button_Size_Grid_Center.png doc/Pics/Button_Size_Grid_Minimize.png \
             doc/Pics/Button_Size_Shape_11.png doc/Pics/Button_Size_Shape_x2.png \
             doc/Pics/Button_Size_Shape_x3.png doc/Pics/Button_Toolbar_Cols_X.png \
             doc/Pics/Button_Toolbar_Cols_Y.png doc/Pics/Button_Toolbar_Cols_Z.png \
             doc/Pics/Button_Toolbar_Drag_Free.png doc/Pics/Button_Toolbar_Drag_Rubber.png \
             doc/Pics/Button_Toolbar_Draw_Brush.png doc/Pics/Button_Toolbar_Draw_Eraser.png \
             doc/Pics/Button_Toolbar_Draw_Fixed.png doc/Pics/Button_Toolbar_Draw_Variable.png \
             doc/Pics/Button_Toolbar_Symm_X.png doc/Pics/Button_Toolbar_Symm_Y.png \
             doc/Pics/Button_Toolbar_Symm_Z.png doc/Pics/Button_Transform_Flip_X.png \
             doc/Pics/Button_Transform_Flip_Y.png doc/Pics/Button_Transform_Flip_Z.png \
             doc/Pics/Button_Transform_Nudge_X.png doc/Pics/Button_Transform_Nudge_Y.png \
             doc/Pics/Button_Transform_Nudge_Z.png doc/Pics/Button_Transform_Rotate_X.png \
             doc/Pics/Button_Transform_Rotate_Y.png doc/Pics/Button_Transform_Rotate_Z.png \
             doc/Pics/Form_Group_Editor.png doc/Pics/NavigationA.png \
             doc/Pics/NavigationB.png doc/Pics/Panel_Colour_Assignment_A.png \
             doc/Pics/Panel_Colour_Assignment_B.png \
             doc/Pics/RepresentationA.png doc/Pics/RepresentationB.png \
             doc/Pics/RepresentationC.png doc/Pics/RepresentationD.png \
             doc/Pics/RepresentationE.png doc/Pics/RepresentationF.png \
             doc/Pics/SizeBar.png \
             doc/Pics/SolverProgress.png doc/Pics/Subtab_Size.png \
             doc/Pics/Subtab_Tools.png \
             doc/Pics/Subtab_TransformA.png doc/Pics/Subtab_TransformB.png doc/Pics/Subtab_TransformC.png \
             doc/Pics/ToolbarGrid.png doc/Pics/Window_Clarissa.png \
             doc/Pics/Window_Config.png doc/Pics/Window_DDD_0.png \
             doc/Pics/Window_DDD_1.png doc/Pics/Window_DDD_2.png \
             doc/Pics/Window_DDD_3.png doc/Pics/Window_Placements.png \
             doc/Pics/Window_StartUp.png doc/Pics/Window_ImageExport.png \
             doc/Pics/Window_Status.png \
             doc/Pics/Emulation_2Layer.png \
             doc/Pics/Emulation_DiagonalCut.png \
             doc/Pics/Emulation_Pagoda2.png \
             doc/Pics/Emulation_Pagoda.png \
             doc/Pics/Window_Anaglyph.png

dist_doc_DATA = doc/UserGuide.pdf \
                examples/README examples/AlPackino.xmpuzzle examples/DraculasDentalDesaster.xmpuzzle \
                examples/PelikanBurr.xmpuzzle examples/CubeInCage.xmpuzzle examples/Bermuda.xmpuzzle \
                examples/BallRoom.xmpuzzle examples/SolidSixPieceBurrs.xmpuzzle \
                AUTHORS COPYING README NEWS ChangeLog

%.pdf: %.tm
	texmacs '$<' -x " \
	    (generate-all-aux) (update-buffer) (print-to-file \"$*.ps\") \
	    (generate-all-aux) (update-buffer) (print-to-file \"$*.ps\") \
	    (generate-all-aux) (update-buffer) (print-to-file \"$*.ps\") \
	    (quit-TeXmacs)"
	ps2pdf -dPDFSETTINGS=/prepress -dEncodeColorImages=false '$*.ps' '$@'

abs_builddir = @abs_builddir@
win32dir = $(PACKAGE)-$(VERSION)-win32

dist-win32: all
	$(RM) -r $(win32dir)
	$(INSTALL) -d                        $(win32dir)
	$(INSTALL) -d                        $(win32dir)/examples
	$(INSTALL) -m755 src/burrTxt.exe     $(win32dir)/
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
