# Microsoft Developer Studio Project File - Name="burrTools" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=burrTools - Win32 Release
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "burrTools.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "burrTools.mak" CFG="burrTools - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "burrTools - Win32 Release" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE "burrTools - Win32 Debug" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "burrTools - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\libs\fltk-1.1.5\\" /I "..\..\libs\flu-2.14\\" /I "..\src\lib" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "HAVE_FLU" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ..\..\libs\flu-2.14\lib\flulib.lib  ..\..\libs\fltk-1.1.5\lib\fltk.lib ..\..\libs\fltk-1.1.5\lib\fltkgl.lib Release/burrlib.lib wsock32.lib opengl32.lib glu32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /pdb:none /machine:I386
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "burrTools - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "d:\priv\fltk-1.1.5\\" /I "..\src\lib" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 Debug/fltk.lib Debug/fltkgl.lib Debug/burrlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib opengl32.lib glu32.lib WSOCK32.LIB libc.lib libcp.lib msvcrt.lib oldnames.lib /nologo /subsystem:console /pdb:none /machine:I386 /nodefaultlib /out:"Debug/burrTools.exe "

!ENDIF 

# Begin Target

# Name "burrTools - Win32 Release"
# Name "burrTools - Win32 Debug"
# Begin Group "Quellcodedateien"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\gui\ArcBall.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\AssemblyCallbacks.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\DisasmToMoves.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\main.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\MainWindow.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\pieceColor.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\PieceSelector.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\PieceVisibility.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\SquareEditor.cpp
# End Source File
# Begin Source File

SOURCE=..\src\gui\VoxelView.cpp
# End Source File
# End Group
# Begin Group "Header-Dateien"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\gui\ArcBall.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\AssemblyCallbacks.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\DisasmToMoves.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\MainWindow.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\pieceColor.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\PieceSelector.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\PieceVisibility.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\SquareEditor.h
# End Source File
# Begin Source File

SOURCE=..\src\gui\VoxelView.h
# End Source File
# End Group
# Begin Group "Ressourcendateien"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
