<TeXmacs|1.0.6>

<style|book>

<\body>
  <doc-data|<doc-title|BurrTools>|<\doc-author-data|<author-name|Andreas
  Röver, Ronald Kint-Bruynseels>>
    \;
  </doc-author-data|<author-email|roever@users.sf.net>>>

  <\table-of-contents|toc>
  </table-of-contents>

  <\with|par-mode|right>
    <prologue>\ 
  </with>

  <\with|par-mode|right>
    <em|to my mother> <em|(1950-2005)>
  </with>

  \;

  \;

  <no-indent>What are <name|BurrTools>? <name|BurrTools> contain two main
  parts. On the one hand there is a program that assembles and disassembles
  burr-type puzzles. That program contains a graphical user interface (GUI)
  which allows creation and editing puzzle definitions, solving the puzzle
  and the display and animation of the found solutions. This is probably the
  most interesting part for most people. On the other hand there is a
  <name|<name|<name|C++>>> library that may help with the search for and
  design of new puzzles. This library contains all the necessary tools to
  write programs that do the things that the graphical interface does (and
  more).

  The first part of this document describes the graphical program. It should
  contain descriptions of all concepts and explain how to use them in the GUI
  program. The second section will contain a description of the library and
  some internals. Also, some of the used algorithms are explained. This
  section is probably only interesting for people wanting to use the library
  for their own puzzle design explorations.

  But first a little bit of history of this program. There are already two
  programs that do the same that <name|BurrTools> can do. One is
  <name|BCPBox/Genda> written by Bill Cutler. Cutler's programs are very
  versatile, they even can handle space grids different from cubes. The other
  one is <name|PuzzleSolver3D> by André van Kammen. I had bought this program
  a while ago and have generally been quite satisfied with it. I have taken
  over quite some ideas from the GUI that André developed. So why another
  program you might ask. Here are a few reasons:

  <\enumerate-numeric>
    <item>The available programs are not for <name|Linux>, which is my
    operating system of choice,

    <item>the available programs are binary only programs and hence it is
    quite hard to do more interesting things like burr growing in an
    automated way,

    <item>the programs do cost money,

    <item><name|PuzzleSolver3D> seems to be abandoned. There hasn't been any
    update for quite a while, and

    <item><name|PuzzleSolver3D> has some nasty limits to the shape sizes and
    the number of possible placements.
  </enumerate-numeric>

  Anyway, I was not completely satisfied with the available software. Then in
  summer 2003 a German computer magazine, started a competition to write a
  program that counts the number of solutions to a merchandising puzzle of
  them as fast as possible. My program wasn't the fastest but it was the
  starting point for the <name|BurrTools>.

  As there are many people out there that are a lot more creative than I am
  and that could use a program like this to design nice puzzles I decided to
  make it public and free<\footnote>
    Free as in free speech and as in free beer (see
    <verbatim|http://www.gnu.org>)
  </footnote>.

  I added a GUI that can work on many operating systems, including
  <name|Linux> and <name|Windows>. This has the disadvantage that the GUI
  looks a bit different from what the normal <name|Windows> user is used to,
  so stay calm if things look a bit unusual, they behave in fact quite
  similar to how a normal <name|Windows>-program behaves.

  Lately 2 people played important roles in the development of the program.
  These 2 are Ronald Kint-Bruynseels and Derek Bosch. Ronald has rewritten
  the first part of this manual and has generally contributed lots of well
  organized suggestions. Derek is responsible for the OSX port of the
  program. Without him there would be no binary for this operating system
  available.

  I want to thank both of them for their work. I also want to thank all the
  other people that have sent in bug reports, suggestions and praise. Their
  input is very welcome and crucial to the further development of the
  program.

  All this work has taken nearly 3 years to reach the current state, I hope
  it was worth it and you have a lot of fun with the program.

  \;

  <\with|par-mode|right>
    Andreas Röver
  </with>

  <part|User Guide>

  <\with|par-mode|right>
    <chapter|Getting Started>
  </with>

  <section|Introduction>

  The first part of this user guide is written from a <em|procedural>
  approach. Rather than sequentially describing the elements of the GUI and
  their functions, this manual guides you through the program the way you
  should create your first design. Terms may be briefly repeated in several
  places of the text. Although too much redundancy has been avoided.

  Throughout the text the following fonts and notations are used:

  \;

  <\with|par-mode|left>
    <\with|par-mode|right>
      <tabular|<tformat|<cwith|1|7|1|1|cell-width|3cm>|<cwith|1|7|2|2|cell-width|12cm>|<cwith|1|8|1|1|cell-halign|r>|<cwith|9|9|1|1|cell-halign|r>|<cwith|1|9|2|2|cell-hyphen|t>|<table|<row|<cell|Roman>|<cell|This
      font is used for the main text.>>|<row|<cell|<em|Roman
      Italics>>|<cell|Italics are used to emphasis words or sentences in the
      main text.>>|<row|<cell|<strong|Roman Bold>>|<cell|Used for titles,
      subtitles and <em|concepts>.>>|<row|<cell|<with|font-family|ss|Sans
      Serif>>|<cell|Used for elements of the GUI. The same wording is used as
      in the GUI.>>|<row|<cell|<strong|<with|font-family|ss|Sans Serif
      Bold>>>|<cell|Used for elements of the GUI and indicating that an
      in-<no-break>depth explanation follows.>>|<row|<cell|<name|Small
      Capitals>>|<cell|Used for program names and puzzle
      names.>>|<row|<cell|<with|font-family|tt|Typewriter>>|<cell|Used for
      file names, directories, URL's and code
      examples.>>|<row|<cell|<with|font-family|tt|[Typewriter]>>|<cell|Between
      square brackets. Denotes a keyboard
      command.>>|<row|<cell|<with|mode|math|\<vartriangleright\>>>|<cell|Followed
      by a number. A reference to another part in the text that provides more
      detailed information on the subject. To be read as '<em|see (also)
      ...>'.>>>>>
    </with>
  </with>

  <section|Installing BurrTools>

  <subsection|Downloading BurrTools>

  <name|BurrTools> is an <em|Open Source> software project. The most recent
  release of the program is always available for <em|free> download at the
  <name|BurrTools> website:<next-line>

  <\with|par-mode|center>
    \ <verbatim|http://burrtools.sourceforge.net><line-break>
  </with>

  \;

  <no-indent>At the bottom of that page you can select the proper download
  for your operation system. This will bring you to the download page where
  you have to select a mirror site to start the downloading. It's highly
  recommended to select the mirror site on the server nearest to your
  location.

  <name|Microsoft Windows> users can either download the
  <with|font-family|tt|Windows Binary<name|<with|font-family|tt|>>> (a zipped
  file which needs manual extraction and installation) or the self-extracting
  <with|font-family|tt|Windows Installer>. Unless you have a slow connection
  to the internet downloading the installer is probably the best option. To
  use <name|BurrTools> on a <name|Linux> platform you can either download
  provided pre compiled version or the <with|font-family|tt|Source><name|>
  files and compile the program on your system (see installation guidelines
  below).

  <subsection|Installation of BurrTools>

  <subsubsection|Microsoft Windows>

  If you downloaded the <with|font-family|tt|Widows Binary>, just extract the
  file into the directory of your choice and the GUI is ready to be used.
  When you opted for the <with|font-family|tt|Windows Installer>, start the
  executable and follow the instructions on your screen.

  <subsubsection|Mac OS X>

  For detailed installation instructions please refer to the manual or help
  files of your operating system.

  <subsubsection|Linux / Unix>

  For <name|Linux> users <name|BurrTools> comes in two versions: a pre
  compiled binary and source code.

  The binary is provided in the hope that it is working on many variants of
  the <name|Linux> OS. It is compiled for Intel processors and requires a
  more or less modern <name|Linux> system. As distinct versions of
  <name|Unixes> differ widely it is likely that the binary will not work for
  your system. In that case you need to compile <name|BurrTools> yourself.

  <paragraph|Using the Pre compiled Binary>

  If you want to try the binary, just download the archive with the current
  version. Decompress the archive into a directory of your choice and start
  <verbatim|burrGui> within that directory. It either works or it doesn't.

  If it doesn't make sure you have at least the following libraries installed
  on your system: <verbatim|zlib>, <verbatim|libpng>, <verbatim|libxml2> and
  <verbatim|libxslt>. Of course you also need a working X windowing system.
  If the program still doesn't work call <verbatim|ldd burrGui> from the
  console within the path where you decompressed the files. This will list
  all libraries required by the binary and where the system could find them.
  If one of the listed binaries is not available try to install that. If all
  that doesn't work you should consider compiling on your own or mail me.

  <paragraph|Compiling from Source>

  These installation instructions just contain some hints for the compilation
  of <name|BurrTools>. As <name|BurrTools> requires a few not so widespread
  libraries it is not the easiest task to do this.

  To install <name|BurrTools> for <name|Unix> you first need to make sure you
  have the following libraries installed: <with|font-family|tt|zlib>,
  <with|font-family|tt|libpng>, <with|font-family|tt|libxml2> and
  <with|font-family|tt|libxslt>. These libraries are usually installed on
  every <name|Linux> system. You just have to make sure that you have
  installed the development packages, otherwise it is not possible to compile
  a program that use these libraries, but just start programs that use them.

  Additionally the following libraries are required:
  <with|font-family|tt|flkt>, <with|font-family|tt|flu> and
  <with|font-family|tt|xmlwrapp>.

  <verbatim|Fltk> is the library used for the GUI of <name|BurrTools>. It may
  be included in your <name|Linux> distribution or it may not.

  The problem is that we need a version of this library that is not compiled
  with the default switches. This library must be compiled with C++
  exceptions enabled. If you don't do this the program will simply shut down
  when an internal error occurs instead of displaying an error message and
  making an emergency save. To compile <with|font-family|tt|flkt> with
  exceptions enabled you have to do the following:

  <\itemize-dot>
    <item>Download and decompress as usual

    <item>Run <verbatim|configure> just as usual

    <item>Remove <verbatim|-fno-exceptions> from the file
    <verbatim|makeinclude>

    <item>Finish normally be calling <verbatim|make> and <samp|<verbatim|make
    install>>
  </itemize-dot>

  It is of course possible to use a normal version of the <verbatim|fltk>
  library, you just don't get the emergency save feature if there is a bug in
  the GUI of <name|BurrTools>. But as the number of bugs is hopefully quite
  small right now that should not be such a big problem.

  <verbatim|Flu> is not included in most distributions. So you need to
  install it manually. It is installed in the usual way. Refer to
  installation instructions of that library. <name|BurrTools> can be compiled
  without <verbatim|Flu> but then the File Open dialogue is really ugly.

  The last library, <verbatim|xmlwrapp>, can be hard to find, so here a link

  <\with|par-mode|center>
    <verbatim|xmlwrapp.sf.net>
  </with>

  This library is compiled and installed in the usual Unix way, read their
  installation documentation.

  Now <name|BurrTools> can be compiled and installed the usual way with
  <verbatim|configure>, <verbatim|make>, <verbatim|make install>.

  <subsection|Files and Folders>

  After installing <name|BurrTools> the following files should be on your
  system:

  \;

  <\with|par-mode|left>
    <\with|par-mode|right>
      <tabular|<tformat|<cwith|1|6|1|1|cell-width|2.5cm>|<cwith|1|6|2|2|cell-width|12.5cm>|<cwith|1|7|1|1|cell-halign|r>|<cwith|1|7|2|2|cell-hyphen|t>|<table|<row|<cell|<with|font-family|tt|burrGui.exe>>|<cell|The
      graphical user interface (GUI) to create puzzle files for
      <name|BurrTools>.>>|<row|<cell|<with|font-family|tt|UserGuide.pdf>>|<cell|This
      user guide.>>|<row|<cell|<with|font-family|tt|COPYING>>|<cell|A text
      file containing the <em|GNU General Public Licence>. This file may be
      deleted to save on disk space, but should always be included when
      sharing the program. Read it carefully before sharing or modifying the
      program.>>|<row|<cell|<with|font-family|tt|AUTHORS>>|<cell|A text file
      containing information about the contributors to the development of
      BurrTools. This file may be deleted to save on disk
      space.>>|<row|<cell|<with|font-family|tt|ChangeLog>>|<cell|An
      automatically created text file containing an overview of the changes
      made to the program since version 0.0.6. This file may be deleted to
      save on disk space.>>|<row|<cell|<with|font-family|tt|NEWS>>|<cell|A
      more readable version of <with|font-family|tt|ChangeLog>. Here all
      (more or less important) changes to the different versions are
      collected in a comprehensive list. This is probably the place to look
      for what changed when downloading a new version. This file may be
      deleted to save on disk space.>>|<row|<cell|<with|font-family|tt|uninstall.exe>>|<cell|The
      uninstall program will only be added after installing <name|BurrTools>
      with the <with|font-family|tt|Windows Installer>.<next-line>>>>>>
    </with>
  </with>

  Also a new folder, <with|font-family|tt|examples>, is created. This
  subdirectory contains a few examples of existing puzzles that illustrate
  the capabilities and functions of <name|BurrTools>. A brief overview of the
  examples is presented in Appendix <reference|AppendixExamples>.

  <section|Concepts and Definitions>

  Before we start describing the functions of <name|BurrTools>, let's
  synchronise our use of vocabulary and explain a few concepts that are
  crucial to the way <name|BurrTools> works.

  <subsection|Definitions><label|Definitions>

  <\description>
    <item*|Voxel>A voxel <em|(volume pixel)> is a space unit in the
    3-<no-break>D space. The shape of the voxels is defined by the space grid
    type. Currently <name|BurrTools> only supports cubic voxels. Each voxel
    has one of the following three states: <em|Empty>, <em|Fixed>
    (<em|Filled>) and <em|Variable> (<with|mode|math|\<vartriangleright\>><reference|Concepts>).
    Additional to that each voxel can also contain supplementary information
    in the form of colours that are attached to the whole or parts of that
    voxel. Currently <name|BurrTools> can only attach one single colour to
    the voxel as a whole.

    <item*|Spacegrid>The spacegrid defines the shape and orientation and
    arrangement of the voxels. Right now there are 3 space grids available in
    <name|BurrTools>: cubes, prisms with a equilateral triangle as base and
    tightly packed spheres. Spacegrids are always fixed and periodic. That
    means that a voxel in a certain position will always have the same shape
    and orientation. So a spacegrid defining, for example, all Penrose
    patters is not possible because this is neither a fixed nor a periodic
    pattern.

    <item*|Shape>This is a definition of a 3-<no-break>dimensional object.
    Shapes are assembled out of voxels.\ 

    <item*|Piece>A piece is a shape that is used as a part of the puzzle.

    <item*|Multipiece>Some pieces may have the same shape. <name|BurrTools>
    requires you to tell it that two or more pieces do have the same shape,
    otherwise it will find all solutions with all permutations. So a
    multipiece is a piece that's used more than once in the problem.

    <item*|Group (also Piece Group)>A collection of pieces (and/or
    multipieces) than can move with respect to each other, but cannot be
    separated from one another. Denoted with {}.

    <item*|Result>This is the shape that the pieces of the puzzle are
    supposed to assume once the puzzle is assembled.

    <item*|Problem>A problem in <name|BurrTools> consists of a list of pieces
    and/or multipieces, a result shape and possibly some constraints. You can
    have more than one problem in a file as it may be possible to have more
    than a single task with the same set of pieces (e.g. Piet Hein's
    <name|Soma Cube>). In other words, a problem is a statement about
    <em|what to do> with the pieces.

    <item*|Puzzle>A puzzle is either a single problem or a collection of
    problems.

    <item*|Identifier>A unique code to identify a shape, colour or problem.
    This consists of an automatically assigned prefix to which a custom name
    may be added. The prefix is already unique. It is a letter followed by a
    number. The letter is different for all items that required identifiers,
    e.g. it is S for shapes, P for problems and C for colours.

    <item*|Assembly>An assembly is a physically possible (meaning the pieces
    do not overlap in space) arrangement of pieces so that the resulting
    shape is formed. It is not guaranteed that it is actually possible to get
    the pieces into the positions of the assembly without using advanced
    technologies like Star Trek beaming.

    <item*|Solution>A Solution is an assembly with instructions how to
    assemble/disassemble the pieces.

    <item*|Assembler>The part of the program or algorithm that tries to
    assemble the puzzle.

    <item*|Disassembler>The part of the program that tries to find out how
    the pieces must be moved to assemble the puzzle. It does this by trying
    to disassemble an assembly. Some puzzles like <name|Pentominoes> don't
    require checking for disassemblability, they are always constructible.
    That's why these two tasks are separated.

    <item*|Solver>A short name to refer to the assembler and disassembler as
    a unit or just one of these without specifying which one.
  </description>

  <subsection|Concepts><label|Concepts>

  As described above <name|BurrTools> works with shapes which are merely a
  collection of voxels that each can have either one of three different
  states: <em|empty>, <em|fixed> or <em|variable>. Particularly the
  difference between fixed and variable voxels has a great impact on the way
  the solver works and which assemblies are considered to be valid and which
  are not. Besides that, the validity of solutions can be further restricted
  by imposing colour constraints.\ 

  <subsubsection|Voxel States>

  <\description-compact>
    <item*|Empty>The empty state is rather superfluous as it can also be
    regarded as the absence of any voxel. It is just used in the result shape
    to indicate the spots that can't be filled at all (holes).

    <item*|Fixed (or Normal)>The normal or fixed voxels <em|need to be
    filled> in the final result, otherwise it is not considered to be a valid
    assembly.

    <item*|Variable>The variable state is used to instruct the program that
    for a particular voxel it is unknown whether it will be filled or empty
    in the final assembly. This is required for puzzles that have holes in
    <em|undetermined> places (like all the higher level six-piece burrs). All
    voxels that <em|might> be empty <em|<em|<em|<em|must>>>> have the
    <em|<em|variable>> state in the result shape. Right now the variable
    state can only be used in <em|<em|result shapes>> and the solver will pop
    up an error message whenever it encounters a variable state in a normal
    piece.<next-line>Later on variable voxels might be used in piece shapes
    as well to define voxels in the shape that the program might alter to
    create interesting puzzles.
  </description-compact>

  The question now is: <em|why not always use variable voxels> in the result
  shape? This is a matter of speed. When the program tries to find assemblies
  and encounters a voxel that it is unable to fill with the available shapes
  it can immediately abort, if that voxel has the filled state in the result
  shape as the algorithm is instructed that it <em|must> fill this particular
  voxel but it cannot do so, so something is wrong. On the other hand, if the
  state of that voxel is variable the algorithm knows nothing and has to
  carry on.

  <subsubsection|Colour Constraints>

  Colours allow you to add constraints to the possible placement of pieces.
  This is done by assigning a colour to one or more voxels of the piece(s)
  and the result shape (<with|mode|math|\<vartriangleright\>><reference|AddingColour>).
  Then you can set some colour placement conditions for each problem
  (<with|mode|math|\<vartriangleright\>><with|mode|math|><reference|ColourConstraints>).
  The program will place pieces only at positions that fulfil the colour
  conditions defined.

  These colour conditions currently allow the definition of what coloured
  voxels of the pieces may go into what coloured voxels in the result shape.
  The <em|neutral> <em|colour> is different, since it always fits. Voxels in
  a piece that are in the neutral colour fit everywhere and neutral coloured
  voxels in the result shape can accommodate for every piece voxel,
  independent of its custom colour.

  Currently the assigned colour is used just like painting the whole voxel
  with this colour, but in the future more advanced possibilities for
  colouring and conditions may be added.

  <section|Notes for PuzzleSolver3D Users><label|PS3DUsers>

  <name|BurrTools> was initially very much based on <name|PuzzleSolver3D> by
  André van Kammen but diverged quite a bit from that. We strongly advise you
  to read this user guide since there are some features in <name|BurrTools>
  that work somewhat different as their counterparts in <name|PuzzleSolver3D>
  and there are also some functions that PuzzleSolver3D doesn't have. Below
  are the most prominent differences that need your attention:

  <\enumerate-numeric>
    <item><name|BurrTools> doesn't handle holes automatically as
    <name|PuzzleSolver3D> does. This may at first sound like a disadvantage
    but in fact it isn't. Unless you select <em|'Outer limits of result must
    be filled'> on the solve tab, <name|PuzzleSolver3D> treats all cubes of
    the target shape as cubes that might be filled but don't need to be.
    Cubes that <em|must> be filled however speed up the search process. The
    more there are of these (as compared to the total number of cubes), the
    faster the solver will run as fewer possibilities are left to test.
    <name|BurrTools> requires you to specify exactly which cubes in the
    result shape must be filled and which ones may be empty.

    <item>The <name|BurrTools> solver doesn't automatically detect multiple
    identical pieces. You need to specify, if a piece is used more than once.
    If you just copy them the way you do in <name|PuzzleSolver3D> the program
    will find way too many solutions. For example, with Bruce Love's
    <name|Lovely 18 Piece Burr> it will find nearly 40,000,000 times as many
    solutions as there really are. So be careful.

    <item><name|BurrTools> allows you to define multiple problems in a single
    session. So you can, for example, save all the <name|Soma Cube> (Piet
    Hein) problems within one single file.

    <item><name|BurrTools> has no limits to the number and size of pieces.
    You can have as many pieces as you want and they are not confined to a
    grid of 24<with|mode|math|\<times\>>24<with|mode|math|\<times\>>24.

    <item>There is no limit to the number of possible positions for the
    pieces. So it won't happen that <name|BurrTools> complains about too many
    placements. As long as your computer has sufficient memory the program
    will merrily continue working -- even if it would take longer than the
    universe exists -- to complete the search.

    <item><name|BurrTools> supports another gridspace besides the cube space
    supported by <name|PuzzleSolver3D>. This allows the design and analysis
    of completely new puzzles<next-line>
  </enumerate-numeric>

  <subsection|Importing <name|PuzzleSolver3D> files>

  <name|BurrTools> also has capabilities for <em|importing>
  <name|PuzzleSolver3D> files. So there's no need to redo your designs from
  scratch, although some postediting may be required because of the
  differences in handling duplicates of pieces and holes in the puzzle.

  There are 2 possibilities for the holes. Depending on whether the option
  ``Fill outer Cubes'' is enabled or not when you solve the puzzle with
  <name|PuzzleSolver3D> you must either make the inner cubes of the result
  shape or the whole shape variable when you want to get the same results
  with <name|BurrTools>. This can be done with the tools described in section
  <reference|ChapterConstrainingTools>. With these tools you can make inner
  and outer cubes of a shape variable.

  The duplicate pieces are handled automatically. <name|BurrTools> adds all
  shapes to the new puzzle but does not add duplicates to the problem instead
  the counter for the original is increased. The unused shapes are marked as
  unused and can be deleted when they are not required.

  <\with|par-mode|right>
    <chapter|The BurrTools Interface>
  </with>

  When BurrTools is started for the very first time the GUI will look like
  Figure<\float|float|tb>
    <big-figure|<postscript|Pics/Window_StartUp.png|*5/8|*5/8||||>|<label|FigureMainWindowStart>The
    main window on start-up>
  </float> <reference|FigureMainWindowStart> which shows the main window.
  Although some small variations may occur depending on your operating
  system, screen resolution and display preferences settings. The GUI has
  four major parts. On top there is a <em|menu bar> that allows handling of
  files and offers extra functionality as well as some preferences settings
  for the program. At the bottom there is a traditional <em|status line>
  presenting relevant information about the task at hand. In between there is
  a <em|tools section> on the left and a <em|3-<no-break>D viewport> on the
  right.

  <section|The BurrTools Menus>

  Below is a brief overview of the main menu entries with references to the
  places in the text where a more detailed explanation is provided.

  <\description-compact>
    <item*|<with|font-family|ss|File>>This menu holds the procedures for
    handling files within <name|BurrTools> and for exiting the program
    (<with|mode|math|\<vartriangleright\>><reference|FileMenu>).

    <item*|<with|font-family|ss|Toggle 3D>>Swaps the 2-<no-break>D and the
    3-<no-break>D grids for the <with|font-family|ss|Entities> tab
    (<with|mode|math|\<vartriangleright\><reference|Navigating2D3D>>).

    <item*|<with|font-family|ss|Export>>Contains a submenu with 2 entries.
    One allows you to export the contents of the 3-<no-break>D viewer that
    can be used to create high quality solution sheets
    (<with|mode|math|\<vartriangleright\>><reference|ExportingImages>). The
    other allows you to create STL files for 3-<no-break>D printers
    (<with|mode|math|\<vartriangleright\>><reference|ExportingSTL>)

    <with|font-family|ss|<item*|Grid Parameters>>This menu entry will allow
    you to change parameters for the currently used space grid. These
    parameters include things like scaling of axes or skew. Not all space
    grids support parameters.

    <item*|<with|font-family|ss|Status>>This opens up a window containing
    lots of maybe useful information about the shapes of the puzzles
    (<with|mode|math|\<vartriangleright\>><reference|ChapterStatus>).

    <item*|<with|font-family|ss|Edit Comment>>Allows appending textual
    information to the puzzle file (<with|mode|math|\<vartriangleright\>><reference|AddingComments>).

    <item*|<with|font-family|ss|Config>>This menu item provides some
    preferences settings (<with|mode|math|\<vartriangleright\>><reference|ConfigMenu>).

    <item*|<with|font-family|ss|About>>Shows a window with some information
    about the program.
  </description-compact>

  <subsection|The File Menu><label|FileMenu>

  The <with|font-family|ss|<strong|File>> menu has all the traditional
  entries for handling files. Many of these are well known from other
  software and need not much explanation. Some of the items also have
  keyboard short cuts as indicated in the menus. Prior to executing most of
  these commands a warning (and option to cancel) is given whenever changes
  to the current design haven't been saved yet.

  <\description-compact>
    <item*|<with|font-family|ss|New>>Starts a new design after removing all
    the information of the current one. The first thing that happens when you
    start a new puzzle is that you will be asked which spacegrid to use. When
    <name|BurrTools> is started it always starts with a puzzle that uses the
    cubes spacegrid, so when you want to use another grid you need to use
    this menu.

    <item*|<with|font-family|ss|Load>>Opens a <name|BurrTools>
    <with|font-family|tt|*.xmpuzzle> file. A notification will pop up when a
    <em|partially> solved design is loaded. Short cut:
    <with|font-family|tt|[F3]>.

    <item*|<with|font-family|ss|Import>>This entry opens a traditional file
    dialogue that allows importing <name|PuzzleSolver3D> files
    (<with|font-family|tt|*.puz>) into <name|BurrTools.> Although these
    imported designs often can be subjected to the solver right away, some
    postediting may be required because of the differences in the way
    <name|BurrTools> handles holes in the result and uses duplicates of
    pieces. BurrTools will import <em|all> the pieces from the
    <with|font-family|tt|*.puz> file and assign them to the shapes S1 to
    S<em|n>-<no-break>1. Accordingly, the <em|result> from the
    <name|PuzzleSolver3D> file will be assigned to the last shape (S<em|n>).
    Also a problem definition is automatically created
    (<with|mode|math|\<vartriangleright\>>Chapter
    <reference|ChapterPuzzles>).

    <no-indent>Since all imported shapes consist only of fixed voxels, the
    result shape may need some editing (puzzles that have internal holes or
    pieces not filling the outskirts of the result shape) to make the solver
    run. Also duplicated pieces are preferably deleted from the
    <with|font-family|ss|Shapes> list (<with|mode|math|\<vartriangleright\>><reference|CreatingShapes>)
    but certainly from the <with|font-family|ss|Piece Assignment> list
    (<with|mode|math|\<vartriangleright\>><reference|PieceAssignment>),
    otherwise <name|BurrTools> will find way too many solutions as it will
    differentiate between all the possible permutations of these identical
    pieces.

    <item*|<with|font-family|ss|Save>>Saves your work into a
    <with|font-family|tt|*.xmpuzzle> file. If the design had not been saved
    before (indicated with '<with|font-family|tt|Unknown>' in the BurrTools
    windows title bar) the <with|font-family|ss|Save As> command will be
    activated. Short cut: <with|font-family|tt|[F2]>.

    <item*|<with|font-family|ss|Save As>>Allows you to save any changes to a
    new file and thus keeping the original design the way it was.

    <item*|<with|font-family|ss|Quit>>Shuts down <name|BurrTools.><next-line>
  </description-compact>

  <with|mode|math|><with|mode|math|><with|mode|math|><with|mode|math|>Except
  when the solver is actually <em|running>, saving your work is always
  possible. This means that after stopping (pausing) the solver it is
  possible to save the results found thus far. Later on these partially
  solved puzzles can be loaded again and the solving process may be resumed.
  This allows you to subject 'huge' problems (e.g. 25 Y-<no-break>pentominoes
  in a 5<with|mode|math|\<times\>>5<with|mode|math|\<times\>>5 cube assembly)
  to <name|BurrTools> and have them solved in several sessions overnight or
  whenever you don't need your computer for other tasks.

  <subsection|The Configuration Menu><label|ConfigMenu>

  The <with|font-family|ss|<strong|Config>> item on the menu bar opens a new
  window (Figure<\float|float|hf>
    <big-figure|<postscript|Pics/Window_Config.png|*5/8|*5/8||||>|<label|FigureConfig>The
    configuration window>
  </float> <reference|FigureConfig>) to set some options for the GUI. These
  settings will be stored in a file that is either in your home directory
  (<name|Unix>) or in your profile (<name|Windows>). The program will use
  these settings each time it is started.

  <\description-compact>
    <item*|<with|font-family|ss|Fade Out Pieces>>This option affects the way
    pieces that <em|become> <em|separated> from the rest are depicted. Hence,
    the effects are only visible after running the solver
    (<with|mode|math|\<vartriangleright\>><reference|VisibilityOfPieces>).

    <item*|<with|font-family|ss|Use Lights in 3D View>>This option toggles
    the use of a spotlight in the 3-<no-break>D viewer. When disabled the
    items in the 3-<no-break>D viewport get a uniform (high) illumination,
    whereas enabling this option provides a more rendered appearance of the
    objects by adding a spotlight in the upper right corner of the
    3-<no-break>D viewport and shading the faces of the objects. However, on
    some systems this may result in a relatively dark left bottom corner that
    can hamper a clear view on the objects.

    <item*|<with|font-family|ss|Use Tooltips>>By default <name|BurrTools>
    shows tooltips for most of its controls, but to the more experienced user
    these become soon very annoying. This option allows you to switch these
    tooltips off.
  </description-compact>

  <section|The Status Line>

  The status line has two parts. On the left information about the task at
  hand is given and on the right are some tools to alter the 3-<no-break>D
  view. Currently only the option <with|font-family|ss|<strong|Colour 3D
  View>> is available. When checked all custom colours
  (<with|mode|math|\<vartriangleright\>><reference|AddingColour>) will be
  shown in the 3-<no-break>D viewer, whereas the shapes will be represented
  with their neutral colour if this option is left unchecked.

  <section|The Tools Section>

  In between the menu bar and the status line is the most important part of
  <name|BurrTools.> The section that allows you to submit existing puzzles to
  the solver, but more even important lets you create and test your own
  designs.

  <subsection|The Puzzle People>

  The tools section has three major tabs that somewhat can be compared with
  real people in the world of mechanical puzzles. First there is the
  <with|font-family|ss|Entities> tab, which can be seen as the craftsman who
  <em|creates> different <em|shapes> but hasn't to bother about the purpose
  of these (<with|mode|math|\<vartriangleright\>>Chapter
  <reference|ChapterShapes>). As long as his saw blade is sharp he's the
  happiest man in the whole wide world. Next, we have the
  <with|font-family|ss|Puzzle> tab. This is the weirdo who thinks it's fun to
  come up with completely insane <em|problems> to be solved with the
  otherwise very innocent objects of our craftsman
  (<with|mode|math|\<vartriangleright\>>Chapter <reference|ChapterPuzzles>).
  However, his contribution to the preservation of our planet is
  considerable... by saving a lot of wood scraps from the incinerator. And
  last we have the <with|font-family|ss|Solver>, the poor guy who spends not
  only a great deal of his money on these finely crafted puzzles but almost
  all of his leisure time on <em|solving> them
  (<with|mode|math|\<vartriangleright\>>Chapter
  <no-break><reference|ChapterSolver>), only to feel very euphoric when he
  finally succeeds. But scientists are still breaking their heads over the
  question whether this is caused by the sweet smell of success, or is merely
  due to severe sleep deprivation.

  <subsection|Resizing the Elements>

  Although the layout of the GUI is designed to suit the needs of most users,
  it sometimes may be useful to resize some elements to enhance the comfort
  in using <name|BurrTools.> Besides the traditional resizing of the main
  window, <name|BurrTools> has a couple of features to alter the relative
  importance of its controls.

  First, the tools tabs can be made wider or narrower (thus making the
  3-<no-break>D viewport more or less important) by dragging the right edge
  of the tools section. Hovering your mouse pointer over that edge will make
  it change into a left-right arrow, indicating that you can start dragging
  it.

  Second, within each of the three main tabs some sections (panels) can be
  resized as well. For example, if you have a design with many different
  shapes but no colour constraints at all, reducing the size of all colour
  related controls and maximising those concerning shapes could be very
  advantageous. The panels on the tool tabs are separated by so called resize
  handles (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/SizeBar.png|*5/8|*5/8||||>|<label|FigureResize>Resize
    handles>
  </float> <reference|FigureResize>). The separators that allow resizing are
  easily recognised by a little bevelled square on their right end. Hover
  your mouse pointer over the lines until it changes into an up-down arrow,
  indicating that you can drag the separator up or down to resize the panel.

  Note that each section has a minimum size. It is not possible to make it
  smaller than that minimum size.

  <section|The 3-<no-break>D Viewer>

  Normally the biggest part of the GUI is reserved for the 3-<no-break>D
  viewport. In fact this 3-<no-break>D viewer is threefold and has different
  properties for each of the tabs of the tools section. For the
  <with|font-family|ss|Entities> tab the 3-<no-break>D viewport shows the
  currently selected shape and reflects all editing operations performed on
  that shape. Also the x-, y- and z-<no-break>axes are shown to assist
  navigating in space. With the <with|font-family|ss|Puzzle> tab activated an
  overview of the current problem is presented: the result shape (double
  sized) on top and a single instance of each shape used as pieces below it.
  Finally, for the <with|font-family|ss|Solver> tab, the 3-<no-break>D viewer
  can be used to browse all found assemblies and/or show an animation of the
  moves involved in the disassembly of the puzzle.

  Any object in the 3-<no-break>D view can be <em|rotated> by simply dragging
  it and the scrollbar on the right allows <em|zooming> in or out on that
  object by respectively moving the slider down or up. Note that the zoom
  settings are independent for each of the three tools tabs.

  Extra options for the 3-<no-break>D viewer are available in the
  <with|font-family|ss|Config> menu (<with|mode|math|\<vartriangleright\>><reference|ConfigMenu>).

  <\with|par-mode|right>
    <chapter|Creating Shapes><label|ChapterShapes>
  </with>

  The key concept of <name|BurrTools> is <em|shapes>. A shape is simply a
  definition of an object in 3-<no-break>D space and consists of a collection
  of voxels (space units). These voxels in turn may have their own
  characteristics such as <em|state> and <em|colour>. Note that this
  definition also includes shapes made out of voxels that are only attached
  to each other by a single edge, just a corner or even are completely
  separated in space. The solver certainly won't bother... but how these
  shapes could be crafted in the workshop is beyond the scope of the program.

  All functions and tools for creating and editing shapes - once the grid
  type is set - are located on the <with|font-family|ss|<strong|Entities>>
  tab (shapes are the <em|physical> <em|entities> that can make a puzzle when
  subjected to certain rules) which has - from top to bottom - three main
  sections (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Window_DDD_0.png|*5/8|*5/8||||>|<label|FigureEntitiesTab>Creating
    shapes on the Entities tab>
  </float> <reference|FigureEntitiesTab>):

  <with|font-series|medium|<\description-compact>
    <item*|The <with|font-family|ss|Shapes> panel>This section is mainly a
    list of the available shapes and has the tools for creating and managing
    the shapes. Shapes to be edited can be selected in this list
    (<with|mode|math|\<vartriangleright\>><reference|CreatingShapes>).

    <item*|The <with|font-family|ss|Edit> panel>This section provides the
    tools to build or edit the currently selected shape. This panel contains
    a series of subtabs with several tools for adjusting the
    <with|font-family|ss|<strong|<with|font-series|medium|Size>>> of the
    shapes, <with|font-family|ss|<strong|<with|font-series|medium|Transform>>>
    them in 3-<no-break>D space and some extra editing
    <strong|<with|font-family|ss|<with|font-series|medium|Tools>>>. Below
    these subtabs there's a toolbar with the devices for actually
    constructing the shapes in the 2-<no-break>D grid at the bottom of the
    panel (<with|mode|math|\<vartriangleright\>><reference|BuildingShapes>).

    <item*|The <with|font-family|ss|Colours> panel>This panel contains -
    besides a list of the available colours - the tools to create and edit
    custom colours which can be assigned to the voxels of the shapes. These
    colours can be merely ornamental or can have a serious impact on the way
    the solver will work by imposing restrictions on the possible placements
    of the pieces (<with|mode|math|\<vartriangleright\>><reference|AddingColour>).
  </description-compact>>

  <section|Spacegrids>

  Currently <name|BurrTools> handles cubic grids, grids that use prisms with
  a base shape that is a equilateral triangle and tightly packed spheres. The
  spacegrid is used for all shapes that are used within a puzzle so you can
  not have one shape made out of cubes and one using another grid. The
  spacegrid needs to be set <em|before> you start with the puzzle. It can not
  be changed later on. The gridtype is selected when you use the
  <with|font-family|ss|<strong|New>> option. Some gridtypes support it to set
  some parameters of the grid, like scaling or skew. These parameter can be
  used to suppress certain orientations for shapes but not to create new
  puzzle shapes. E.g the sphere grid might some time support a switch to turn
  it into a space of rhombic dodecahedra. This space is very similar except
  that some orientations that are possible with spheres can not be done with
  the dodecahedra.

  Same for cubes: there might be a parameter that scales the cubes in
  y-direction. If that values differs from the x-direction value it will be
  only possible to turn the cubes by 180<degreesign> when rotated around the
  x-axis.

  <section|Creating Shapes><label|CreatingShapes>

  The very first step is to initialise the shapes that can be used in your
  puzzle design. All the tools to do so are just below the
  <with|font-family|ss|<strong|Shapes>> caption (Figure
  <reference|FigureEntitiesTab>). Clicking the
  <with|font-family|ss|<strong|New>> button starts a completely new one with
  an empty grid, while <with|font-family|ss|<strong|Copy>> allows you to edit
  a previously entered shape without destroying the first. Obsolete and
  redundant shapes can be discarded with the
  <with|font-family|ss|<strong|Delete>> button.

  All shapes are identified with an '<strong|<with|font-family|ss|S<em|x>>>'
  prefix. This prefix serves as a unique identifier for the shape throughout
  the GUI and cannot be removed or altered, but
  <with|font-family|ss|<strong|Label>> allows you to add a more meaningful
  name. Note that on the status line the shapes will only be referred to by
  their prefixes.

  By clicking an identifier in the list the shape becomes selected and ready
  to be edited. Also a short description of that shape appears on the status
  line. The currently selected shape is indicated with a white border around
  its identifier in the shapes list.

  The buttons with the arrows pointing left and right allow you to change the
  position of the shape in the list. The first one moves the selected shape
  toward the front of the list, whereas the other button moves the shape
  toward the end of the list. Note that rearranging shapes will cause to
  change their prefix but not the additional name.

  Unlike the pieces in <name|PuzzleSolver3D> shapes don't need to be part of
  the puzzle. This means that you can build a file that contains a vast
  number of shapes, e.g. all 59 notchable six-piece burr pieces, of which you
  assign only 6 to the pieces of your puzzle design.

  Finally the shapes have an additional parameter: the weight. This value is
  used when constructing the disassembly animations. When the disassembler
  has found 2 groups of pieces that can be moved against each other it needs
  to decide which group to actually move and which to keep where it is. This
  decision can be influenced with the weight. The program searches the
  maximum weight in both groups and the one group that has the bigger maximum
  weight will be kept in place and the other group will be moved. If both
  groups have the same maximum weight the group with the smaller number of
  pieces will be used.

  <section|Grid Functions><label|GridFunctions>

  Since shapes are defined as objects in 3-<no-break>D space and
  theoretically 3-<no-break>D space is unlimited in size, it's convenient
  somehow to be able to define a more feasible subspace to work with. This,
  and some more advanced scalings of the shapes, can be accomplished with the
  functions on the <with|font-family|ss|<strong|Size>> subtab
  (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Subtab_Size.png|*5/8|*5/8||||>|<label|FigureSizeSubtab>Grid
    and scaling functions>
  </float> <reference|FigureSizeSubtab>) of the
  <with|font-family|ss|<strong|Edit>> panel.

  Note that the tab might look slightly different for different gridtypes.
  For example the sphere grid doesn't have the shape buttons as those are
  useless with this grid.

  <subsection|Adjusting the Grid Size><label|AdjustingGridSize>

  When the very first shape is initialised it has a default grid size of
  6<with|mode|math|\<times\>>6<with|mode|math|\<times\>>6, but all other new
  shapes will inherit the grid size of the currently selected shape. This
  feature can be very useful in creating a series of shapes that are
  restricted with respect to certain dimensions (e.g. all pentacubes that fit
  in a 3<with|mode|math|\<times\>>3<with|mode|math|\<times\>>3 grid).
  Selecting the proper shape before creating a new one often can save a
  considerable amount of time by avoiding otherwise necessary grid
  adjustments.

  Adjusting the grid size to your needs can be done either by entering values
  in the input boxes next to the axis labels or by dragging the spin wheels.
  When you enter values the grid will be updated as soon as you select one of
  the other input boxes (either by a mouse click or by the
  <with|font-family|tt|[Tab]> key) or when you press the
  <with|font-family|tt|[Return]> key. Note that the grid is also updated by
  simply clicking in or next to the 2-<no-break>D grid. To avoid unexpected
  results it's recommended always to confirm the entered values with the
  <with|font-family|tt|[Return]> key. Increasing any grid dimension is
  completely harmless, but decreasing them needs some caution since it can
  destroy parts of the shape.

  The checkboxes for <em|linking adjustments> - to the right of the spin
  wheels - allow you to adjust two or all dimensions simultaneously. All
  linked dimensions will <em|<em|increase>> or decrease by the same
  <em|absolute> amount. However, none of the dimensions can be made smaller
  than 1 unit. Linked dimensioning is very useful in creating bigger and
  complex shapes such as the result shape of <name|No Nukes!> (Ronald
  Kint-Bruynseels), which is easily done by first creating the central burr
  in a 6<with|mode|math|\<times\>>6<with|mode|math|\<times\>>6 grid and
  adding the extensions after resizing the grid to
  14<with|mode|math|\<times\>>14<with|mode|math|\<times\>>14 and centring the
  'core' in that enlarged grid.

  <subsection|Advanced Grid and Scaling Functions><label|AdvancedGrid>

  <name|BurrTools> has some powerful time saving functions to manipulate the
  position of the shape in its grid or to rescale a shape together with the
  grid. These features are grouped below the captions
  <with|font-family|ss|<strong|Grid>> and
  <with|font-family|ss|<strong|Shape>> on right side of the
  <with|font-family|ss|Size> subtab. The first set of three will only affect
  the grid and/or the position of the shape in the grid, the other procedures
  however will have an impact on the shape as such by scaling it up or down.

  Below is an overview of these functions, explaining what they precisely do
  and with an indication of the purpose they were introduced in
  <name|BurrTools>. No doubt you'll soon find other situations in which these
  tools can prove to be valuable.

  \;

  <strong|<with|font-family|ss|<no-indent>Grid> tools.> Most of these tools
  are somewhat extended versions of the more general transformation tools
  (<with|mode|math|\<vartriangleright\>><reference|TransformationTools>) and
  have the advantage that they can act on all shapes at once
  (<with|mode|math|\<vartriangleright\>><reference|AdjustAllShapes>).

  \;

  <tabular|<tformat|<cwith|1|3|1|1|cell-width|1cm>|<cwith|1|3|2|2|cell-hyphen|b>|<cwith|1|3|1|1|cell-valign|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|1|2|2|cell-width|13cm>|<cwith|1|1|1|1|cell-width|2cm>|<cwith|1|3|1|1|cell-halign|r>|<table|<row|<cell|<postscript|Pics/Button_Size_Grid_Minimize.png|*5/8|*5/8||||>>|<cell|<strong|Minimise
  the grid -> This function will minimise the grid to fit the dimensions of
  the shape it contains. Use it to reduce the disk space occupied by your
  puzzle files. Note that the result of this function is strictly based on
  the contents of the grid and will have no effect whatsoever on empty
  grids.<next-line>>>|<row|<cell|<postscript|Pics/Button_Size_Grid_Center.png|*5/8|*5/8||||>>|<cell|<strong|Centre
  the shape in the grid -> This function centres the shape in the surrounding
  grid thus allowing you to edit all sides of the shape. In some cases this
  will also <em|increase> one or more dimensions of the grid by a single unit
  to provide truly centring. The function is most useful in editing
  symmetrical shapes in combination with the compound drawing methods
  (<with|mode|math|\<vartriangleright\>><reference|CompoundDrawing>).<next-line>
  >>|<row|<cell|<postscript|Pics/Button_Size_Grid_Align.png|*5/8|*5/8||||><strong|>>|<cell|<strong|Align
  the shape to the origin -> This function brings the shape as close as
  possible to the origin of the grid. It can very useful if you want to make
  a descending series of rectangular blocks by copying the shape and manually
  adjusting the grid dimensions.<next-line>>>>>>

  <no-indent><strong|<with|font-family|ss|Shape> tools.> Use the following
  functions wisely because unnecessary and extreme scaling up of the shapes
  will bear a heavy load on your system resources and can increase solving
  time dramatically. Also, trying to undo such 'ridiculous' upscalings with
  the 1:1 tool can take a considerably long time. So, <em|think twice, click
  once...>

  These tools only make sense for spacegrids where a group of voxels can be
  group to make a upscaled shape that looks like a voxel of the grid, e.g. a
  group of 2x2x2 cubes looks like a bigger cube. As this is not working with
  spheres, those tools are not available there.

  \;

  <tabular|<tformat|<cwith|1|3|1|1|cell-width|1cm>|<cwith|1|3|2|2|cell-hyphen|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|3|1|1|cell-valign|t>|<cwith|1|1|2|2|cell-width|13cm>|<cwith|1|1|1|1|cell-width|2cm>|<cwith|1|3|1|1|cell-halign|r>|<table|<row|<cell|<postscript|Pics/Button_Size_Shape_11.png|*5/8|*5/8||||>>|<cell|<strong|Minimise
  the size of the shape <with|font-family|rm|<with|font|roman|(1:1 tool)>> ->
  This function tries to make the shape as small as possible without any loss
  of information and at the same time scales down the grid by the same
  factor. Use this function to check the design for oversized shapes which
  would slow down the solver. Note that although this function can undo the
  effects of both the next scaling functions, the result cannot be guaranteed
  since the algorithm may scale down beyond the initial
  size.<next-line>>>|<row|<cell|<postscript|Pics/Button_Size_Shape_x2.png|*5/8|*5/8||||>>|<cell|<strong|Double
  the scale -> This function will double the scale of the shape (and its
  grid). In other words, it will replace every voxel in the shape with a
  group of voxels that all have the same characteristics (state and colour)
  as the original voxel. This can be very useful to introduce half-unit
  notches or colouring into the design without having to redraw the
  shape(s).<next-line>>>|<row|<cell|<postscript|Pics/Button_Size_Shape_x3.png|*5/8|*5/8||||>>|<cell|<strong|Triple
  the scale -> This function is similar to doubling the scale. Only now a
  scaling factor of 3 is used and hence every voxel in the shape will be
  replaced by 27 identical voxels. This can be very useful if you want to
  introduce <em|'pins and holes'><em|<em|>> into your design.>>>>>

  <subsection|Adjusting All Shapes><label|AdjustAllShapes>

  A last, but certainly not least, item to mention is the
  <strong|<with|font-family|ss|Apply to All Shapes>> checkbox. When checked
  <em|all> shapes, whether they are selected or not, will be affected by the
  settings and procedures on the <with|font-family|ss|<strong|<with|font-series|medium|Size>>>
  subtab. This is very useful and time saving when a certain adaptation needs
  to be done to all the shapes, e.g. transforming a six-piece burr with
  length 6 into one with length 8.

  However, some precautions are build in to prevent unnoticed destroying of
  shapes. Manually reducing any grid dimension will still only be performed
  on the currently selected shape, whereas increasing (which is completely
  harmless to the shapes) will affect all grids. On the other hand,
  minimising the grids will be applied to all shapes since it is content
  related. The 1:1 tool won't affect any shape unless <em|all> shapes can be
  scaled down <em|by the <em|<em|same>> factor>. This to prevent ending up
  with an unintended mixture of differently scaled shapes.

  <section|Building and Editing Shapes><label|BuildingShapes>

  <\with|par-par-sep|0>
    Once a shape has been initialised the 2-<no-break>D grid wherein it can
    be build becomes accessible on the <with|font-family|ss|<strong|Edit>>
    panel. Basically one needs only three tools to create any shape, but some
    more features are added to make life easy. All these are on the toolbar
    right above the 2-<no-break>D grid (Figure<\float|float|bhft>
      <big-figure|<postscript|Pics/ToolbarGrid.png|*5/8|*5/8||||>|<label|FigureToolbarGrid>Toolbar
      and 2-D grid>
    </float> <reference|FigureToolbarGrid>). The first four buttons are the
    <em|basic drawing tools> and <em|colouring tool>. These are all toggle
    buttons, meaning that enabling one will disable the others. They affect
    the presence and/or the state and colour of the voxels by clicking in, or
    dragging over the cells in the 2-<no-break>D grid.
  </with>

  Next come two toggle buttons that allow you to select the <em|drawing
  style>. This is the way the basic drawing tools will respond to dragging
  the mouse over the grid cells. Finally, a series of <em|compound drawing
  tools> follows. These extend the range of the basic drawing tools and can
  all be cumulatively added to them.

  <subsection|Navigating in 2-<no-break>D and
  3-<no-break>D><label|Navigating2D3D>

  Building and editing takes almost exclusively place in the 2-<no-break>D
  grid to which the 3-<no-break>D viewport only acts as a visual aid. Both
  have their corresponding axes in the same colour: <em|red> for the
  x-<no-break>axis, <em|green> for the y-<no-break>axis and <em|blue> for the
  z-<no-break>axis. For the 2-<no-break>D grid, which actually can show only
  a single layer at a time, the z-<no-break>axis is represented with a
  scrollbar (Figure <reference|FigureToolbarGrid>). By default every new
  shape starts on the bottom layer and the scrollbar allows you to move up
  and down through the different layers along the z-<no-break>axis (the
  number of z-<no-break>layers is always indicated with the proper number of
  ticks along the scrollbar). Another way to navigate these
  z-<no-break>layers is by pressing <with|font-family|tt|[+]> (moves up one
  layer) or <with|font-family|tt|[-]> (moves down one layer) on the
  keyboard.<\float|float|tbhf>
    <big-figure|<postscript|Pics/NavigationA.png|*5/8|*5/8||||>
    \ \ \ \ <postscript|Pics/NavigationB.png|*5/8|*5/8||||>|<label|FigureNavigation>Selections
    of grid cells in 2-<no-break>D an 3-<no-break>D>
  </float>

  Moving the mouse cursor over the 2-<no-break>D grid gives an indication of
  the cell(s) - depending on the state of the compound drawing tools - that
  will be affected by clicking. These indications are also reflected in the
  3-<no-break>D viewer. Furthermore, to facilitate positioning on different
  layers every non-empty voxel on the 2-<no-break>D layer just below the
  current one 'shines through' in a very light shade of the neutral colour
  associated with that shape (Figure <reference|FigureNavigation>). This
  makes building shapes from bottom to top very easy.

  With larger grid sizes the cells of the 2-<no-break>D grid can become very
  small, even when the available area for the grid on the
  <with|font-family|ss|Entities> tab is maximised. To overcome this
  inconvenience the 2-<no-break>D grid and the 3-<no-break>D viewport can be
  exchanged. To do so, click the <strong|<with|font-family|ss|Toggle 3D>>
  item on the menu bar or press <with|font-family|tt|[F4]>. Note that this
  only affects the position of the 3-<no-break>D viewport for the
  <with|font-family|ss|Entities> tab.

  <subsection|Basic Drawing Tools>

  The basic drawing tools affect only the presence and/or the state of a
  particular voxel in the shape. In fact they're - together with the brush
  tool (<with|mode|math|\<vartriangleright\>><reference|BrushTool>) - all
  that's needed to create any shape in <name|BurrTools>. The following is a
  description of these tools. Note that each is also accessible through a
  keyboard short cut.

  \;

  <tabular|<tformat|<cwith|1|1|2|2|cell-hyphen|t>|<cwith|1|1|1|1|cell-valign|t>|<cwith|1|1|1|1|cell-halign|r>|<cwith|2|2|1|1|cell-valign|t>|<cwith|2|2|1|1|cell-halign|r>|<cwith|3|3|1|1|cell-valign|t>|<cwith|3|3|1|1|cell-halign|r>|<cwith|2|2|2|2|cell-hyphen|t>|<cwith|3|3|2|2|cell-hyphen|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|1|2|2|cell-width|13cm>|<cwith|1|1|1|1|cell-width|2cm>|<table|<row|<cell|<postscript|Pics/Button_Toolbar_Draw_Fixed.png|*5/8|*5/8||||>>|<cell|<strong|Fixed
  pen -> Use this tool to draw <em|normal> or <em|fixed> voxels. Fixed voxels
  are represented by completely filled cells in both the 2-<no-break>D and
  the 3-<no-break>D grid (<with|mode|math|\<vartriangleright\>><reference|Representations>).
  Remember that these fixed voxels <em|must be filled> in the final result.
  Keyboard short cut: <with|font-family|tt|[F5]>.<next-line>>>|<row|<cell|<postscript|Pics/Button_Toolbar_Draw_Variable.png|*5/8|*5/8||||>>|<cell|<strong|Variable
  pen -> This tool allows you to draw <em|variable> voxels. In the
  2-<no-break>D grid these variable voxels do not completely fill the cells,
  but have a narrow border showing the background of the grid. In the
  3-<no-break>D viewport the variable voxels have a black inset
  (<with|mode|math|\<vartriangleright\>><reference|Representations>).
  Variable voxels instruct the solver that these particular places may be
  <em|either> <em|filled or empty> in the final result. So variable voxels
  are only allowed in result shapes and the solver will give a warning
  whenever it encounters any variable voxels in a shape used as a piece.
  Short cut: <with|font-family|tt|[F6]>.<next-line>>>|<row|<cell|<postscript|Pics/Button_Toolbar_Draw_Eraser.png|*5/8|*5/8||||>>|<cell|<strong|Eraser
  -> The eraser will remove voxels from the shape. Note that clicking or
  dragging with the right mouse button has the same effect of erasing voxels.
  The eraser tool however proves its use in minute adaptations of shapes.
  Short cut: <with|font-family|tt|[F7]>.>>>>>

  <subsection|Drawing Styles>

  <name|BurrTools> has two different drawing styles. These styles affect the
  way voxels are drawn/erased or colours are added by <em|dragging> with the
  mouse. In drawing shapes by simply clicking
  'cell-<no-break>by-<no-break>cell' both are equivalent.\ 

  \;

  <tabular|<tformat|<cwith|1|2|1|1|cell-hyphen|t>|<cwith|1|2|1|1|cell-valign|t>|<cwith|1|2|1|1|cell-halign|r>|<cwith|1|2|2|2|cell-hyphen|t>|<cwith|1|2|2|2|cell-valign|t>|<cwith|1|1|2|2|cell-width|13cm>|<cwith|1|1|1|1|cell-width|2cm>|<table|<row|<cell|<postscript|Pics/Button_Toolbar_Drag_Rubber.png|*5/8|*5/8||||>>|<cell|<strong|Rectangular
  dragging style ('rubber band') -> On dragging over the 2-<no-break>D grid
  with the mouse just a <em|rectangular> <em|selection> of cells will be
  made. This is shown with a heavy border around the selected cells and the
  voxels will only be altered on releasing the mouse button. Releasing the
  mouse button outside the actual grid however will make the whole operation
  void and can serve as some kind of 'undo'. This style not only proves its
  use in drawing rectangular shapes or parts, but is extremely useful for
  adding colour to (large areas of) the shape.<next-line>>>|<row|<cell|<postscript|Pics/Button_Toolbar_Drag_Free.png|*5/8|*5/8||||>>|<cell|<strong|Free
  dragging style -> All drawing and colouring operations will be performed on
  a single cell basis and <em|as soon as> the mouse cursor is dragged over
  that particular cell. This drawing style is very useful for creating
  complex and irregular shapes and colour patterns.<next-line>>>>>>

  <no-indent>The status of these drawing styles is remembered by
  <name|BurrTools> so that it always defaults to the drawing style that was
  active on the last shut down of the program.

  <subsection|Compound Drawing Tools><label|CompoundDrawing>

  Although the basic drawing tools are all that is needed for creating
  shapes, some compound drawing tools are added to speed up the process. The
  compound drawing tools can be added <em|cumulatively> to the basic drawing
  tools and only extend the range of action for the latter ones.

  Note that these tools always go along the 3 orthogonal axes, so they are
  very useful for cubes but might needs a bit getting use to for the other
  spaces as they might behave differently along the 3 axes. The triangular
  prisms for for example are stacked along the z-axis, side by side along the
  x-axis and tip by tip along the y-axis.

  \;

  <tabular|<tformat|<cwith|1|3|1|1|cell-halign|r>|<cwith|1|3|1|1|cell-valign|t>|<cwith|1|3|2|2|cell-hyphen|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|1|1|1|cell-hyphen|t>|<cwith|2|2|1|1|cell-hyphen|t>|<cwith|1|1|1|1|cell-width|2cm>|<cwith|1|1|2|2|cell-width|13cm>|<table|<row|<cell|<postscript|Pics/Button_Toolbar_Symm_X.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Toolbar_Symm_Y.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Toolbar_Symm_Z.png|*5/8|*5/8||||>>|<cell|<strong|Symmetrical
  drawing methods -> For every voxel drawn, erased or coloured its
  symmetrically placed counterpart (with respect to the centre of the grid
  and along one of the space axes) will be affected as well. Activating only
  one of these options will double the number of edited cells, whereas
  activating two or all three will affect respectively four times and eight
  times as many cells simultaneously. These options are not only useful for
  drawing symmetrical shapes, but they are also very well suited for finding
  the centre of the grid and (temporarily) setting the extends of a
  shape.<next-line> >>|<row|<cell|<postscript|Pics/Button_Toolbar_Cols_X.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Toolbar_Cols_Y.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Toolbar_Cols_Z.png|*5/8|*5/8||||>>|<cell|<strong|Column
  drawing methods -> These options - possibly combined with the symmetrical
  drawing tools - can really speed up drawing shapes as they will affect
  <em|all> voxels that are in the same row or column along one of the space
  axes. The number of voxels that will be affected depends on the size
  settings of the grid. Hence, to take fully advantage of these functions the
  grid should be first adjusted to the proper dimensions.>>>>>

  <section|Adding Colour><label|AddingColour>

  There are basically two reasons for using colours in your puzzle designs.
  The first is merely <em|aesthetically> and colours are only used to explore
  the looks of the puzzle. This can help you selecting the proper species of
  woods or stains before taking your design to the workshop. The second
  however is far more important as it uses colours to force c.q. prevent
  certain positions of particular pieces in the assembly. These
  <em|constraining> techniques can be very useful to pursue a unique solution
  for a puzzle design. Of course one can try to achieve both the aesthetic
  and constraining goals at the same time. Figure<\float|float|tbf>
    <big-figure|<postscript|Pics/Window_DDD_1.png|*5/8|*5/8||||>|<label|FigureCustomColours>A
    shape with custom colours>
  </float> <reference|FigureCustomColours> shows an example of
  <name|Dracula's Dental Disaster> (Ronald Kint-Bruynseels) in which colours
  serve both. The red and black voxels are meant to impose constraints on the
  placements of the pieces, whereas the white colour of the parts on the
  inside of the pieces is only used to make them look nice.

  <subsection|The Neutral Colour and Custom Colours >

  Even when no 'special' colours at all are used, all created shapes do look
  different with respect to their 'colour'. This is the so called <em|neutral
  colour> and is only there to <em|distinguish> the shapes from one another.
  These neutral colours are standard for each newly created shape (the first
  one in the shapes list is always blue, the second one green, the third one
  red, etc...) and cannot be altered.

  As far as the solver is concerned, the neutral colour doesn't even exist as
  all appearances of it are fully interchangeable. So any voxel in the pieces
  that has only the neutral colour can go into any voxel of the result shape
  and every voxel in the result that has no other colour than the neutral can
  accommodate for any voxel of the pieces, independent of its colour.

  Independent from their neutral colour, voxels can have customised colours
  as extra attributes. To avoid confusion, it's recommended to have these
  colours well distinguishable from the neutral colours in use, since a
  custom colour that is identical to one of the neutral colours will have a
  completely different effect on the way the solver behaves. Almost without
  exceptions custom colours need some constraint settings
  (<with|mode|math|\<vartriangleright\>><reference|ColourConstraints>) to
  make the solver run.

  <subsection|Creating and Editing Custom Colours>

  The tools for creating and editing colours are located on the
  <with|font-family|ss|<strong|<with|color|blue|<with|color|black|Colours>>>>
  panel of the <with|font-family|ss|<strong|<strong|<with|font-family|rm|<with|font-series|medium|<with|font-family|ss|Entities>>>>>>
  tab. This panel also has a list in which the colours can be
  <with|font-series|medium|selected> to be used in the design or to become
  edited. <with|color||>The <strong|<with|font|roman|<with|font-family|ss|New>>>
  button allows you to create a custom colour. A dialogue will pop up and
  present you the necessary tools to create the colour you need. Accordingly
  the <with|font-family|ss|<strong|Edit>> button allows you to transform an
  already existing colour using a similar dialogue. This dialogue also shows
  the currently selected colour for comparison (unless the neutral colour is
  selected, which makes the dialogue to show the default medium grey). Note
  that the neutral colour can be neither removed or changed. It's important
  to realise that the <name|BurrTools> engine only discriminates custom
  colours by number as indicated in their prefix
  '<with|font-family|ss|<strong|C<em|x>>>' and not by the actual colours
  themselves. Hence it is possible to create identical colours that
  nevertheless will be treated as different colours. So, it's strongly
  advised to introduce only colours for which the difference can easily be
  discerned. Otherwise, finding out why a puzzle has no solutions can be very
  hard. The <strong|<with|font-family|ss|Remove>> button will not only
  discard the colour from the list, but will also remove it from any voxel
  that has it as an attribute by replacing it with the neutral colour.

  <subsection|Applying Colours><label|BrushTool>

  Colours can be applied while drawing the shape. Just select a colour and it
  will become an extra attribute of the fixed pen or the variable pen.
  Additional colouring can be done by using the <em|Brush> tool.

  \;

  <tabular|<tformat|<cwith|1|1|2|2|cell-valign|t>|<cwith|1|1|2|2|cell-hyphen|t>|<cwith|1|1|1|1|cell-valign|t>|<cwith|1|1|1|1|cell-halign|r>|<cwith|1|1|1|1|cell-width|2cm>|<cwith|1|1|2|2|cell-width|13cm>|<table|<row|<cell|<postscript|Pics/Button_Toolbar_Draw_Brush.png|*5/8|*5/8||||>>|<cell|<strong|<strong|Brush>
  tool -> This is a <em|'colouring only'> device and merely adds the selected
  colour to the voxels without altering their state. The brush tool can also
  be activated by pressing <with|font-family|tt|[F8]> on the
  keyboard.<next-line>>>>>>

  <no-indent>The behaviour of this brush tool is similar to that of the
  drawing pens. So it obeys the drag styles and can be extended with the
  compound drawing tools. Note that the right mouse button will still
  completely erase the voxel.

  <section|Representations><label|Representations>

  Voxels can either be fixed or variable and each of these can come with or
  without an additional custom colour. In <name|BurrTools> all of these have
  their own specific representations in the 2-<no-break>D grid as well as in
  the 3-<no-break>D viewport. Figure<\float|float|tbh>
    <\big-figure>
      <postscript|Pics/RepresentationA.png|*5/8|*5/8||||>
      \ \ \ \ <postscript|Pics/RepresentationC.png|*5/8|*5/8||||>
      \ \ \ \ <postscript|Pics/RepresentationE.png|*5/8|*5/8||||>

      <postscript|Pics/RepresentationB.png|*5/8|*5/8||||>
      \ \ \ \ <postscript|Pics/RepresentationD.png|*5/8|*5/8||||>
      \ \ \ \ <postscript|Pics/RepresentationF.png|*5/8|*5/8||||>
    </big-figure|<label|FigureRepresentations>Representations in
    2-<no-break>D and 3-<no-break>D>
  </float> <reference|FigureRepresentations> shows an overview of these. In
  this picture the neutral colour is red (= shape S3) and the custom colour
  is green (RGB = 0.600, 0.753, 0).

  Fixed voxels always fill the cell completely in the 2-<no-break>D grid as
  well as in the 3-<no-break>D grid. In all the pictures of Figure
  <reference|FigureRepresentations> the voxels on the left are fixed voxels.
  Variable voxels only fill the cell partially in 2-<no-break>D and have a
  black inset in 3-<no-break>D (the voxels on the right in Figure
  <reference|FigureRepresentations>).

  Voxels that have a custom colour added (the yellow voxels in Figure
  <reference|FigureRepresentations>) show this colour as an inset in the
  2-<no-break>D grid, whereas in the 3-<no-break>D viewer they are completely
  painted with this colour (provided that the
  <strong|<with|font-family|ss|Colour 3D View>> on the status line is
  checked, otherwise they will be painted in the neutral colour). Note that
  in both grids the neutral colours also have a slightly checkered pattern
  which can assist navigating in space (except for the spheres, they have no
  checkering).

  <section|Transformation Tools><label|TransformationTools>

  Editing complex shapes can be very cumbersome and requires often a lot of
  navigating through the 2-<no-break>D grid. So, properly positioning and/or
  orientating the shape in the 2-<no-break>D grid can save a lot of time.
  <name|BurrTools> comes with a set of functions that help you adjust the
  position and orientation of the shapes. These functions are grouped on the
  <with|font-family|ss|<strong|Transform>> subtab of the
  <strong|<with|font-family|ss|Edit>> panel (Figure<\float|float|tb>
    <\big-figure>
      <postscript|Pics/Subtab_TransformA.png|*5/8|*5/8||||>

      <postscript|Pics/Subtab_TransformB.png|*5/8|*5/8||||>

      <postscript|Pics/Subtab_TransformC.png|*5/8|*5/8||||>
    </big-figure|<label|FigureTransformationTools>Transformation tools>
  </float> <reference|FigureTransformationTools>). The first thing to see is
  that the transform tab looks quite different for all 3 available gridtypes.
  On the top of the figure you see the tab for cubes, blow for the triangles
  and at the bottom for spheres.

  \;

  <tabular|<tformat|<cwith|1|3|1|2|cell-hyphen|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|3|1|1|cell-valign|t>|<cwith|1|3|1|1|cell-halign|r>|<cwith|1|3|2|2|cell-width|13cm>|<cwith|1|3|1|1|cell-width|2cm>|<cwith|1|1|1|1|cell-width|2cm>|<table|<row|<cell|<postscript|Pics/Button_Transform_Flip_X.png|*5/8|*5/8||||><line-break><postscript|Pics/Button_Transform_Flip_Y.png|*5/8|*5/8||||><line-break><postscript|Pics/Button_Transform_Flip_Z.png|*5/8|*5/8||||>>|<cell|<strong|Flip
  -> These 'three' functions are merely <em|one single mirroring tool> and
  the only difference is the orientation of the mirrored shape they provide.
  The first will mirror the shape along the x-axis (or in a plane through the
  centre of the grid and parallel to the YZ-plane). The other do perform the
  same task, but along the y-axis (XZ-plane) or the z-axis (XY-plane)
  respectively. Note that each button can either undo its own action as well
  as the actions of the other buttons since the result of each function can
  be obtained by simply rotating the outcome of any other. However, there are
  three buttons to provide some control over the orientation of the mirrored
  shape in the grid space, which can have a time saving effect if the shape
  needs further editing.<next-line>>>|<row|<cell|<postscript|Pics/Button_Transform_Nudge_X.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Transform_Nudge_Y.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Transform_Nudge_Z.png|*5/8|*5/8||||><next-line>and
  more>|<cell|<strong|Nudge - >These functions provide <em|translations>
  (along the x-axis, y-axis or z-axis for the cubes or along different axes
  for other gridtypes) of the shapes in their surrounding grids. These
  buttons have two parts, of which the left part will shift the shape towards
  the origin of the grid and the right part will move it away from the
  origin. Note that shifting a shape beyond the boundaries of the grid will
  (partially) destroy it. So these nudging operations can also be used to
  erase unwanted parts on the outer limits of the
  shapes.<next-line>>>|<row|<cell|<postscript|Pics/Button_Transform_Rotate_X.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Transform_Rotate_Y.png|*5/8|*5/8||||><next-line><postscript|Pics/Button_Transform_Rotate_Z.png|*5/8|*5/8||||>>|<cell|<strong|Rotate
  -> These functions allow you to <em|rotate> the shapes around an axis
  parallel to the x-axis, y-axis or the z-axis. Again, these buttons have two
  parts, of which the left rotates the shape 90<degreesign> anti-clockwise
  (viewed towards the origin) and the right button turns the shape
  90<degreesign> clockwise. To avoid destroying shapes by rotating them the
  grid may become rotated as well.<next-line>The triangle space has only one
  rotation button for the x and y-axis because it is only possible to rotate
  by 180<degreesign> around these axes.>>>>>

  <section|Miscellaneous Editing Tools>

  The <with|font-family|ss|<strong|Tools>> subtab (Figure<\float|float|tbf>
    <big-figure|<postscript|Pics/Subtab_Tools.png|*5/8|*5/8||||>|<label|FigureTools>Extra
    editing tools>
  </float> <reference|FigureTools>) offers extra editing tools. Currently
  only some constraint related tools are available.

  <subsection|Constraining Tools><label|ChapterConstrainingTools>

  These tools are <em|mass editing> tools that somehow have an impact on the
  possible placements of the pieces in the final result. They act either on
  the inside or the outside of the shape. Voxels that are considered to be on
  the inside are voxels that have another voxel adjacent to <em|all> of their
  faces. Consequently, outside voxels have at least one empty voxel
  neighbouring.

  <tabular|<tformat|<cwith|1|1|2|2|cell-hyphen|t>|<cwith|1|1|1|1|cell-valign|t>|<cwith|1|1|1|1|cell-halign|r>|<cwith|2|2|1|1|cell-valign|t>|<cwith|2|2|1|1|cell-halign|r>|<cwith|3|3|1|1|cell-valign|t>|<cwith|3|3|1|1|cell-halign|r>|<cwith|2|2|2|2|cell-hyphen|t>|<cwith|3|3|2|2|cell-hyphen|t>|<cwith|1|3|2|2|cell-valign|t>|<cwith|1|1|2|2|cell-width|13cm>|<cwith|1|1|1|1|cell-width|2cm>|<table|<row|<cell|<postscript|Pics/Button_Constraints_Fixed.png|*5/8|*5/8||||>>|<cell|<strong|Fixed
  Inside/Outside -> These functions allow you to change the state of the
  voxels that are either on the inside (left button) or on the outside (right
  button) of the shape into fixed voxels. Although one can think of
  situations in which these can be useful as such, they are mostly used to
  undo the effects of the next two functions.<next-line>>>|<row|<cell|<postscript|Pics/Button_Constraints_Variable.png|*5/8|*5/8||||>>|<cell|<strong|Variable
  Inside/Outside -> These functions will respectively make all the voxels on
  the inside or the outside of the shape variable. Making the inside variable
  is very useful for puzzles with internal holes in undetermined places. On
  the other hand making the outside variable can prove its use in a lot of
  design situations (e.g. adding extensions to the pieces). Clicking both
  buttons will make the shape completely build out of variable voxels. Use
  these wisely as the more variable voxels there are, the slower the solver
  will run.<next-line>>>|<row|<cell|<postscript|Pics/Button_Constraints_Colour.png|*5/8|*5/8||||>>|<cell|<strong|Colour
  Remover -> These buttons will remove any custom colours from the voxels
  that are either on the inside or the outside of the shape and replaces them
  with the neutral colour. Removing the colour from the inside can prevent
  having to apply complex colouring to the result shape in situations were
  the colour constraints are only relevant to the overall appearance of the
  puzzle. >>>>>

  <section|Managing Shapes and Colours>

  Currently only the shapes can be rearranged with the left and right arrow
  buttons of the <with|font-family|ss|Shapes> section, but more advanced
  managing procedures will be added in the future.

  <section|Shape Information><label|ChapterStatus>

  When using the main menu entry <with|font-family|ss|<strong|Status>> a
  window (Figure<\float|float|tb>
    <big-figure|<postscript|Pics/Window_Status.png|*5/8|*5/8||||>|<label|WindowStatus>The
    Status window>
  </float> <reference|WindowStatus>) like the one above opens and displays
  all kinds of information about all the shapes available inside the puzzle.
  The table columns have the following meanings:

  <\description-compact>
    <item*|Units Normal>Contains the number of voxels inside the shape that
    have the state fixed.

    <item*|Units Variable>Contains the number of voxel inside the shape that
    have the state variable.

    <item*|Units Sum>Contains the number of voxels inside the shape that are
    either fixed or variable.

    <item*|Identical>If the shape is identical to another shape with smaller
    number the first one of these number is displayed, so if shape 3, 4 and 5
    are identical shape 4 and 5 will point to shape 3 but shape 3 will show
    none. So the table only points to a shape above.

    <item*|Identical Mirror>A shape is entered, if the shapes can somehow be
    transformed into the other including the mirror transformation

    <item*|Identical Shape>A shape is entered, if the shape is identical
    without including mirrored shapes.

    <item*|Identical Complete>In this case shapes must be completely
    identical including colours and not only the appearance of the shape.

    <item*|Connectivity>This part of the table shows if the shape is
    completely connected and doesn't contain any separate voxels

    <item*|Connectivity Face>This part is marked with an X when all parts of
    the shape are connected via the faces of the voxels

    <item*|Connectivity Edge>This part is marked with an X when all parts of
    the shape are connected via an edge or a face of the voxel

    <item*|Connectivity Corner>This part is marked with an X when all parts
    of the shape are connected via a corner, an edge or a face

    <item*|Holes>This part of the table contains information about possible
    holes inside the shapes.

    <item*|Holes 2D>A 2D hole is a hole in the shape, if the shape would be 2
    dimensional. So the o-octomino has a 2D hole.

    <item*|Holes 3D>A 3D hole is a completely surrounded region inside a
    shape.

    <item*|Sym>This is a column that is mainly there for my help.
    <name|BurrTools> needs to know about all kinds of symmetries a shape can
    have. If a shape turns up that has a kind of symmetry yet unknown to the
    program it can not solve puzzles with this shape. So here is a tool to
    check beforehand and without the need to create a problem. If you ever
    see a mark in the last column send me the shapes where it turns up. As
    long as this last column is empty everything is fine.
  </description-compact>

  <section|Tips and Tricks>

  Below are some tips and tricks that can be useful to simplify your designs,
  speed up the designing and/or solving process, or can be used as
  workarounds for some limitations of <name|BurrTools.> We encourage the
  reader to share his own tips and tricks with us so that we can incorporate
  them in a future update of this document.

  <subsection|Voxel State and Size Tips>

  <\description-compact>
    <item*|State and Solver Speed>The more variable voxels (as compared to
    the total number of voxels) there are in the result shape the slower the
    solver will run. Also the number of pieces has an impact on the solving
    time. Hence, replacing variable voxels with empty spaces for determined
    holes in the puzzle is to be considered. Also leaving out a piece in
    complex packing puzzles (and making its position in the result empty) can
    reduce the solving time considerably.

    <item*|Size and Solver Speed>Also the size of the shapes has an effect on
    the solving speed, since bigger shapes inevitably lead to more
    possibilities: for a 1<with|mode|math|\<times\>>1<with|mode|math|\<times\>>1
    cube there's only one possible placement in a
    2<with|mode|math|\<times\>>2<with|mode|math|\<times\>>2 grid (excluding
    symmetries), but for a 2<with|mode|math|\<times\>>2<with|mode|math|\<times\>>2
    cube there are four of them in a 4<with|mode|math|\<times\>>4<with|mode|math|\<times\>>4
    grid. So trying to minimise <em|all> shapes with the 1:1 tool before
    taking the puzzle to the solver is highly recommended for complex
    designs.

    <item*|Complete sets>Often complete sets of pieces (e.g. the hexacubes in
    <name|Haubrich's Cube>) can be easily made by repeatedly copying the
    current shape and editing it with the properties of left and right
    clicking.

    <item*|Symmetry>A detailed treatment of some symmetry issues will be
    added to the next update of this document.
  </description-compact>

  <subsection|Colouring Tips>

  <\description-compact>
    <item*|Colouring Shapes>Colouring shapes as a whole is easily done with
    the brush tool in combination with the rectangle dragging style and
    z-<no-break>columns switched on.

    <item*|Aesthetic Colours>When colours are solely used for aesthetic
    reasons make sure that the <em|result> shape has only the neutral colour.
    This will prevent having to set a lot of constraint conditions.

    <item*|One-Sided Polyominoes>Polyominoes can be made one-sided by having
    them two layers high and adding different constraint colours to both the
    layers. The constraint settings (<with|mode|math|\<vartriangleright\>><reference|ColourConstraints>)
    should simply be a 'one-<no-break>to-<no-break>one' relationship.

    <item*|Hiding Pieces>For puzzles in which the goal is to hide a certain
    piece on the inside of the assembly (e.g. Trevor Wood's <name|Woodworm>)
    <em|two> constraint colours should be used. One for the exterior and one
    for the voxels on the inside of the result shape. Also colour the piece
    that must be hidden with this 'inside' colour and apply the 'outside'
    colour to all other pieces. The constraint settings
    (<with|mode|math|\<vartriangleright\>><reference|ColourConstraints>) must
    then be such that the piece to be hidden is only allowed to go into the
    'inside' colour and the other pieces may go into either colour.
  </description-compact>

  <section|Simulating non-cubic spacegrids>

  It is possible to emulate spacegrids different from cubes by just using
  cubes. This way <name|BurrTools> can solve different kind of puzzles. This
  section will give hints of how to things. It will not contain obvious
  emulation possibilities like hexagons with 6 triangles or x by y rectangles
  using several squares, but rather the more complicated possibilities. The
  chapter can not be complete but it rather wants to show what can be done
  and give you some initial ideas. If you come up with a cool idea you are
  welcome to send it to me and I will include it in here.

  Generally this emulation requires to use more cubes for one basic unit.
  This will probably result in a slowdown of the solving process. But this
  slowdown is not always that grave. <name|BurrTools> knows how to merge
  voxels that are always occupied by the same piece into one, so if there is
  for example a puzzle that uses hexagonal pieces made out of the triangular
  prisms and these hexagons are always within a hexagonal grid
  <name|BurrTools> will merge the 6 triangle together and work with the
  resulting shapes. This only takes some time at the initialisation phase. On
  the other hand there might be many placements of pieces that fit the
  underlying cube to triangle grid that are not proper placements and that
  need to be sorted out first. This can take a long time. <name|Major Chaos>
  by Kevin Holmes for example has a lot of illegal placements for pieces that
  need to be sorted out. That takes a very long time, but once that is done
  the solving is actually very fast.

  <subsection|Two-Sided Pieces>

  <\float|float|tbhf>
    <big-figure|<postscript|Pics/Emulation_2Layer.png|*7/8|*7/8||||>|Emulate
    2 Sided Piece<label|Emul2Sided>>
  </float>If you have pieces that have a top and a bottom there are several
  possibilities to model that in <name|BurrTools>. One possibility is to use
  colours. Make the piece and the result 2 layers thick. The bottom layer of
  both will get a special colour.

  Another possibility is to add an additional layer that has voxels only in
  certain places as seen in the picture. The additional voxel prevents the
  rotation of the shape. But you have to make sure that the allowed rotations
  are still possible, e.g. if you place the notches in different places
  rotation around the z-axis is also no longer possible. An example can be
  seen in Figure <reference|Emul2Sided>

  <subsection|Diagonally Cut Cubes and Squares>

  Cubes can be cut in many different ways, the cut that results in shapes
  such as given in Figure <\float|float|tbh>
    <big-figure|<postscript|Pics/Emulation_DiagonalCut.png|*7/8|*7/8||||>|<label|EmulDiagCut>Diagonally
    cut cube>
  </float> <reference|EmulDiagCut> can be emulated using cubes as seen in the
  image.

  It is, of course, also possible to simulate diagonally cut squares this
  way. The squares need to be 2 layers thick.

  <subsection|Cairos>

  Cairos are pentagons but luckily they have only 4 rotations.

  <subsection|Squares with Cuts of Slope 0.5>

  <subsection|Edge Matching>

  Sometimes it is possible to emulate edge matching problems by using notches
  and dents at the outside of the shapes.

  <subsection|Other possibilities>

  There are many other shapes that can be emulated. As one example I will
  show 2 ways to emulate William Waites <name|Knit Pagoda> (see
  Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Emulation_Pagoda.png|*6/8|*6/8||||>|<label|EmulPagoda1>The
    Knit Pagoda>
  </float> <reference|EmulPagoda1>). Additionally to the shape the pieces
  have an upside and a bottom. Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Emulation_Pagoda2.png|*7/8|*7/8||||>|<label|EmulPagoda2>Emulation
    for one of the <name|Knit Pagoda> Pieces>
  </float> <reference|EmulPagoda2> shows 2 possible ways to emulate these
  pieces. Both shapes emulate the T-shaped piece seen on the right bottom.

  It is quite easy to see that the pink shape working. It is constructed
  starting with a 3x3x1 square and adding a cube at the centre of one shape
  if that side is bulged outward and removing one cube, when the side is
  bulging inwards. Finally add a cube at the centre of the 3x3 square to make
  it unflippable.

  The second is quite a bit more complicated to understand. Here the starting
  point is a 2x2 square. A cube is added or removed for the bulges just as in
  the other case but those cubes can not be in the middle. They are at one
  side so that the cube from an outer bulge can go into a gap created by an
  inner bulge. The resulting shape for one unit contains 4 cubes along a
  zig-zag line. You can see it by looking for the lighter cubes in the
  turquoise shape above. This ways has the additional advantage of avoiding
  flips because when the piece is flipped over the orientation of the bulges
  changes and the cubes do not mesh.

  <\with|par-mode|right>
    <chapter|Defining Puzzles><label|ChapterPuzzles>
  </with>

  Typically a puzzle problem in <name|BurrTools> consists of a collection of
  pieces (shapes) and a goal, say another shape that the pieces should form
  when correctly assembled. This is what we call a <em|simple problem
  definition>. Note that it may well be not that 'simple' to solve it in real
  life. More elaborated or <em|complex puzzle problems> contain also colour
  constraints and/or grouped pieces.

  As stated before, a puzzle can be a collection of problems, either simple,
  complex or a mixture of both. The <with|font-family|ss|<strong|Puzzle>> tab
  (Figure<\float|float|tb>
    <big-figure|<postscript|Pics/Window_DDD_2.png|*5/8|*5/8||||>|<label|FigurePuzzleTab>Defining
    problems on the Puzzle tab>
  </float> <reference|FigurePuzzleTab>) provides all the tools needed to
  build a variety of puzzle problems that are suited for the
  <with|font-family|ss|Solver>.

  <section|Defining Simple Problems>

  As defined above, a simple puzzle problem consists only of a collection of
  pieces and a result shape that can be assembled (and preferably also be
  disassembled) with these pieces (Figure
  <reference|FigureSimplePuzzleProblem>). Bear in mind that a simple problem
  also implicates that <em|all> the pieces can be separated from one another.
  It takes only two steps (which are also required for complex problems) to
  create such a problem: <em|initialising> the problem and <em|assigning>
  shapes to the pieces and the result.

  <subsection|Initialising Problems><label|InitialisingProblems>

  The first step is to <em|initialise> the problem(s). All the tools to do so
  are just below the <with|font-family|ss|<strong|Problems>> caption. Just
  like with shapes this can be done by clicking the
  <with|font-family|ss|<strong|New>> button to start a completely new one, or
  by using <with|font-family|ss|<strong|Copy>> to edit a previously created
  problem definition without destroying the first. Accordingly, problems can
  be removed with the <with|font-family|ss|<strong|Delete>> button. All
  problems find their place in the problems list below these buttons and are
  identified with a '<with|font-family|ss|<strong|P<em|x>>>' prefix to which
  a more meaningful description can be added by clicking the
  <with|font-family|ss|<strong|Label>> button.<with|mode|math|> Also the
  methods for selecting and rearranging problems are similar to their
  counterparts on the <with|font-family|ss|Entities> tab and need no further
  explanation here.

  <subsection|Piece Assignment><label|PieceAssignment>

  Until now we dealt with shapes as rather abstract concepts. Only by
  <em|<em|<em|assigning>>> these shapes to the pieces or the goal of a puzzle
  they become meaningful. All available shapes are presented in the top list
  of the <with|font-family|ss|<strong|Piece Assignment>> panel in which they
  can be selected and be given their purpose in the puzzle. Since a strict
  distinction is made between shapes and pieces, it's not necessary that all
  shapes are used in a single problem or in any problem at all.

  <\float|float|tb>
    <big-figure|<postscript|Pics/Window_Clarissa.png|*5/8|*5/8||||>|<label|FigureSimplePuzzleProblem>A
    simple puzzle problem with multipieces>
  </float>Although not mandatory, it's probably best to assign the result
  shape first: select the appropriate shape and click
  <with|font-family|ss|<strong|Set Result>>. The result shape is then
  depicted in the top left part of the 3-<no-break>D viewport (which also
  shows a smaller example of the currently selected shape) and the status
  line shows some information about the problem at hand. Next, any other
  shape can be assigned to the pieces of the puzzle by selecting it and
  clicking <with|font-family|ss|<strong|+1>><with|font-family|ss|<strong|>>.
  This adds a single copy of the shape to the second list which holds all the
  shapes used as pieces. If multipieces are involved, just add as many
  instances of the shape as required by the same means. In the list of pieces
  any multipiece has an instance counter added - between brackets - to its
  identifier. A single instance of every shape used in the puzzle is shown in
  the lower part of the 3-<no-break>D viewer. To make corrections, pieces can
  be removed from the puzzle by selecting them (they also can be selected by
  clicking them in the pieces list) and clicking
  <strong|<with|font-family|ss|-<no-break>1>>. Again, this only removes a
  single instance and needs to be repeated for removing multipieces.

  Most of the time it is necessary to add one instance of all defined shapes
  to the puzzle. If there are a lot of them this can take while. This is what
  the <with|font-family|ss|<strong|+1 each>> button is for. It increases the
  piece counter for each shape (except the one assigned for the result) by
  one. Or it adds a first instance of the shape to the problem. The
  <with|font-family|ss|<strong|Clr>> button removes all pieces from the
  problem.

  Since it doesn't make sense to have a certain shape to be result and piece
  at the same time, the shape set as result cannot be added to the list of
  pieces. Consequently, assigning a shape that's already in the list of
  pieces to the result will remove it from the list.

  Whenever the total number of cubes in the pieces is within the boundaries
  set by the result shape (which can be inspected on the status line) this
  kind of simple puzzle problems can be taken to the solver. Note that the
  solver won't run when one or more pieces contain any variable voxels.

  <section|Grouping Pieces>

  Something we deliberately haven't mentioned in the description above is the
  fact that the solver will halt whenever it is unable to separate some
  pieces from each other. In other words, the solver will attempt to separate
  <em|all> the pieces from each other and reports that no solution exists
  when it fails to do so. This is just what is required for most puzzles as
  you need to have single pieces as a starting point. But there are a few
  puzzles for which you have groups of pieces that are <em|movable> but
  <em|not separable>. Here the piece groups come in handy. Probably everyone
  familiar with <name|PuzzleSolver3D> ever experienced the futile attempts of
  that program trying to solve such designs by nearly endlessly shifting the
  entangled pieces back and forth. Not so with <name|BurrTools> as piece
  groups allow you to tell the disassembler that it is OK when it cannot
  separate a few pieces from one another.

  <subsection|Concept>

  When the disassembler finds two or more pieces that cannot be taken apart
  it checks whether all of the involved pieces are in the same group. If
  that's the case it rests assured and continues. If the pieces are <em|not>
  in the same group the disassembler aborts its work and reports that the
  assembly can not be disassembled. This is the basic idea, but there is a
  bit more to it.

  <subsubsection|Complete Disassembly>

  A special case is <em|'Group-<no-break>0'>. All pieces in this group
  <em|need to be separated> from each other. This group is included so that
  it is not required to place all the pieces into their own group, when you
  want to completely disassemble the puzzle. Pieces automatically go into
  Group-<no-break>0, so you don't need to take care of that. As a matter of
  fact you won't even find any reference to that Group-<no-break>0 in the
  GUI.

  <subsubsection|Basic Piece Grouping>

  On the other hand, when dealing with puzzles of which is known that certain
  pieces (say S<em|a> and S<em|b>) can't be separated from each other,
  grouping these pieces will cause the solver to report a valid disassembly
  for which the grouped pieces are treated as a single piece. Be it not a
  rigid piece since the parts can freely (within certain boundaries) move
  with respect to each other.

  <\with|par-mode|center>
    {S<em|a>, S<em|b>}

    Group-<no-break>1 <with|mode|math|\<rightarrow\>> S<em|a>+S<em|b>
  </with>

  Of course this technique can also be used (in a truly designing situation)
  for pieces that <em|may> be entangled. If these pieces are indeed
  inseparable the solver will report so, but if they can be separated the
  solver may report the complete disassembly as well:\ 

  <\with|par-mode|center>
    {S<em|a>, S<em|b>} ?

    Group-<no-break>1 <with|mode|math|\<rightarrow\>> S<em|a>+S<em|b>

    Result: {S<em|a>, S<em|b>} and/or S<em|a>, S<em|b>
  </with>

  Now for the hard part: <em|pieces can be in more than one group>. If you
  have e.g. a puzzle for which you know that piece S<em|a> either interlocks
  with piece S<em|b> or piece S<em|c> and cannot be separated from it, but
  you don't know which of those (S<em|b> or S<em|c>) piece S<em|a> is
  attached to, you can assign Group-<no-break>1 to S<em|a>+S<em|b> and
  Group-<no-break>2 to S<em|a>+S<em|c>:

  <\with|par-mode|center>
    {S<em|a>, S<em|b>} or {S<em|a>, S<em|c>}

    Group-<no-break>1 <with|mode|math|\<rightarrow\>> S<em|a>+S<em|b>

    Group-<no-break>2 <with|mode|math|\<rightarrow\>> S<em|a>+S<em|c>
  </with>

  This way the disassembler detects that both pieces are in Group-<no-break>1
  when S<em|a> and S<em|b> are inseparable and it finds that both pieces are
  in Group-<no-break>2 when S<em|a> and S<em|c> cannot let go from each
  other. In both cases the solver will report a valid disassembly. However,
  if S<em|b> and S<em|c> are entangled the solver is not able to find a valid
  disassembly.

  <subsubsection|Grouping Multipieces>

  All instances of a multipiece need to have the same group assignment, but
  you can instruct how many of these may be in a group <em|maximally>. That
  means you can make statements like 'not more than 3 pieces of S<em|n>
  <em|may> be in Group-<no-break>1':

  <\with|par-mode|center>
    S<em|a<with|mode|math|<rsub|1>>>, S<em|a><with|mode|math|<rsub|2>>, ...
    S<em|a><with|mode|math|<rsub|n>>

    Group-<no-break>1 <with|mode|math|\<rightarrow\>>
    S<em|a<with|mode|math|<rsub|1>>>+S<em|a><with|mode|math|<rsub|2>>+S<em|a><with|mode|math|<rsub|3>>
  </with>

  Now how does it all come together? The disassembler starts to do its work.
  For each subproblem (a subproblem is a few pieces that it somehow has to
  get apart) it first checks if there is a unique group assignment for all
  involved pieces - i.e. all pieces have exactly one group assigned and that
  group is the same for all of them - it doesn't even attempt to disassemble
  that subproblem.

  \ If this is not the case it tries to disassemble. In case of a failure it
  adds the pieces that are in this subproblem to a table of lists of pieces.
  This is an array and each entry contains a list of pieces. Once done with
  the disassembler the program comes back to this table and tries to assign a
  group to each of the lists of pieces in the array. It just checks all
  possibilities by comparing the entries of the table with the group
  assignments made by the user. Whenever the sum of pieces (of a certain
  shape S<em|x>) in such a 'problematic' table entry is bigger than the value
  the user designated to that particular piece, no valid group assignment can
  be made. If the program can find a valid assignment the puzzle is
  disassembled, if it can not the puzzle is assumed to be not disassemblable.

  <subsubsection|Example>

  Assume we have a puzzle that contains (among others) 5 pieces of shape
  S<em|a>. Three of them might go into Group-<no-break>1 and another 2 into
  Group-<no-break>2. There is also a piece S<em|b> that might go into
  Group-<no-break>1:

  <\with|par-mode|center>
    Group-<no-break>1 <with|mode|math|\<rightarrow\>>
    S<em|a<with|mode|math|<rsub|1>>>+S<em|a><with|mode|math|<rsub|2>>+S<em|a><with|mode|math|<rsub|3>>+S<em|b>

    Group-<no-break>2 <with|mode|math|\<rightarrow\>>
    S<em|a><with|mode|math|<rsub|1>>+S<em|a><with|mode|math|<rsub|2>><em|>
  </with>

  <no-indent>After the disassembler ran we have the following lists of pieces
  in the table:\ 

  <\enumerate-numeric>
    <item>S<em|a>, S<em|a>

    <item>S<em|a>, S<em|a>, S<em|b>
  </enumerate-numeric>

  Now the program has to assign Group-<no-break>2 to the first set of pieces
  and Group-<no-break>1 to the second set of pieces. Because otherwise piece
  S<em|b> would be in the wrong group, it can only be in Group-<no-break>1.
  If there would be another piece S<em|a> in the first set it would not be
  possible to assign groups because we can only have two pieces S<em|a> in
  Group-<no-break>2. But it would be possible to have another piece S<em|a>
  in the second set.

  We have no idea how useful this might be with puzzles as most of the
  currently available puzzles require a complete disassembly. But who knows,
  maybe this feature will help in the design of lots of puzzles new and crazy
  ideas.

  <subsection|Creating Piece Groups>

  Although the above may sound complicated, implementing piece groups is
  actually very simple. All actions take place in the
  <with|font-family|ss|<strong|Group Editor>> (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Form_Group_Editor.png|*5/8|*5/8||||>|<label|FigureGroupEditor>The
    Group Editor>
  </float> <reference|FigureGroupEditor>) which becomes activated by clicking
  the <with|font-family|ss|<strong|Group>> button. Initially the
  <with|font-family|ss|Group Editor> shows a tabulated overview of the pieces
  used in the problem. The first column (<with|font-family|ss|Shape>) lists
  the pieces by their prefix and name, the second (<with|font-family|ss|n>)
  enumerates the instances of each. Note that it is possible to add or remove
  instances by changing these <with|font-family|ss|n>-values.

  Creating piece groups is straightforward as the
  <with|font-family|ss|<strong|Add Group>> button simply adds a new group to
  the problem. Each new group gets its own column (<with|font-family|ss|Gr
  1>, <with|font-family|ss|Gr 2>, etc...) in which one can specify the
  <em|maximum> number of instances of a certain piece that can go in that
  particular group. Just click on a cell and it will become an input box.
  Cells that contain a value <no-break>\<gtr\> <no-break>0 will receive the
  neutral colour of the corresponding shape, cells with zero are grey and no
  number is shown. Any group that has no values at all in its column will be
  deleted on closing the <with|font-family|ss|Group Editor>. Hence, deleting
  all the values of a previously made group will remove the group even if its
  column stays present in the <with|font-family|ss|Group Editor>.

  <section|Setting Colour Constraints><label|ColourConstraints>

  The <strong|<with|font-family|ss|Colour Assignment>> panel
  (Figure<\float|float|tbf>
    <big-figure|<postscript|Pics/Panel_Colour_Assignment_A.png|*5/8|*5/8||||>
    \ \ \ \ <postscript|Pics/Panel_Colour_Assignment_B.png|*5/8|*5/8||||>|<label|FigureColourAssignment>Colour
    assignment>
  </float> <reference|FigureColourAssignment>) also has two lists. The first
  one shows all the available custom colours and allows selecting a certain
  colour for which then some relations can be set. These relations simply
  indicate which colour(s) in the result can accommodate for which colour(s)
  in the pieces. By allowing certain combinations (which is in fact
  prohibiting all other combinations) constraints are imposed on the
  theoretically possible placements of the pieces. These relationships are
  shown and constructed in the second list. This list has three columns of
  which the first shows the 'piece colours', the last shows the 'result
  colours' and the one in between clearly depicts the relationships by a
  series of arrows pointing from the piece colours to the result colours. The
  list is either sorted by the piece colours or by the result colours. The
  buttons <with|font-family|ss|<strong|Sort by Piece>> and
  <with|font-family|ss|<strong|Sort by Result>> switch between these two
  views.

  When <em|sorted by piece> (the left part of Figure
  <reference|FigureColourAssignment>), the bottom list is showing you that
  every voxel of the pieces with colour C<em|x> can go into every voxel of
  the result that has one of the colours on the end points of the arrows
  starting from Cx. When <em|sorted by result> (on the right in Figure
  <reference|FigureColourAssignment>), the list shows which piece colours
  will be allowed to go in a particular colour of the result.

  To set these relationships, first click the piece colour (or result colour,
  depending on the sorting method) for which you want to set the constraints.
  This will activate the 'relations line' for that particular colour which is
  indicated with a dark surrounding box (note that clicking anywhere on this
  relations line has the same effect). Next, the down and up pointing arrows
  will respectively add or remove the colour selected in the top list to or
  from the constraint settings.

  <section|Managing Problems>

  Currently puzzle problems can only be rearranged with the left and right
  arrow buttons of the <with|font-family|ss|Problems> section, but more
  advanced managing procedures may be added in the future.

  <section|Tips and Tricks>

  Some tricks and tips will be added to the next update of the user guide.

  <subsection|Grouping Tips>

  <subsection|Constraint Tips>

  <\with|par-mode|right>
    <chapter|Solving Puzzles><label|ChapterSolver>
  </with>

  Solving puzzles is what <name|BurrTools> is really about. Without its
  solving engine the program would be nothing more than a simple tool for
  drawing a very specific kind of 3-<no-break>D objects... a task a lot of
  other software is no doubt even better suited for.

  Solving puzzles is very straightforward with BurrTools even if the
  <with|font-family|ss|<strong|Solver>> tab (Figure<\float|float|tbf>
    <big-figure|<postscript|Pics/Window_DDD_3.png|*5/8|*5/8||||>|<label|FigureSolver>Solving
    puzzles>
  </float> <reference|FigureSolver>) has quite a some controls. On top there
  is the <with|font-family|ss|Parameters> panel, that contains a list
  allowing you to select a specific problem to be solved, provides option
  settings for the solver and has a series of buttons to direct the solving
  process. Finally, some information of the ongoing solving process is
  presented.

  A second panel (<with|font-family|ss|Solutions>) has the tools to browse
  the different solutions found, animate the moves to disassemble the puzzle
  to <em|inspect> the solutions in detail and to organize found solutions.

  <section|Solver Settings>

  In order to make the solver run a problem must be selected first. A list of
  all previously defined problems is available right below the
  <with|font-family|ss|<strong|Parameters>> caption. Selecting problems to be
  solved is \ similar to selecting shapes, colours or problems on the other
  tabs. Note that only the selected problem will be solved and that solving
  one problem will preserve the results of any already solved or partially
  solved problem. Currently there are the following options for the solver.
  All deal with the kind of information the solver will report.

  <\description-compact>
    <item*|<with|font-family|ss|Solve Disassembly>>When checked the solver
    will also try to disassemble the assemblies found and only those that
    indeed can be disassembled will be added to the list of solutions. If
    this option is left unchecked, the solver will merely search for all
    <em|theoretically> possible assemblies, i.e. assemblies for which the
    pieces do not overlap. Since solving disassemblies takes time (and often
    far more than assembling), it's recommended to leave this option
    unchecked for puzzles that always can be disassembled (e.g.
    <name|Pentomino> problems and other packing problems). For that kind of
    puzzles running the disassembler would only slow down the process without
    any gain in information. Also saving and loading the disassembly
    instructions takes a lot of time and memory, so if they are not really
    needed they are just a waste of time.

    <item*|<with|font-family|ss|Just Count>>When checked the solver will only
    count the number of solutions it will drop the found solutions right
    after they were found. Check this option if you're only interested in the
    number of solutions and not in the solutions themselves.\ 

    <with|font-family|ss|<item*|Drop Disassm>>When checked the program
    checks, if the found assembly is disassembable and discards the solution
    if it is not disassembable. But the disassembly is <em|not> stored, only
    the assembly and some information <em|about> the disassembly (like its
    level). This is useful if you have a problem that has many solutions and
    you want to find the most interesting solutions. Disassemblies take up a
    lot of memory within the computer so it is useful to just save some
    information while solving the puzzle and then later on, when everything
    is finished recalculate the disassemblies for the interesting solutions.

    <with|font-family|ss|<item*|Sort by>>This option lets you choose in which
    way the found solutions are ordered. There are 3 possibilities:

    <\enumerate-numeric>
      <item>Unsorted: The solutions are sorted into the list in the order in
      they are found.

      <item>by Level: The solutions are sorted by the level. First the number
      of moves to remove the first piece, if that is identical then by the
      moves for the second piece, and so on.

      <item>by number of moves to disassemble: The solutions are sorted by
      the sum of all moves required to completely disassemble the puzzle.
    </enumerate-numeric>

    <with|font-family|ss|<item*|Drop>>If a puzzle has very many solutions it
    might not be possible or even necessary to save all of them. E.g for
    polyomino-like puzzles it might be nice to keep just every 1000 of the
    millions of solutions to have a profile of the possible solutions. Here
    you can specify every how many-th solution you want to keep. A 1 means
    you keep every solution, a 100 means you keep the first and the 101st and
    the 201st and so on.

    <with|font-family|ss|<item*|Limit>>Limits the number of solutions to be
    saved. There will never be more than the specified amount of solutions in
    the list. When the list is full the program has 2 choices:\ 

    <\enumerate-numeric>
      <item>Solutions are sorted: The programs throws away the solutions at
      the end. So low level solutions are removed

      <item>Solutions are unsorted: The program starts to throw away every
      second solution. So when you started with a drop-value of one and the
      list is full the program starts to drop every 2nd solution is finds and
      only adds every 2nd solution to the list. But for each added solution
      it also removes every 2nd solution that already has been added to the
      list. After a while the list contains only every 2nd solution then the
      program only adds every fourth solution and removes again every 2nd
      solution in the list which result in only every fourth solution ending
      in the list. This sounds complicated but what is does is that is makes
      sure you have an nice crossection of all the solutions found until then
      and not just the first or last.
    </enumerate-numeric>
  </description-compact>

  <section|Solving Puzzles>

  Next to the solver options are some buttons to direct the solving process.
  Problems can be solved either in an automatic way or in a (manually)
  step-<no-break>by-<no-break>step manner.

  <subsection|Automatic Solving>

  An automatic search will proceed until all solutions, i.e. assemblies and
  disassemblies (when requested) are found. To begin an automated search
  click the <with|font-family|ss|<strong|Start>> button. Typically the
  solving process consists of a preparation phase followed by several cycles
  of assembling and disassembling. The latter one is of course omitted when
  the <with|font-family|ss|Solve Disassembly> option is left unchecked.

  The automatic solving process can also be interrupted by clicking
  <with|font-family|ss|<strong|Stop>>, but often the solver needs to finish
  some tasks first before it can actually halt
  (<with|mode|math|\<vartriangleright\>><reference|SolverInformation>). Any
  interrupted solving process can be saved to the puzzle file and be resumed
  in another session with <name|BurrTools>. In fact, on loading such a
  partially solved puzzle <name|BurrTools> will inform you about the
  possibility to continue with the search for solutions. When the solver is
  interrupted the shapes (<with|mode|math|\<vartriangleright\>>Chapter
  <reference|ChapterShapes>) and/or the problems
  (<with|mode|math|\<vartriangleright\>>Chapter <reference|ChapterPuzzles>)
  can be edited. If no editing whatsoever of these has been done the solving
  process can be simply resumed (<with|font-family|ss|<strong|Continue>>),
  otherwise you need to start all over again. But keep in mind the this
  saving of the internal state of the solver is very version dependent. So it
  is likely that a new version of <name|BurrTools> can not resume solving a
  puzzle saved with an older version. So it is good practice to finish
  solving jobs with one version of <name|BurrTools> before updating to the
  next.

  When the solver is running it provides a lot of information about its
  current state (what it is doing) and an estimate of the time it will need
  to finish the search. All this information is presented on six lines
  immediately below the solver control buttons (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/SolverProgress.png|*5/8|*5/8||||>|<label|FigureSolverInformation>The
    solver information>
  </float> <reference|FigureSolverInformation>).

  <subsubsection|Solver Progress Information>

  The first line of the solver information is a progress bar indicating the
  percentage of work it has done. The fifth
  (<with|font-family|ss|<strong|Time Used>>) and the sixth
  (<with|font-family|ss|<strong|Time Left>>) line respectively show the time
  already spend on the search and an estimate of the time still needed to
  finish the solving process. Note that the latter one and also the
  information about the percentage done are <em|very rough estimates> since
  these are based on the possible placements of the pieces already tested and
  still to test. However, the possible placements to be tested are constantly
  fluctuating as they are determined by the positions of previously placed
  pieces (<with|mode|math|\<vartriangleright\>><reference|BrowsingPlacements>).

  <subsubsection|Solver State Information><label|SolverInformation>

  Probably most important is the <with|font-family|ss|<strong|Activity>> and
  result information provided by the solver. The
  <with|font-family|ss|Activity> line not only tells you what the solver is
  currently doing, but it also whether the solver can be interrupted or not.
  The following is an overview of the activities of the solver:

  <\description-compact>
    <item*|nothing>This indicates that the solver is ready to be started
    (provided a valid problem is selected) and that no information is
    available about earlier attempts to solve the selected problem.

    <item*|prepare>The solver is creating the internal data structure for the
    assembler. This structure is more or less a listing of all the possible
    places that all the pieces can go to.

    <item*|optimize piece <em|n>>In this second stage of the preparation the
    placements for each piece are tested for plausibility. Some placements
    are just nonsense in a way that they result in unfillable holes or
    prevent the placement of other pieces. These placements are removed from
    the data structure (<with|mode|math|\<vartriangleright\>><reference|BrowsingPlacements>).

    <item*|assemble>The program is currently searching for assemblies.

    <item*|disassemble>An assembly was found and is now tested for
    disasembability.

    <item*|pause>A search was started and interrupted.

    <item*|finished>The search was completed, all found solutions, ordered by
    the set up sorting criterium, can be inspected
    (<with|mode|math|\<vartriangleright\>><reference|InspectingResults>).

    <item*|please wait>The user wanted to stop the search, but the program
    still has to finish what it is doing right now. Only the assembler is
    interruptible. The preparation and optimisation stages need to be
    finished. The disassembly search also has to be finish first.

    <item*|error>Something is wrong with the puzzle and an error message,
    providing more specific information on the error, is usually
    displayed.<next-line>
  </description-compact>

  Finally the solver gives information about the thus far found
  <with|font-family|ss|<strong|Assemblies>> (i.e. assemblies for which the
  pieces do not overlap in 3-<no-break>D space) and
  <with|font-family|ss|<strong|Solutions>> or disassemblies (i.e. assemblies
  that also can be constructed in real life using the particular pieces of
  the puzzle). Note that the <with|font-family|ss|Solutions> are only
  reported (and in fact tested) when the <with|font-family|ss|Solve
  Disassembly> option is enabled.

  <subsection|Manually Solving>

  Besides the automated search <with|font-family|ss|BurrTools> allows you to
  run the solver step-by-step. Note that this feature is still under
  construction and that it has a lot of shortcomings. For instance, it won't
  add the found solutions to the list or update the solver information. So it
  certainly needs a lot of improvements in a future release of the program.
  For the time being it is only useful to check the assembly process when
  something went wrong with the automated search.

  A manual search needs the initial preparation phase as well as an automatic
  search. This can be accomplished by clicking
  <with|font-family|ss|<strong|Prepare>>. The solver will halt after this
  initial phase and the subsequent steps of the assembler can be seen in the
  3-<no-break>D viewer by clicking the <with|font-family|ss|<strong|Step>>
  button.

  <section|Browsing Placements><label|BrowsingPlacements>

  The <strong|<with|font-family|ss|Browse Placements>> button opens a window
  (Figure<\float|float|tbh>
    <big-figure|<postscript|Pics/Window_Placements.png|*5/8|*5/8||||>|<label|FigurePlacementBrowser>Placement
    browser>
  </float> <reference|FigurePlacementBrowser>) that lets you examine the
  positions for each piece that will by tried by the assembler. The
  placements displayed in this window are the possible positions left in the
  current state of the assembler. So if the assembler has placed a piece
  S<em|a> and this prevents placing another piece S<em|b> at some positions,
  these positions of piece S<em|b> will <em|not> be visible in the list. If
  you want to see every placement tried you either have to initialise a
  manual search (click the <with|font-family|ss|Prepare> button), stop the
  assembler before is starts to do anything (click <with|font-family|ss|Stop>
  while in preparation or optimisation stage) or you have to wait until the
  assembler has finished its work.

  The <strong|<with|font-family|ss|Placement Browser>> window (Figure
  <reference|FigurePlacementBrowser>) has a very simple layout and consist
  mainly of a 3-<no-break>D viewer and some additional scrollbars. This
  3-<no-break>D viewport, that shows the outline of the result shape and
  therein the shape for which the possible positions are to be analysed,
  behaves similar to the one of the main window. Drag the piece to rotate it
  in space and use the scrollbar on the right to zoom in or out.

  Each piece in the problem (note that each instance of a multipiece is
  available) can be selected with the scrollbar on top of the window. The
  left scrollbar allows browsing all the different placements for the
  selected piece. Both these scrollbars can also be controlled with the
  cursor keys on the keyboard: <kbd|[Up]> and [<kbd|Down]> for the left
  scrollbar and <kbd|[Left]> and <kbd|[Right]> to select the piece. Be
  careful though, the first stroke on the keyboard that doesn't fit the
  current scrollbar will just select the other one and the following
  keystroke will start to move the slider.

  <section|Inspecting Results><label|InspectingResults>

  As soon as any result is found the solutions list becomes available on the
  <with|font-family|ss|<strong|Solutions>> panel and the 3-<no-break>D viewer
  shows the first solution in the list. Note that subsequent solutions are
  simply added to that list and that they only can get sorted by the total
  number of moves (in case disassembly was requested) after the search is
  completed. Already found solutions can at any time be inspected and this
  does not interfere with the ongoing solving process, but bear in mind that
  on completing the search resetting the scrollbar for browsing the solutions
  may be needed to show the solutions properly ordered.

  This panel has four components: a scrollbar
  (<with|font-family|ss|<strong|Solution>><with|font-family|ss|<strong|>>) to
  browse the different solutions, a second scrollbar
  (<with|font-family|ss|<strong|Move>>) to view the moves involved in the
  disassembly, an array of buttons with very short labels to organize the
  solution list and a list of all instances of the pieces in the puzzle
  problem, which allows you to alter the visibility of particular pieces in
  the solution(s).

  <subsection|Selecting Solutions and Animating Disassemblies>

  By moving the slider of the top scrollbar
  (<with|font-family|ss|<strong|Solution>>) any solution from the list can be
  selected as is indicated by its number in the text box left of the it.
  Above the scrollbar there is an indication of the total number of solutions
  in the list. When the scrollbar is active it can also be controlled by the
  <with|font-family|tt|[Left]> and <with|font-family|tt|[Right]> cursor keys.
  Keep in mind that the number of solutions in the list may be different from
  the real number of solutions. The correct number of solutions for the
  problem is shown in the solver progress section.

  The second scrollbar (<strong|<with|font-family|ss|Move>>) also has a text
  box on the left, this time reflecting the stage of disassembly (i.e. the
  number of moves executed in the disassembling process) of the currently
  selected solution. Moving the slider to the right will animate the
  disassembly, moving it to the left will reassemble the pieces in the
  3-<no-break>D viewer. Again, when activated the scrollbar can be controlled
  by the <with|font-family|tt|[Left]> and <with|font-family|tt|[Right]>
  cursor keys. Above this scrollbar the <em|total> number of moves required
  for the disassembly is shown followed by the level(s) of the selected
  solution. Note that this scrollbar is only visible for solutions which have
  disassembly instructions available.

  The position of the <with|font-family|ss|Move> scrollbar isn't affected by
  selecting any other solution and thus allows easily comparing the different
  solutions at a particular stage in the disassembly process.

  Below the <with|font-family|ss|Move> scrollbar are 2 fields that show you 2
  numbers associated with the currently selected solution. The first is the
  assembly number and the second is the solution number. Both numbers define
  when a solution was found. The first found assembly gets assembly number
  one. But that one might not be disassembable so it gets thrown away. The
  second found assembly gets assembly number two and if it is also
  disassembable it gets solution number 1. So you will see assembly 2 and
  solution 1 in these 2 fields for the given example.

  <subsection|Handling Solutions>

  The big button group below the Solution selector and animator lets you
  modify the solutions. They are only activated when no solver is running.

  With the buttons in the first row you can resort the found solutions by the
  same criteria as you can select for the solver. You can sort them in the
  order they were found (unsorted) or by level or by sum of moves to
  completely disassemble.

  The second row buttons allows the deletion of certain solutions from the
  list.

  <\description-compact>
    <with|font-family|ss|<item*|Del All>>removes all solutions

    <with|font-family|ss|<item*|Del Before>>removes all solution before the
    currently selected solution. The selected solution is the first one in
    the list that is not removed

    <with|font-family|ss|<item*|Del At>>removes the currently selected
    solution

    <with|font-family|ss|<item*|Del After>>removes all solutions behind the
    currently selected one. The selected on is the last one that is kept

    <with|font-family|ss|<item*|Del w/o DA>>remove all solutions that have no
    disassembly
  </description-compact>

  The last row of buttons allow the addition or removal of disassemblies to
  the list of puzzles.

  <\description-compact>
    <with|font-family|ss|<item*|D DA>>deletes the disassembly of the
    currently selected solution. The disassembly is replaced by a something
    containing only information about the disassembly, so you can still sort
    the solutions

    <with|font-family|ss|<item*|D A DA>>deletes all disassemblies

    <with|font-family|ss|<item*|A DA>>adds the disassembly to the currently
    selected solution

    <with|font-family|ss|<item*|A A DA>>add the disassembly to all solutions.
    Already existing disassemblies are thrown away

    <with|font-family|ss|<item*|A M DA>>add the disassembly to all solutions
    that do not have one. Solutions that already have a disassembly are left
    unchanged
  </description-compact>

  <subsection|Visibility of Pieces><label|VisibilityOfPieces>

  In the list at the bottom of the <with|font-family|ss|Solutions> panel all
  pieces used in the problem are represented by their identifier. Instances
  of multipieces have a counter added to their prefix which now takes the
  form '<with|font-family|ss|<strong|Sx.n>>' and their neutral colour may be
  slightly modified to tell them apart.

  By clicking an identifier the visibility state of that particular piece is
  altered in the 3-<no-break>D viewer. Each piece can have three states:
  <em|visible,> <em|outlined> or <em|invisible.> Clicking an identifier
  repeatedly just cycles through these states and also alters the way the
  identifiers are depicted in the list. These features are very useful in
  designs for which the pieces are packed in a box, since the box would hide
  most of the action that is going on inside (e.g. <name|Al Packino>,
  <with|mode|math|\<vartriangleright\>>Appendix
  <reference|AppendixExamples>). Also they are very useful for inspecting the
  interaction of a few pieces and allow comparison between different
  solutions as the visibility states remain invariant in selecting solutions.

  By default the pieces that become separated from the rest gradually fade
  out during the final move. Sometimes this is unwanted as it may hinder a
  clear view on what's going on. This can be avoided by unchecking
  <strong|<with|font-family|ss|Fade Out Pieces>> on the options window
  (activated through <with|font-family|ss|<strong|Config>> on the menu bar).

  <\with|par-mode|right>
    <chapter|Reporting with BurrTools>
  </with>

  <name|BurrTools> comes with some extra features to assist you in making
  puzzle solution sheets, either for your personal archives or to be issued
  with your exchange puzzles and commercially produced puzzles. Currently,
  these capabilities are very basic and need to be improved in a future
  update of the program. So, don't expect too much from them right now, but
  rather consider them to be merely a preview or a teaser to stick to
  <name|BurrTools>.

  <section|Adding Comments><label|AddingComments>

  The <with|font-family|ss|<strong|Edit Comment>> entry on the menu bar opens
  a new window that allows you to add textual information to the puzzle file.
  It can be used to append extra information to the puzzle such as the name
  of the designer, or a 'to do' list for your own designs.

  <section|Exporting Images><label|ExportingImages>

  The <with|font-family|ss|<strong|Export - Images>> entry on the menu opens
  a window that allows you to export a portion of the current puzzle in to (a
  list of) images (see Figure<\float|float|tbf>
    <big-figure|<postscript|Pics/Window_ImageExport.png|*4/8|*4/8||||>|<label|FigureImageExport>The
    image export window>
  </float> <reference|FigureImageExport>). The window has a 3D view on the
  right and input elements that control what is being created on the left. On
  the very bottom of these controls you can select what you want to create
  images of. Depending on what is present in the puzzle, the following things
  can be exported:

  <\description-compact>
    <item*|Shape>An image of a single shape is created. You can select which
    shape with the shape selector below.

    <item*|Problem>An image containing all shapes that are used for a problem
    is created. Again you will find the problem selector below that is used
    to select which problem you are going to create images of.

    <item*|Assembly>An image showing the positions of the pieces in an
    assembly is created. You can select the problem. Of that problem the
    first assembly is exported.

    <item*|Solution>An image containing all steps necessary to disassemble a
    problem is created. In this case you also select the problem with the
    selector below. The images will be created for the last solution of that
    problem.
  </description-compact>

  This is the first thing that you have to select. Naturally only the choices
  are available to which the puzzle has data. So if you have not run the
  solver on the current puzzle it is impossible to export solutions or
  assemblies.

  Above these selector you find the file output parameters. First the name
  and the path to where the images are supposed to be created. If you give no
  path the images are put into the working directory of the program. The file
  name is just a prefix, so if you keep 'test' as file name you get files of
  the form 'test000.png', 'test001.png', <with|mode|math|\<ldots\>>.

  Finally you can say how many images you intend to create. <name|BurrTools>
  will try to do so, but might use less. If you only have one assembly to
  export, only one page can be created.

  The <with|font-family|ss|Number of images> entry is ignored by the software
  for the time being, it will be used later on.

  Above these input elements you find the last section that defines how you
  want to output, what you output. You can define the quality and some
  additional parameters that influence how the images look, but not what is
  to be seen.

  In the top left corner you find the definition of the background of the
  image. You can choose between transparent or white. Transparent is useful
  if you want to have a background with patterns or want to further edit the
  images.

  Below you find the settings for the oversampling factor. The higher that is
  the smoother the images will look, but the more memory and calculation time
  is required.

  Below you can select if you want to use the constraint colours for the
  output or rather the neutral colour of the shapes.

  The checkbox <with|font-family|ss|Dim static pieces> makes <name|BurrTools>
  draw pieces that are not involved in the current move in a lighter colour,
  so that the actually moving pieces are easier to spot. This, of course,
  only works when exporting solutions.

  Finally there are the parameters for the image size that the program
  creates. You have 2 possibilities. Either define the pixel size directly,
  or define the size of the image in millimetres and the DPI printer
  resolution. If you want to create A4 or letter sized images for printing
  you can use the predefined sizes.

  To position the shapes in the output images you can use the 3D view at the
  right of the export window. All images exported will use the same settings
  for angle and zoom as in that 3D view. If the shapes reach above or below
  the 3D view they will be cut. Left and right is different. The width of the
  images to generate is not fixed. So the program will make them quite a bit
  wider to accommodate the horizontal spread of the pieces.

  If you have finished with all settings press <with|font-family|ss|Export
  Image(s)<strong|>>. You will see a flurry of images in the 3-<no-break>D
  view. The program draws the shapes there and grabs the content from the
  display. This may take a while. First the size of the images is determined
  then the images are drawn in the required high resolution for the output.
  The progress can be seen on the left besides the 2 buttons. You will see
  how many images are finished and how many there are overall.

  Hint: If you get unexpected results and broken images try to do nothing
  while the images are exported. On Linux it is forbidden to change the
  virtual desktop because then nothing is drawn.

  The export is far from what we want it to be, many important features are
  missing, so you can expect some progress in later versions of
  <name|BurrTools>.

  <section|Exporting to STL><label|ExportingSTL>

  STL, which stands for Standard Triangulation Language or Standard
  Tesselation Language is a file format used by stereolithography software.
  STL-Files describe the surface of 3-<no-break>dimensional objects.
  <name|BurrTools> can export single shapes into STL files so that 3D printer
  can quickly fabricate prototypes of them.

  The main menu entry <with|font-family|ss|<strong|Export - STL>> opens the
  window seen in Figure<\float|float|tb>
    <big-figure|<postscript|Pics/Window_StlExport.png|*4/8|*4/8||||>|<label|stlexportwindow>The
    STL-Export window>
  </float> <reference|stlexportwindow>. The window has shape selector, a
  3-<no-break>D view of the selected shape and some parameters that control
  the created shapes.

  <with|font-family|ss|<strong|Filename>> and
  <with|font-family|ss|<strong|Path>> control the name and position of the
  generated file. <with|font-family|ss|<strong|Cube Size>> controls the base
  length of the created cubes. <with|font-family|ss|<strong|Bevel>> controls
  the size of the bevel and <with|font-family|ss|<strong|Shrink>> allows to
  have a gap between different pieces, so that it is actually possible to
  assemble them. If the shapes were make to correct sizes they would touch
  and movement impossible.

  The STL-Export does right now only work for cubes. Triangles and spheres
  are not working. Also the shapes to export must not contain any variable
  voxels.

  <\with|par-mode|right>
    <chapter|Future Plans>
  </with>

  So, what are our future plans? There are a lot of things still missing (or
  in need of improvements) from the current program. A list of things that
  might be interesting to implement are the following:

  <\itemize-dot>
    <item>Add some special algorithms that are faster for certain kind of
    puzzles. The current algorithm is quite good for nearly all puzzles, but
    it's not <em|the> fastest.

    <item>Add more colour constraint possibilities, e.g. edge matching, ...

    <item>Add different more space grids, add parameters to some grids
    (lengths and angles).

    <item>Add rotation checks to the disassembler.

    <item>Add a shape generator: create all piece shapes that fulfil certain
    rules (shape, colours, union of two shapes, ...)

    <item>Libraries of shapes to import pieces from.

    <item>Add tools for puzzle design (see below).

    <item>Make it possible to divide problems so that they can be solved
    parallel on several computers and then the solutions are merged back
    together in one file.

    <item>Improve multi threading so that multi-core CPUs are better used.

    <item>Improve assembler to cope with ranges of piece numbers (e.g. 1-5 of
    piece x) and doesn't need to place all pieces. So that is is possible to
    solve piece sets and also to create puzzles by defining a set of pieces
    and let the program find out which of them results in a nice puzzle.

    <item>Better tool for colourization of a piece. E.g. checkering, but it
    needs to be more general than just checkering.

    <item>Create a debug window to make it possible to find out why there is
    no assembly or why an assembly can not be taken apart.

    <item>Speed improvements.

    <item>''Unificator'' a tool that makes it easy to check the results of
    adding color constraints. For example: suppose you have designed a puzzle
    with one interesting solutions and many uninteresting and you want now,
    by adding color constrains, make that one solution unique. It can be a
    labor intensive task to do that. The unificator would help here by
    quickly showing the resulty of adding color here or making a piece that
    color, ...<next-line>
  </itemize-dot>

  We would be very happy to get contributions from other people. After all
  there are quite a few people out there that have their own puzzle solving
  programs, maybe they have some nice additions. There is one important thing
  to keep in mind: the additions have to run on <name|Linux>. So you can not
  use any proprietary library that is not available for <name|Linux>.

  <subsection|Burr Design Tools>

  The following paragraphs are written as if the features were already
  implemented, but this is only done so that the text can be copied into the
  real book without having to rewrite a lot of it.

  There are 3 possible design methods implemented in BurrTools

  <\enumerate-numeric>
    <item>BurrGrowing after Dic Sonnevelds ideas

    <item>Constructing, the natural approach

    <item>Destructing, the inverse way, take the assembled puzzle and try to
    assign cubes to one of the pieces
  </enumerate-numeric>

  The following sections will describe these methods

  <subsubsection|Burr Constructing>

  The idea behind Constructing is to create new puzzles out of a set of
  pieces, try all possibilities and select the best found. To give the
  designer a great number of possibilities there are loooots of options here
  beginning with the design of the pieces ending with the method of how to
  solve the generated puzzle and how to save them.

  The basis for the Burr Construction is a normal puzzle file containing some
  shapes. These shapes are then taken by the constructor and made into many
  puzzles that are solved.

  So lets start with the piece generation. Each piece for the puzzle that
  needs to be generated may be assembled out of the following possibilities:
  a fixed piece, a list of pieces, a merger of 2 or more pieces, a piece
  containing variable cubes. The whole possibilities can be stacked on one
  another, so you can specify a list of 2 pieces where is piece is the merger
  of 3 pieces containing variable cubes... . All this can result in many
  possibilities, so be careful if you want a full analysis this side of
  eternity. Because of the complexity the program also might encounter the
  same puzzle several times. It will also be possible to let the program
  select puzzles out of the definition space by chance instead of doing a
  full analysis.

  So what do the possibilities mean.

  <\description-compact>
    <item*|fixed piece>a shape containing no variable cubes. This shape is
    directly used

    <item*|variable piece>a shape containing n (n\<gtr\>0) variable cubes.
    All shapes are used that have one of the <with|mode|math|2<rsup|n>>
    possible conditions for the variable cubes are used

    <item*|list of pieces>the pieces in the list are taken one after the
    other

    <item*|merger of n pieces>a new piece is constructed containing the union
    of both pieces, where the union is set, if one of more of the shapes to
    merge is set and the others are not set and variable is at least one is
    variable.
  </description-compact>

  At the end of the process it is possible to define the type of connection
  that must exists inside the shapes, shapes that do not fulfil this
  requirement are dropped

  All these possibilities may lead to a huge number of shapes, so be careful.

  Now it is possible to select the way the puzzle is solved. This includes
  disassembly (if or if not), also reduction and parameters for reduction can
  be set

  Finally it is possible to select the way the created puzzles are saved.

  <\itemize-dot>
    <item>All / only Solvable / only uniquely

    <item>keep best with least number of solutions

    <item>keep best with highest disassembly level

    <item>keep best with biggest disassembly tree (most branches on the way
    out)

    <item>keep best with highest number of not disassembable solutions
  </itemize-dot>

  \ Save puzzles with solution(s) or without to save space

  The puzzles are all saved into single directory, that must be selected

  It would be nice to be able to stop the search process and continue later
  on, the parameters for the constructor should be saved into the source
  puzzle file (including the current state)

  <subsubsection|Destruction>

  Destruction is in some way the inverse process of construction. Here you
  start with the finished assembly and you assign the outer voxels to certain
  pieces. Now the search process starts by assigning the not yet assigned
  cubes to pieces or to voids. All possibilities are tried and the best are
  kept.

  Additionally it is possible to pose certain requirements on the piece
  shapes. You can say in which way the pieces must be connected (by faces,
  edges, corners), if the pieces need to be machine makable.

  Also it is possible to do the whole process randomly instead of completely

  <subsubsection|Burr Growing>

  This method has been pioneered by Dic Sonneveld. It is suitable to create
  extremely high level burrs. The algorithm works by adding cubes to pieces
  to prevent certain moves and hope that the puzzle will still be
  disassembable in a different way.

  <part|Advanced User Guide>

  <\with|par-mode|left>
    <\with|par-mode|right>
      <chapter|The Internals>
    </with>
  </with>

  This chapter explains some of the internals. It is still quite incomplete,
  probably out of date and might even be wrong...

  <section|The Puzzle File Format>

  For those people that want to do things that the GUI is not supporting the
  exact file format of the files used by the GUI and the library may be of
  interest.

  The format is actually a gzip compressed XML-File. The program can read
  both, compressed and uncompressed files transparently so you don't need to
  zip them before loading into the program. The GUI always writes compressed
  files so if you want to change something in them you first need to
  decompress it.

  I wont describe all the elements of the XML-File, it's easier if you enter
  something similar to what you need in the GUI and look in which way the
  program saves these information.

  <subsection|Voxel Space>

  Because this is probably the most complicated part of the format here is a
  description of how the voxel spaces are saved. The size of the space is
  saved inside the attributes of the node and the contents of the node is
  saved in text of the node.

  The 3 voxel states are saved with 3 characters:

  <\itemize-dot>
    <item>'<verbatim|_>' for empty voxels

    <item>'<verbatim|#>' for filled voxels

    <item>'<verbatim|+>' for variable voxels.
  </itemize-dot>

  Colours can not be attached to empty voxels but to voxels with the other 2
  states. Currently colours are just a number (up to 2 digits) that are
  simply written as a decimal number and are appended to the voxel state. If
  the colour number is 0 (which is the neutral colour) nothing is appended.

  <section|The Library>

  The library is available for all people who want to do an analysis that
  would be too much work to do by hand with the GUI. A bit of C++ programming
  experience is necessary to handle the task.

  There are 4 important classes in the library. The class
  <verbatim|voxel_<degreesign><degreesign><degreesign>c> handles a 3
  dimensional array. Each position inside the array corresponds with one cube
  inside the piece. The class <verbatim|puzzle_c> is responsible for the
  whole puzzle containing a set of pieces and a solution. The classes
  <verbatim|assembler_x_c> and <verbatim|disassembler_x_c> (where
  <verbatim|x> is a number which may be available to select different
  algorithms that do the same task) are responsible to find assemblies and to
  disassemble the found assemblies. The important aspects of these classes
  will be explained in the next sections.

  <subsection|Class Voxel>

  This class contains functions to organise, modify, transform
  3-<no-break>dimensional arrays of cubes. Each entry inside the array
  contains 2 values:

  <\itemize>
    <item>The type of voxel (is it empty <verbatim|VX_EMPTY>, filled
    <verbatim|VX_FILLED> or a variable cube <verbatim|VX_VARIABLE>

    <item>The colour constraint colour. Here values between 0 and 64 are
    possible. 0 is the neutral colour.
  </itemize>

  The class provides a set of functions to rotate, translate, mirror, resize
  and minimise the shape. The <verbatim|transform> function allows to
  generate all possible rotations <emdash> also including mirroring, if
  wished. The function <verbatim|selfSymmetries> calculates which of these
  transformations result in the same shape. <verbatim|Connected> finds out
  the all the cubes in the shape are connected in one big piece (neither the
  assembler nor the disassembler requests that this is the case).

  If all this is not enough then there are functions that return the value of
  the different cubes inside the shape and also to set the value of the
  cubes. These functions exists in different versions. One requires the x, y
  and z coordinate of the cube requested. The other just takes one number.
  For this function all the cubes are in one long row. This function is
  efficient to use if all cubes are traversed and an action is done that is
  independent of the exact position of this cube inside the shape. Finally
  there is a set of get functions that also work with coordinates outside the
  box of the shape. These function always return <verbatim|VX_EMPTY> for
  cubes outside the bounding box.

  Then there is a bounding box that encloses all non empty voxels. This box
  is used by the selfSymmetry function. It only transforms the part inside of
  the box and then compares. There are 2 comparison functions: one compares
  the voxel space one by one the other one compares the space inside the
  bounding box, so the content may be shifted and still they are considered
  identical.

  <subsection|Class Puzzle>

  This class contains all the information of the puzzle including the shapes,
  the result shape and piece shapes and number, the colour constraints, the
  solutions, the grouping information and some statistics. This class
  contains all the information that gets saved in hard disc.

  The class contains a huge amount of functions that allow you to set and get
  the contained information.

  <subsection|Class assembler>

  As already explained this class tries to find assemblies for a puzzle. It
  uses the dancing link algorithm explained later.

  The caller is informed about found solution via a class that the caller has
  to provide. This class contains a function. This function is called for
  each found assembly with the found solution as parameter.

  The caller can then do whatever he pleases. He can just count the number of
  solutions by increasing a counter. He can save the found solutions. He can
  analyse, if the found solution is disassembable. If the caller is not
  interested in the solution he has to delete it.

  <subsection|Class Disassembler>

  The disassembler tries to find out if an assembly can be taken apart. And
  if it can be taken apart it will return a shortest disassembly sequence.
  The class contains some datastructures to make it possible to quickly check
  multiple assemblies of the <em|same> problem. So it is possible to chreate
  one instance of this class and disassemble a whole set of puzzles and then
  destroy it.

  <subsection|Class Assembly>

  This class contains an assembly of a puzzle. The assembly is always
  connected to a specific problem of a puzzle because it takes reference to
  the piece numbers defined in the problem and also to the shapes of the
  pieces defined within the puzzle.

  The assembly itself contains just a list of positions and transformations.
  What shape is behind that must be asked from the puzzle class

  Assemblies can be transformed. This changes to placement and transformation
  of all the included pieces so that the resulting piece arrangement is
  rotated.

  Assemblies can also be compared. This is required for the rotation avoiding
  technique describes below for the assembler.

  <subsection|Class Disassembly>

  This class contains all the information to completely (or with piece
  grouping not completely) disassemble the puzzle. It contains a tree. On
  each branch of the tree the puzzle separates into 2 parts. If one part can
  not be further assembled (e.g only one piece is in that part or the
  grouping makes is not necessary to disassemble that part) the pointer to
  the subtree is <verbatim|NULL>. Each node of the tree contains a list of
  piecepositions that are the steps to take the problem apart.

  <subsection|Example>

  A very simple example can be found within the source code of the project.
  Check the burrTxt sources. They just check a few command line options, load
  the puzzle and then solve it, no fuzz with user interface, multi threaded
  application, ...

  <section|The Algorithms>

  There are only two algorithms of interest inside this program. One is the
  assembly algorithm. This one is based on the ``Dancing Link'' algorithm
  from D.E.Knuth. I needed to update the algorithm in 2 ways:

  <\enumerate-numeric>
    <item>We require cubes that <em|may be filled> as well as cubes that
    <em|must be filled>. The original algorithm only provides the 2nd type of
    cubes.

    <item>We need to do something about multiple identical pieces. The
    original algorithm will find <with|mode|math|<big|prod><rsub|s\<in\>shapes>num(s)!>
    as many solutions as there really are.
  </enumerate-numeric>

  The 2nd interesting algorithm is the disassembler. This is mainly a breadth
  first tree search over all possible placements of the pieces.

  <subsection|Assembly>

  As already said this algorithm is based on the Dancing link algorithm. This
  algorithm is mainly a very efficient and elegant backtracking method that
  stops much more early than many other algorithms. It stops when is finds
  that a piece can not be placed any more. It stops when it finds that a cube
  of the solution shape can not be filled any more. These recursion stops
  don't need to be implemented separately, but they are part of the
  algorithm. But bevore we go on describing the details, there is one mayor
  problem that needs to be solved: avoid finding solutions multiple times.

  <subsubsection|How to Avoid Finding Multiple Assemblies>

  Now this is a complicated problem. There is the naïve approach which would
  be to save all found assemblies and check new found assemblies against this
  list. This has major problems. You need to save all assemblies and there
  can be many. You need to check against all those save assemblies and that
  can get slow. If you want to make a break and later on continue you need to
  save all those solutions on harddisc and load them again. An of course the
  worst problem is that you waste a lot of time. If it just would be possible
  to not find those solutions in the first place.

  To solve this problem let us first analyze what kind of double solutions
  exist

  <\description-compact>
    <item*|Identical assemblies>These are solutions that do look completely
    identical (they are not even rotated). There are 2 possible reasons for
    this to happen:

    <\enumerate-numeric>
      <item>Two or more identical pieces that are exchanged

      <item>One piece has symmetries and the (invisible) difference between
      the 2 found solutions is that this piece is rotated

      <item>A bug in the code that makes the program find <em|really>
      identical assemblies. Lets assume that this is a rare event.
    </enumerate-numeric>

    <item*|Rotated assemblies>These are solutions that are identical but need
    to be rotated first to find that out.
  </description-compact>

  The first kind of assemblies can be avoided relatively easily by removing
  rotations from pieces that result in the same piece. And by being careful
  with identical pieces and avoid finding the permutations of these pieces.
  With these precausions it can be assured that <em|no> identical looking
  assemblies are found.

  The second kind is very hard. The recursive part of the program will find
  them. It is possible to avoid finding a few of the rotations and in some
  puzzles is even possible to avoid finding any of them but there are puzzles
  where the program <em|will> find some or even all possible rotation, so a
  solution needs to be found that can detect rotations when they are found.

  But first let's see how we can avoid as many of the possible rotations as
  possible. This is done by selecting one piece and dropping a few of the
  rotations that are possible with this piece. As this piece can not only be
  inside the solution in certain positions all solutions that would require
  that piece to be rotated will not be found. If we can be sure that all
  solutions that are dropped also exist as a rotation inside the solutions
  that we find we are lucky. But which piece to select? And what rotations to
  drop?

  To find out which piece it helps to think of the perfect piece. Lets assume
  our target is a cube and it has only one solution. A cube has 24 symmetries
  so we would normally find 24 solutions (maybe even more, due to mirror
  solutions, but let's forget about this for a while). With each rotation
  that we drop from our selected piece one of the possible rotations for the
  solutions wont be found until we have only one possible rotation left for
  the selected piece and so we find only that solution where this piece is in
  that left over rotation. All other rotations would require the piece to be
  in another rotation, which is not in our list to try. But this only works,
  if the piece really has 24 differen rotations from which we can drop 23. If
  the piece is symmetric in one way or another it will not have that many
  different rotations as a few of them will result in the same piece and thus
  can not be considered. So the best choice is always a piece with no
  symmetries. What to do if there is none such piece? Select one has has the
  least overlap with the symmetries of the result shape.

  Before we make clear what that means we have to see which rotations need to
  be dropped. We need to drop those rotations that might result in a rotated
  solution. A rotated solution is one that has the same exterior appearance.
  So the possible rotations result from the shape of the result. If all these
  rotations do exist in the selected piece we can supress the rotations from
  the solutions by dropping them.\ 

  And now back to the clean and general solution. Here Bill Cutler came to my
  help. He told me what he did and that is something very ingenious.

  The first thing to do it to be able to compare two assemblies that are the
  same but one is a rotation of the other and be able to say assembly
  <with|mode|math|a<rsub|1>> is smaller or larger or equal to assembly
  <with|mode|math|a<rsub|2>>. This comparison can be implemented by comparing
  piece positions and transformations. It can be completely arbitrary. It
  just must be assured that the rotation suppression with the pivot piece
  does not remove the one transformation that is the smallest when compared
  with the comparison.

  Now the following is done for each assembly found. At first all rotions of
  this assembly are generated that result in the same shape for the assembled
  shape. These assemblies are compared with the found one. If there is one
  that is smaller than the found one drop the found assembly and go on
  searching. If the found assembly is the smallest one do whatever needs to
  be done with it.

  There are 2 left open the question. What to do if the found assembly is the
  smallest but there is another assembly just as small? And how can be
  assured that the rotation selected by the comparions function is not
  removed by the rotation avoiding method.

  First to the first question. When does this happen? This happens then when
  solution itself (not only the shape of the result but the also the
  construction) has some symmetry. That means that there are 2 indetical
  looking solutions that differ in exchanged pieces ore a rotation of a piece
  that does result in an identical looking piece. This kind of identical
  solution has already been successfully avoided, so there is no need to take
  special precautions, that case is ignored. If the found assembly is one of
  the smallest it is taken, if there is one ore more smaller assembly, it is
  dropped.

  Now on to the 2nd problem. Here we need to make sure that the rotation
  avoiding method knowns about the comparison function and makes sure that
  the smallest of the assemblies is kept. Here is one possibility:\ 

  If the comparison function looks like this:

  <\code>
    for (p = 0 up to number of pieces the assembly) {

    \ \ if (rotation of piece p in assembly 1 \<less\> rotation of piece p in
    assembly 2)

    \ \ \ \ return assembly 1 is smaller

    \ \ elseif (rotation of piece p in assembly 1 \<gtr\> rotation of piece p
    in assembly 2)

    \ \ \ \ return assembly 2 is smaller

    \ \ elseif (pos x of piece p in assembly 1 \<less\> pos x of piece p in
    assembly2)

    \ \ \ \ return assembly 1 is smaller

    \ \ and so on

    }
  </code>

  For this function an assembly with a piece 1 with a smaller rotation number
  is always smaller that one with a bigger rotation number.

  So if we chose a rotation avoiding technique that always selects piece one
  as pivod piece and always removed the bigger rotation number, we should be
  on the save side.

  <subsubsection|The Dancing Link Algorithm>

  I will describe the only the basics for the original dancing link
  algorithm. For further information read the document available on Mr.
  Knuths web page (<verbatim|http://www-cs-faculty.stanford.edu/~knuth/musings.html>).

  The algorithm represents the puzzle as a matrix. In this matrix the first
  columns represent the pieces and the last columns represent one voxel of
  the result shape each.

  Each line of the matrix corresponds to one possible placement of one piece
  inside the result. The column of the piece and the columns the represent
  the places inside the solution that the piece occupies with the placement
  are 1 inside the matrix. All the other cells are 0.

  The search itself runs on this matrix. It searches for a set so that all
  the lines in this set taken together contain exactly one 1 in each column.
  This means that each piece must be used and each cube in the result must be
  filled.

  The algorithm does 2 operations on the matrix:

  <\enumerate-numeric>
    <item>Cover column n and uncover column n. This means that the column is
    removed from the matrix and no longer taken into account for the search.
    When a column is removed all the rows that contain a 1 in this column
    will also be removed.

    <item>Cover and uncover row n. This means that we select this row for the
    set of rows that we search. The row covering also removes and re-includes
    all the columns that contain a 1 in this row. On these columns operation
    1 is performed.
  </enumerate-numeric>

  The 2nd operation can be interpreted as. Taking one piece and putting it
  inside the result at one possible place. This results in the fact that a
  few cubes of the result don't need to be observed any longer and all
  placements of all other pieces that collide with this placement don't need
  to be checked further.

  The cover and uncover operations are the inversion of one another. If we
  first cover something and then uncover it again the matrix is in exactly
  the same state.

  The algorithm is now recursively trying all possibilities. It selects one
  column and then tries covers all rows that contain a 1 in this columns and
  then calls itself.

  It finished when there are either no more columns left. Then we have found
  a solution or there is one column with no rows. Then we have found a dead
  end and backtrack.

  This algorithm is per se not dependent on square cubes it is not dependent
  on any shape. You only need to transfer your puzzle into the matrix. Even
  William Waites<\footnote>
    see <verbatim|www.puzzlemist.com>
  </footnote> puzzles should be possible. But as the square and cubes are
  most common I have for now only implemented this transformation.

  Now to the changes that I have done to this basic algorithm. There is first
  the matter with the 2 types of cubes. This is easily solved by removing the
  columns of the cubes that <em|may be filled> from the list of columns that
  need to be covered. They are still in the matrix, they just don't <em|need>
  to be covered to find a solution.

  The 2nd problem was much harder. How handle multiple identical pieces? The
  solution that I finally implemented is to enforce an order. All pieces get
  a number and all the placements get a number. If we now have 2 identical
  pieces <with|mode|math|a> and <with|mode|math|b> with
  <with|mode|math|a\<less\>b> I force that the placement of
  <with|mode|math|a>, <with|mode|math|p(a)> is also smaller than the
  placement of <with|mode|math|b> so <with|mode|math|p(a)\<less\>p(b)>. This
  is done by always placing all identical pieces in one go. The moment the
  algorithm decides to place one of the pieces that occur multiple times it
  will also place all the others and always check that these have larger
  placement numbers.

  <subsection|Disassembly>

  The disassembly algorithm is a breadth first tree search. In this tree
  every node represents one possible relative position of the pieces. To find
  out what can be moved in this node the algorithm Bill Cutler used for his 6
  Piece Burr analysis is used. His algorithm anaylzes for 2 pieces how far
  the first piece piece can be moved in the positive direction of each of the
  3 axis if the other piece is fixed. This results in 3 matrixes each square
  with as many rows and columns as there are pieces. The values for negative
  directions can be taken from transposed matrixes. To make these matrixes
  useful they need to contain not pairwise information but for the whole
  state. To get this information the following property is used:

  <\quote-env>
    If piece A can be moved x units when B is fixed and piece C can be moved
    y units whan piece C is fixed then piece C can not be moved more than x+y
    units when B is fixed.
  </quote-env>

  With this property the 3 matrixes are treated again and again until all
  values have reached a stable value. The resulting values tell you exactly
  how far each piece can be moved when some other pieces are fixed.

  Now all possible new states are generated with the aid of these calculated
  values.

  This worked nice but it has been quite slow. Slower than
  <name|PuzzleSolver3D> at least. So I started to optimize. The slowest part
  has been te pairwise analysis of all piece pairs. Initially I implemented
  more and more complicated schemes that were supposed to speed up thing. But
  the code got more and more complicated and due to the usage of preprocessor
  macors utterly undebuggable. And it was still slower than
  <name|PuzzleSolver3D>.

  Finally I came up with a new scheme that solved the speed issues: a cache.
  This cache contains the values calculated for the movement possibilities of
  2 pieces. Once they are calculated they are put into the cache and used
  from there later on. The cache contains the 3 calculated values. The key is
  calculated from the piece numbers, their relative positions and their
  transformations. The incorporation of the transformations made it possible
  to used the cache over the whole process of a puzzle analysis and not to
  restart it for each assembly. This has a mayor impact: the number of cache
  hits is for some puzzles way over 90%.

  This cache also has another nice property. It is possible to remove
  information for a certain piece from it. This comes in handy when
  burrgrowing is used, as the information for changed pieces can be removed
  from the cache but the rest is still intact and useful information.

  <section|Adding to the Library>

  There is currently one useful thing besides the normal improvements that
  might be added to the library: Other puzzle types. The assembly algorithms
  is so abstract that it can cope with many different types of assembly
  puzzles, as long as they have some kind of pattern. Currently the assembler
  only supports puzzles made out of cubes but there is nothing that prevents
  solving puzzle where the base unit is a hexagon. Of course the disassembler
  can not do work with this kind of puzzles.

  To add other geometries the assembler is split into 2 parts. The dancing
  link algorithm and the algorithm that prepares the matrix for the dancing
  link algorithm. This preparation part is called the front end.

  <part|Appendices>

  \;

  <\with|par-mode|right>
    <appendix|Examples><label|AppendixExamples>
  </with>

  <name|BurrTools> comes with some examples that illustrate the capabilities
  and functions of the program. We'd like to thank the designers for allowing
  us to include their designs in the <name|BurrTools> package.

  <section|Al Packino>

  <\description-compact>
    <item*|Design>Ronald Kint-Bruynseels, 2003, Belgium.

    <item*|File><with|font-family|tt|AlPackino.xmpuzzle>

    <item*|Remarks>This puzzle shows how to properly make packing puzzles.
    You always should include the box as a piece so that the program can also
    check if the pieces can be moved into or out of the box. You can also see
    how to handle multipieces. When looking at the solution it is useful to
    display the box as a wire frame. This can be done by clicking at the blue
    rectangle at the lower end of the tools. The rectangle with the S1 in it.
  </description-compact>

  <section|Ball Room>

  <\description-compact>
    <item*|Design>Stewart Coffin, #197-A, USA

    <item*|File><verbatim|BallRoom.xmpuzzle>

    <item*|Remarks>This puzzle shows off the sphere gridspace. It also
    demonstrates that is is possible and useful to include more than one
    problem within one file.
  </description-compact>

  <section|Bermuda>

  <\description-compact>
    <item*|Design>Bill Cutler, 1992, USA

    <item*|File><verbatim|Bermuda.xmpuzzle>

    <item*|Remarks>This puzzle demonstrates the triangle space grid. You can
    see that you can stack many layers on top of each other.
  </description-compact>

  <section|MINE's CUBE in CAGE>

  <\description-compact>
    <item*|Design>Mineyuki Uyematsu, 2002, Japan.

    <item*|File><with|font-family|tt|CubeInCage.xmpuzzle>

    <item*|Remarks>This file contains MINE's CUBE in CAGE 333, cube g. This
    puzzle demonstrates how to use the grouping capabilities. The puzzle
    contains 3 interlocked pieces that construct a cage. These pieces move
    but can not be taken apart. It needs to be told to the program that this
    is intentional. So here you have an example of how to do that.
  </description-compact>

  <section|Dracula's Dental Disaster>

  <\description-compact>
    <item*|Design>Ronald Kint-Bruynseels, 2003, Belgium.

    <item*|File><with|font-family|tt|DraculasDentalDisaster.xmpuzzle>

    <item*|Remarks>This puzzle demonstrates the use of colour contraints.
    Halve of the result must be red and the other halve black. You can see
    the colours if you enable the checkbox in the status line at the bottom
    right.
  </description-compact>

  <section|Level 98 Burr 'The Pelican'>

  <\description-compact>
    <item*|Design>Dic Sonneveld, 2000, The Netherlands.

    <item*|File><with|font-family|tt|PelikanBurr.xmpuzzle>

    <item*|Remarks>This is a <em|very> high level burr. It takes 98 moves to
    get the first piece out of the box. This is just a demonstration of what
    is possible.
  </description-compact>
</body>

<\initial>
  <\collection>
    <associate|font|roman>
    <associate|language|british>
    <associate|page-breaking|sloppy>
    <associate|page-medium|paper>
    <associate|page-orientation|portrait>
    <associate|page-screen-height|768000tmpt>
    <associate|page-screen-width|998400tmpt>
    <associate|par-columns|1>
    <associate|par-hyphen|professional>
    <associate|par-par-sep|0>
    <associate|sfactor|7>
  </collection>
</initial>
