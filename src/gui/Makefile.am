bin_PROGRAMS=burrGui

GUI_CPP_ADD=@GUI_CPP_ADD@
GUI_LD_ADD=@GUI_LD_ADD@
burrGui_LDADD=-L../lib -lburr

burrGui_CPPFLAGS=$(GUI_CPP_ADD) $(XML_CPP_ADD) -Wall
burrGui_LDFLAGS=$(GUI_LD_ADD) $(XML_LD_ADD)
burrGui_SOURCES=VoxelView.cpp VoxelView.h \
                ArcBall.cpp ArcBall.h \
                BlockList.cpp BlockList.h \
                MainWindow.cpp MainWindow.h \
                WindowWidgets.cpp WindowWidgets.h \
                SquareEditor.cpp SquareEditor.h \
                DisasmToMoves.cpp DisasmToMoves.h \
                pieceColor.cpp pieceColor.h \
                gzstream.cpp gzstream.h \
                Fl_Table.cpp Fl_Table.h \
                GroupsEditor.cpp GroupsEditor.h \
                PlacementBrowser.cpp PlacementBrowser.h \
                configuration.cpp configuration.h \
		Layouter.cpp Layouter.h \
		ImageExport.cpp ImageExport.h \
                main.cpp

