bin_PROGRAMS=burrGui

GUI_CPP_ADD=@GUI_CPP_ADD@
GUI_LD_ADD=@GUI_LD_ADD@
burrGui_LDADD=-L../lib -lburr

burrGui_CPPFLAGS=$(GUI_CPP_ADD)
burrGui_LDFLAGS=$(GUI_LD_ADD)
burrGui_SOURCES=VoxelView.cpp VoxelView.h \
		ArcBall.cpp ArcBall.h \
                MainWindow.cpp MainWindow.h \
                WindowWidgets.cpp WindowWidgets.h \
                SquareEditor.cpp SquareEditor.h \
                PieceSelector.cpp PieceSelector.h \
                PieceVisibility.cpp PieceVisibility.h \
                DisasmToMoves.cpp DisasmToMoves.h \
                pieceColor.cpp pieceColor.h \
                AssemblyCallbacks.cpp AssemblyCallbacks.h \
                main.cpp

