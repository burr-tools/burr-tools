<TeXmacs|WinTeXmacs-1.0.5>

<style|book>

<\body>
  <doc-data|<doc-title|BurrTools>|<doc-author-data|<author-name|Andreas
  Röver>|<author-email|roever@users.sf.net>>>

  \;

  <\table-of-contents|toc>
    <vspace*|1fn><with|font-series|bold|math-font-series|bold|Table of
    contents> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-1><vspace|0.5fn>

    <vspace*|1fn><with|font-series|bold|math-font-series|bold|1<space|2spc>The
    Program> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-2><vspace|0.5fn>

    1.1<space|2spc>Introduction <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-3>

    1.2<space|2spc><with|font-shape|small-caps|PuzzleSolver3D> Users
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-4>

    1.3<space|2spc>New Users <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-5>

    <with|par-left|1.5fn|1.3.1<space|2spc>Main Window
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-6>>

    <with|par-left|1.5fn|1.3.2<space|2spc>Piece Tab
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-8>>

    <with|par-left|1.5fn|1.3.3<space|2spc>Problem Tab
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-11>>

    <with|par-left|1.5fn|1.3.4<space|2spc>Solve Tab
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-12>>

    1.4<space|2spc>Future Plans <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-13>

    <vspace*|1fn><with|font-series|bold|math-font-series|bold|2<space|2spc>The
    Internals> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-14><vspace|0.5fn>

    2.1<space|2spc>The puzzle file format
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-15>

    2.2<space|2spc>The Library <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-16>

    <with|par-left|1.5fn|2.2.1<space|2spc>Class voxel
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-17>>

    <with|par-left|1.5fn|2.2.2<space|2spc>Class puzzle
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-18>>

    <with|par-left|1.5fn|2.2.3<space|2spc>Class assembler
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-19>>

    <with|par-left|1.5fn|2.2.4<space|2spc>Class disassember
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-20>>

    <with|par-left|1.5fn|2.2.5<space|2spc>Example
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-21>>

    2.3<space|2spc>The Algorithms <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-22>

    <with|par-left|1.5fn|2.3.1<space|2spc>Assembly
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-23>>

    <with|par-left|1.5fn|2.3.2<space|2spc>Disassembly
    <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-24>>

    2.4<space|2spc>Adding to the Library <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
    <pageref|auto-25>
  </table-of-contents>

  <chapter|The Program>

  <section|Introduction>

  What are <name|BurrTools>? On the one hand they are a program that assemble
  and disassemble burr-type puzzles. That program contains a graphical
  interface that allows creation and change of puzzle definitions and also
  the solving of the puzzle and the display and animation of the found
  solutions. On the other hand they are a C++ library that may help with the
  search and design of new puzzles.

  The program is quite similar to the <name|PuzzleSolver3D>. I have started
  implementing a program that was nearly identical but over the time it
  evolved into something a bit more different.

  The first section of the document is meant for the people already confident
  with <name|PuzzleSolver3D>. The second section is for the people completely
  new to this kind of software. And finally some of the internals of the
  program are explained. This section is probably only interesting for people
  wanting to use the library for their own puzze design explorations.

  But first a little bit to the history of this program. Why another program
  you might ask. There are a few reasons:

  <\enumerate-numeric>
    <item>The available programs (<name|PuzzleSolver3D> and
    <name|BCPBox/Genda>) are not for <name|Linux>, which is my operating
    system of choice.

    <item>The available programs are programs and wont allow me to do more
    interesting things like burr growing in an automated way.

    <item>The programs are quite expensive.

    <item><name|PuzzleSolver3D> seems to be abandoned. There hasn't been any
    update for quite a while.

    <item><name|PuzzleSolver3D> has some ugly limits to the shape sizes and
    the number of possible placements.
  </enumerate-numeric>

  Anyway, I was not satisfied with the available software. Then the C't, a
  German computer magazine, started a competition to write a program that
  counts the number of solutions to a merchandising puzzle of them as fast as
  possible. My program wasn't the fastest but it was the starting point for
  the <name|BurrTools>.

  As there are many people out there that are a lot more creative than I am
  and that could use a program like this to design nice puzzles I decided to
  make it public and free<\footnote>
    Free as in free speech and as in free beer (see
    <verbatim|http://www.gnu.org>)
  </footnote>.

  So I added a GUI that can work on many operating systems, including Linux
  and <name|Windows>. This has the disadvantage that the GUI looks a bit
  different from what the normal <name|Windows>-user is used to, so stay calm
  if things look a bit unusual, they behave in fact quite similar to what a
  normal <name|Windows>-program behaves.

  All this work has taken over two years to reach the current state, I hope
  it was worth it and you have a lot of fun with the program.

  Finally: I am sorry for all the typos, gramatical errors and poor
  expression that this document contains. This has never been my strength not
  even in my mother-tongue.

  <section|<name|PuzzleSolver3D> Users>

  People that know <name|PuzzleSover3D> written by André van Kamen should be
  able to quickly get used to the graphical user interface. For these people
  is this chapter. It just describes the important differences between
  <name|PuzzleSolver3D> and <name|BurrTools>. There are a few important
  differences:

  <\enumerate-numeric>
    <item><name|BurrTools> doesn't handle holes automatically as
    <name|PuzzleSolver3D> does. This may at first sound like a disadvantage
    but in fact it isn't. <name|PuzzleSolver3D> normally treats all cubes of
    the target shape as cubes that might be filled but don't need to be.
    Except if you select ``Fill outer limits`` in the solve tab. Cubes that
    must be filled speed up the search process. The more there are, the
    faster the assembly will be as there are fewer possibilities.
    <name|BurrTools> requires from you to exactly specify which cubes in the
    result shape must be filled and which ones may be empty. This is done by
    either clicking with the left mouse key (must be filled cubes) or the
    right mouse button (may be filled cubes). There is a tool that helps you
    with the variable cubes. The button ``make inside variable'' should
    nearly always do what you expect. Just define your result shape with all
    cubes fixed and then press ``make inside variable''. The result should
    normally be what you intend.

    <item><name|BurrTools> doesn't automatically find multiple identical
    pieces. You need to specify if there is one piece there more than once.
    If you just copy it the way you do in <name|PuzzleSolver3D> the program
    will find way too many solutions. For example for <name|Loveley18> it
    will find nearly 40'000'000 times as many solutions as there are in
    reality, so be careful.

    <item>Currently <name|BurrTools> can not sort out multiple identical
    solutions. It tries to avoid this problem by not finding these solutions
    in the first place but sometimes this is unavoidable and so you may be
    multiple identical solutions.

    <item><name|BurrTools> allows you to define multiple problems in one
    file. So you can, for example, save all the SomaCube problems within one
    file

    <item><name|BurrTools> allows to put constraints on the placement of
    pieces. This is done by having an additional information attatched to
    each unit cube. I call this information color. You can then exactly
    define which colors in the pieces may go into which colored cubes inside
    the result. If just one of the conditions is not fullfilled the placement
    is not possible. To make things a bit easier there is a ``neutal'' called
    color that may go everywhere or accept every color.

    <item><name|BurrTools> allows you to solve puzzles that are not
    completely disassembable. For this feature you have to assign the pieces
    to groups. All pieces that are within the same group can stay together.
  </enumerate-numeric>

  Then there are some other not so mayor things that differ:

  <\enumerate-numeric>
    <item>There are no limits to the number and size of pieces, like in
    <name|PuzzleSolver3D>. You can make the pieces as big as you want and are
    not limited to a size of 24

    <item>There is no limit to the number of placements to the pieces. It
    wont happen that the program complains about too many placements. As long
    as your computer has enough memory the program will merrily continue
    working, even if it would take longer than the universe exists to
    complete the search.

    <item>You can not play with the puzzle. That is a useless feature (in my
    eyes)

    <item>You can not check the placements of the pieces

    <item>You don't need to clear solutions when you want to edit the puzzle
    again. The solutions get automatically removed as soon as you start
    another search or edit the puzzle.

    <item>There is no automatic zoom or animated rotation of the 3D view of
    the pieces or solution. Also the animation of the assembly process can
    not be animated (yet?)
  </enumerate-numeric>

  I hope the description of these difference halps you to find out how to use
  the program. If not you need to read the next chapter anyway.

  <section|New Users>

  This chapter is for users that are new to a program like this. It will
  describe all aspects of the program in a level of detail that I hope is
  enough.

  <subsection|Terms>

  Bevore we start with the program lets synchronize our use of vocabulary.

  <\description>
    <item*|Shape>This is a definition of a 3-dimensional object. The shape
    contains information about the form of the object and some additional
    properties that his object may have. This includes information about the
    placing of pieces

    <item*|Result>This is the shape that the pieces of the puzzle are
    supposed to assume once the puzzle is assembled.\ 

    <item*|Piece>A piece is one part of the puzzle. It

    <item*|Multi piece>Some pieces have the same shape.\ 
  </description>

  <subsection|Main Window>

  <\float|float|tbh>
    <big-figure|<postscript|mainwin.png|/2|/2||||>|The Main
    Window<label|MainWindowImage>>
  </float>The main window (see Figure <reference|MainWindowImage>) contains a
  tool bar at the left side and the rest of the window is used for a 3
  dimensional view. By dragging with the mouse you can rotate the object
  visible inside the 3D view and by moving the slider at the very right edge
  you can zoom the object. The edge between the toolbar and the 3D view can
  be moved by dragging it with the mouse. Move the mouse to this edge. Once
  you see a left right array you can start to drag.

  The menu is relatively simple, just the usual ''File'' menu with ''New
  Puzzle'', \R''Load Puzzle'', \R''Save'' and \R''Save as'' entries. The
  ''Import'' command in the file menu allows you to import
  <name|PuzzleSolver3D> files. So you don't need to enter your designs again.

  Then the menu also contains a ``Toggle 3D'' called entry. This entry only
  works when the shape tab is activated. Then it exchanges the 3D view and
  the grid editor so that you have enough space to edit even large pieces.
  You can also do that by pressing F2.

  Finally the ``Config'' entry: This opens up a (currently very) small dialog
  that allows you to set a few options for the program.

  <\description-compact>
    <item*|Use lights in 3D view>toggles the use of a spot light in the 3D
    view. If you switch that off all faces will get illuminated by the same
    very high amount. If it is turned on there is a light in the upper right
    corner that illuminated the puzzle but this results in a relytively darc
    lower left corner which is sometimes undesired.

    <item*|Use Tooltips>This option allows you to switch off the display of
    the tooltips. After a while they normally get quite annoying.
  </description-compact>

  The options are saved in a file that is either in your home directoy (Unix)
  or in your profile (Windows).

  Finally there is the status line. It contains a checkbox. With this
  checkbox you can toggle between 2 different ways to view the pieces. One
  way shows all cubes of the pieces with their piece color. This color is
  determined by the number that the shape have. The very first shape will
  always be blue. The second way of showing the pieces uses their color
  constraint color instead of the piece color. All cubes that have a color
  attatched will have this color on the display. The cubes that have the
  neutral color attatched will be shown in their piece color. Color
  constraints are explained below.

  The toolbar on the left contain 3 main tabs. Each one of these tabs is
  explained in the following sections.

  <subsection|Piece Tab>

  <\float|float|tbh>
    <big-figure|<postscript|tabs.png|/2|/2||||>|The tools<label|ToolsImage>>
  </float>This tab (See Figure <reference|ToolsImage> left column) defines
  the colors and the shapes. <name|BurrTools> differentiates between shapes
  and pieces. There can be several pieces with the same shape. You as the
  user are responsible for not defining the same shape multiple times. If you
  do you will get multiple identical solutions.

  So let's start with the tab. At the top the color definition is placed
  ``Add'' and ``Rem'' add a new color or remove the selected color. ''Chn''
  can be used to change an existing color. Below these buttons is the list
  with the currently defined colors. There is always the ``Neutral'' color.
  This color can not be removed.

  The picture shows a red line below the color selector. This line shows
  lines in the tool bar that can be moved. You just have to place the mouse
  over this region. It will change to an up-down-arrow and you can drag this
  line up and down. With this feature you can adapt the tool bar so that is
  pleases you. If you, for example, don't neet color constraint you can make
  the color selector very small or even completely invisible. The picture
  shows all lines where this is possible with red lines. These lines are not
  visible in the real program.

  Below the color definitions is the shape list followd by the shape editior.
  New adds another shape. \RDelete removes the currently selected shape.
  \RCopy adds a new shape that is identical to the currently selected one.

  Below these 3 buttons is the list with the shapes. You can activate and
  edit a shape by clicking on it.

  <big-figure|<postscript|tools.png|/2|/2||||>|Shape edit
  tools<label|shapeEditTools>>

  Below the piece selector is another tab with some tools (see figure
  <reference|shapeEditTools>). These tools allow you to change the size of
  the space that is available for the piece definition. <name|BurrTools> uses
  the colours red green and blue to show the 3 space-dimensions. All tools
  that act on one of the axes are coloured accordingly. The 3D view contains
  three coloured lines that also show the axes. The first tab on the tool tab
  (labeled ``Size``) allows you to resize the current shape. The next tab
  (``Transform``) contains buttons that allow you to rotate (R) and shift (S)
  and flip (F) the shape definition inside its space. The button ``S+X`` for
  example shifts the contents up alonw the x-axis. The Button labelled
  ``R-Y`` rotates the shape along the y-axis against the clock. On the last
  tab (``Tools``) there are 2 buttons. Minimise makes the definition space as
  small as possible removing all empty space around the acutal shape
  definition. This is useful to make the files on disk a little bit smaller
  or to have a more centred view of the shape inside the 3D view. The button
  ``Make inside variable'' sets all the unit cubes that are completely
  surrounded by cubes to variable cubes. If you apply this to your result
  shape the behaviour will be as if you pressed ``fill outer cubes'' in
  <name|PuzzleSolver3D>. The 3rd button is used when editing, it is explained
  below

  The last item at the very bottom of this tab is the editor to change the
  shape. It contains a slider at the left that selects the z-plane. You edit
  by clicking and draggin within these squares. The left mouse button adds
  normal cubes. The right mouse button adds variable cubes. These variable
  cubes are only alowed in shapes that are used for the result. Clicking on
  an already filled cube either removes it or replaces it by the other cube
  type. Each cube changed will get the currently selected color (above in the
  color section) attatched to it. If this color it not the neutral color it
  will be visible as a small square in the upper left corner of the squares
  in the editor. The neutral color will not be visible anywhere. When you
  drag the mouse you can fill whole rectangles in one go.

  It is also possible to edit the whole stack instead of just the current
  layer. For this you have to activate the 3rd button in the shape edit tools
  tab. This is a toggle button. Press it and a yellow lamp will be switched
  on press it again and it is switched off. Once you switched the lamp on you
  always edit all z-layers instead of just the active one.

  <subsection|Problem Tab>

  In this tab you assemble the problem(s) that you want to create with the
  shapes that you have defined. One problem consists of a result shape, some
  pieces that are supposed to assemble this shape and a list of color
  assignments.

  At the top is the list with your currently defined problems and some
  buttons to create new problems, delete them or copy an existing one.

  Below is the list with the problems. You can select a problem by clicking
  at it in your list. This problem is then edited.

  Below the problem list is the color assignment editor. You can see a list
  with the colors you have defined in the first tab, followed by some buttons
  followed by the assignment list. Now this allows for every complex
  definitions, even though most of the time very simple 1 to 1 assignments
  will be used.

  In the color assignment list you see the piece colors on the left and the
  result colors on the right connected by some arrows. The list is either
  sorted by the piece colors showing you then that the piece color x can go
  into the cubes of the result that have the color attached that is at the
  other end of the arrow, or the list is sorted by the result color showing
  you that piece colors x, y, ..., z can go into result color a. The 2
  ''Srt'' buttons switch between these views.

  The 2 buttons in the middle with the arrows add or remove a color from the
  currently selected entry in the color assignment list. The color is always
  added on the side where the multiple colors are.

  Finally at the bottom is the shape assignment part. Here you select which
  shape is supposed to be assembled out of what other shapes. 2 Lists contain
  the shapes defined in the shapes tab and a list of the pieces involved in
  this problem. Above the shape list is the result.

  Between the shape and the piece list are 3 buttons labled ''+1'', ''-1''
  and ''Grp''. The first 2 buttons either add another one of the selected
  shape to the list of pieces or remove one of it. In the lower list you can
  see how many of each piece are used for the problem. The colored boxes show
  first the number of the shape and behind in braces the number of times the
  piece is used.

  The 3rd button allows you to define groups of pieces. With piece groups it
  is possible to leave pieces together when diassembling the puzzle. An
  example of a puzzle where this is required is the ``Cube in Cage'' called
  puzzle. Here you have a cage out of 3 pieces and within the cage you have
  to assemble a 3x3x3 cube. The cage is built in such a way that the 3 pieces
  can move, but they don't go apart. This is a problem because normally the
  disassembler tries to disassemble the puzzle into all its pieces but
  because this is not possible here it normally refuses the find solutions.
  The exact working is explained below.

  While editing problems you can see all the involved pieces in the 3D view.
  In the upper left corner you can see the result. It is drawn in double
  size. The upper right corner shows the currently selected piece and below
  these 2 pieces are all the pieces visible that are included in the problem.

  <subsubsection|Grouping>

  As already said: groups allow you to say to the disassembler that it is OK
  when it can not separate one piece from another. If the disassembler finds
  2 or more pieces can not be taken apart it checks if all of the involved
  pieces are in the same group. If that is the case it rests assured and
  continues, if the pieces are not in the same group the disassembler abborts
  its work and says that the assembly can not be disassembled. This is the
  basic idea, but there is a bit more to it.

  First there is ''Group 0''. All pieces inside this group need to be
  separated from one another. This group is there so that it is not required
  to place all the pieces into their own group, when you want to disassemble
  the puzzle. Pieces are automatically in Group 0 so you don't need to take
  care of that case.

  Pieces can be in more than one group. If you have for example a puzzle
  where you know that the piece A either interlocks with piece B or piece C
  and can not be separated from it, but you don't know to which of the two
  pieces B or C piece A it attatched you can assign Group 1 and Group 2 to
  piece A and group 1 to piece B and group 2 to piece C. This way the
  disassembler finds that both pieces are in group 1 when A and B are
  inseparable and it finds that both pieces are in group 2 when A and C can
  not let from each other.

  Pieces with the same shape need to have the same group assignment. That
  means if you have 3 pieces of shape 1 all 3 either all 3 pieces can be in
  group 1 or none of them. But you can also say how many of the 3 pieces may
  be in group 1 <em|maximally>. That means you can say not more than 3 pieces
  of the 3 <em|may> be in group 1.

  Before we now come to the way this works let me show you how you enter the
  required information. Once you press the ``Grp'' button in the problem tab
  a window opens. Here you find a table that has a line for each shape that
  is in the current problem. The first column contains the shape number, then
  comes a number that says how many times this shape is in the puzzle. And
  the following columns all stand for one group. When you first create a
  puzzle there are just 2 columns but you can add more columns by pressing
  the ``Add Group'' button (The unused groups at the end of the list will
  vanish once you close the window). Once you have a group column you can add
  numbers by clicking into the cells. Colored cells contain numbers not
  equlal to zero. Cells with a zero are gray and don't have a number inside.
  The numbers you enter here are the <em|maximal> number of pieces of this
  shape that <em|may> go into this group.

  Now what does it all come together: The disassembler starts to do its work.
  For each subproblem it first checks, if there is a unique group assignment
  for all involved pieces, e.g. all pieces have exactly one group assigned
  and that group is identical it doesn't even try to disassemble that
  subproblem. If this is not the case it tries to disassemble. In case of a
  failure it adds the pieces that are in this subproblem to a list of lists
  of pieces (oh yes this sounds strange). This is a table, each table entry
  contains a list of pieces. Once we are done with the disassembler the
  program comes back to this table and tries to assign a group to each list
  of pieces that are in the table. It just tries all possibilities. When the
  sum if pieces that are now in one group is bigger than the one that you
  have entered in the dialog above this is not a valid group assignment. If
  the program can find a valid assignment the puzzle is disassembled, if it
  can not the puzzle is assumed to be not disassembable. An example: assume
  we have a puzzle that contains (among others) 5 pieces of shape A. 3 of
  them might go into group 1 and another 2 into group 2. There is also a
  piece B that might go into group 1 After the disassembler we have the
  following list of pieces in the table:\ 

  <\enumerate-numeric>
    <item>AA

    <item>AAB
  </enumerate-numeric>

  Now the program has to assign group 2 to the first set of pieces and group
  1 to the 2nd set of pieces. Because otherwise piece B would be in the wrong
  group, it can only be in group 1. If there would be another piece A in set
  1 it would not be possible to assign groups because we can only have 2
  piece A in group 2. But if would be possible to have another piece A in the
  2nd set.

  I have no idea how useful this might be with puzzles as most of the
  currently available puzzles require a complete disassemly. But who knows
  maybe this feature will help in the design of lots of puzzles new and crazy
  ideas.

  <subsection|Solve Tab>

  Inside this tab you can start and stop the solving of a puzzle. But before
  you start there are a few options that may be useful. First the switch
  labeled ''\Rdisassemble``. If it is set the program tries to disassemble
  the assemblies found. Only the assemblies that disassemble are added to the
  list of solutions. Puzzles like the pentominos don't need this option set
  as all found assemblies are assembable. It would only slow down the
  computations.

  Then there is the ''just count'' switch. Press this if you are not
  interested in the solutions themselves but only in the number of them.

  The 3 buttons that start the search, continue a search that was stopped and
  stop a search. The program tries to solve the problem that is selected in
  the top list. You can even stop the current search. Save the current state
  and resume solving after a while. If you load the puzzle file you just
  press continue and the program will continue.

  Once a solution has been found it will be displayed in the 3D view. You can
  rotate it as usual.

  On the very bottom there is also a list of all the pieces visible. On this
  list the pieces that occur multiple times have an additional number
  appended to the number of the shape they belong to. Also the colour is
  slightly modified for each piece. If you click on the field for one piece
  you can toggle between normal view, wire-frame view and invisible mode for
  each piece. This is helpful if you want to shift pieces into a box. The box
  would hide most of the action that is going on inside it.

  The progress meter is only a very rough estimation. It is impossible to
  make a real guess at how far the problem has been checked.

  Below the progess meter is a line showing the current activity of the
  program. The following texts are possible:

  <\description-compact>
    <item*|nothing>This means that the program does not do anything on the
    problem and also no information is available about past tries to solve
    the problem

    <item*|prep>This is the first step in solving the problem. The program
    does some preparation. This step can not be interrupted it has to be
    finished bevore a stop has eny effect. This step also occures when an
    already started problem is resumed.

    <item*|red>This step also belongs to the preparation, can neither be
    interrupted and also occures after resume. Here the program tries to
    reduce the possible placement of pieces.

    <item*|assm>The program currently tries to find assemblies

    <item*|disassm>The program tries to disassemble a found assembly. This
    step can not be interrupted. When you press stop here the program will
    first finish to disassemble the puzzle. But normally this doesn't take
    long.

    <item*|pause>You have pressed stop and the program has stopped and
    waiting for you to either continue or start anew or save or whatever

    <item*|finished>Everything has been checked all found solutions are
    available for access with the sliders below

    <item*|plase wait>You have pressed stop but the program currently does
    something that can not be interrupted. The program finishes that step and
    then stops the search for solutions.

    <item*|error>You did something wrong. The program should have displayed
    an error message. You should edit the puzzle to remove the found problem.
  </description-compact>

  <section|Future Plans>

  So, what are my future plans? There are a lot of things that are still
  missing from the current program. A list of things that might be
  interesting to implement are the following things:

  <\itemize-dot>
    <item>Include the burr growing into the GUI. This might not happen any
    time soon but it might be useful.

    <item>Include a possibility to generate images with solutions. Currently
    you need the program to animate for you.

    <item>Add some special algorithms that are faster for certain kind of
    puzzles. The current algorithms is quite good for nearly all puzzles, but
    it's not <em|the> fastest.

    <item>The disassembly could use some speedups. Here <name|PuzzleSolver3D>
    is sometimes much faster (but also sometimes much slower)
  </itemize-dot>

  I would be very happy to get contributions from other people. After all
  there are quite a few people out there that have their own puzzle solving
  programs, maybe they have some nice additions. There is one important thing
  to keep in mind: the additions have to run on <name|Linux>. So you can not
  use any proprietary library that is not available for <name|Linux>.

  <chapter|The Internals>

  This chapter explains some of the internals. It is still quite incomplete,
  probably out of date and might even be wrong...

  <section|The puzzle file format>

  For those people that want to do things that the GUI is not supporting the
  exact file format of \ the files used by the GUI and the library may be of
  interest.

  The formamt is actually a gzip compressed XML-File. The program can read
  both, compressed and uncompressed files transparently so you don't need to
  zip them bevore loading into the program. The GUI always write compressed
  files so if you want to change something in them you first need to
  decompress it.

  I wont describe all the elements of the XML-File, it's easier if you enter
  something similar to what you need in the GUI and look in which way the
  program saves these information.

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

  <subsection|Class voxel>

  This class contains function so organize, modify, transform 3-dimensional
  arrays of cubes. Each entry inside the array contains 2 values:

  <\itemize>
    <item>The type of voxel (is it emtpy <verbatim|VX_EMPTY>, filled
    <verbatim|VX_FILLED> or a variable cube <verbatim|VX_VARIABLE>

    <item>The color constraint color. Here values between 0 and 64 are
    possible. 0 is the neutral color.
  </itemize>

  The class provides a set of functions to rotate, translate, mirror, resize
  and minimize the shape. The <verbatim|transform> function allows to
  generate all possible rotations <emdash> also including mirroring, if
  whished. The function <verbatim|selfSymmetries> calculates which of these
  transformations result in the same shape. <verbatim|Connected> finds out
  the all the cubes in the shape are connected in one big piece (neither the
  assembler nor the disassembler requests that this is the case).

  If all this is not enough then there are functions that return the value of
  the different cubes inside the shape and also to set the value of the
  cubes. These functions exists in different versions. One requires the x, y
  and z coordinate of the cube requestet. The other just takes one number.
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

  <subsection|Class puzzle>

  This class contains all the information of the puzzle including the shapes,
  the result shape and piece shapes and number, the color constraints, the
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
  interesed inthe solution he has to delete it.

  <subsection|Class disassember>

  The disassembler tries to find out if the puzzle can be taken apart.

  <subsection|Example>

  A very simple example can be found within the source code of the project.
  Check the burrTxt sources. They just check a few command line options, load
  the puzzle and then solve it, no fuzz with user interface, multi threaded
  application, ...

  <section|The Algorithms>

  There are only two algorithms of interest inside this program. One is the
  assembly algorithm. This one is based on the ``Dancing Link'' algorithm
  from D.E.Knuth. I needed to update thi algorithm in 2 ways:

  <\enumerate-numeric>
    <item>We require cubes that <em|may be filled> as well as cubes that
    <em|must be filled>. The original algorithm only provides the 2nd type of
    cubes.

    <item>We need to do something about multiple identical pieces. The
    original algorithm will find <with|mode|math|<big|prod><rsub|s\<in\>shapes>num(s)!>
    as many solutions as there really are.
  </enumerate-numeric>

  The 2nd interesting algorithm is the disassembler. This is mainly a bredth
  first tree search over all possible placements of the pieces.

  <subsection|Assembly>

  I will describe the only the basics for the origianl dancing link
  algorithm. For further information read the document available on Mr.
  Knuths webpage (<verbatim|http://www-cs-faculty.stanford.edu/~knuth/musings.html>).

  The algorithm represents the puzzle as a matrix. In this matrix the first
  columns represents the pieces and the last columns represent one cube of
  the result each.

  Each line of the matrix corresponds to one possible placement of one piece
  inside the result. The column of the piece and the columns the represent
  the places inside the colution that the piece occupies with the placement
  are 1 inside the matrix. All the other cells are 0.

  The search itself runs on this matrix. We search for a set of lines that
  all the lines contain exactly one 1 in each column. This means that each
  piece must be used and each cube in the result must be filled.

  The algorithm does 2 operations on the matrix:

  <\enumerate-numeric>
    <item>Cover column n and uncover column n. This means that the column is
    removed from the matrix and no longer taken into accound for the search.
    When a column is removed all the rows that contain a 1 in this column
    will also be removed.

    <item>Cover and uncover row n. This means that we select this row for the
    set of rows that we search. The row covering also removes and reincludes
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

  The algorithm is now recoursively trying all possibilities. It selects one
  column and then tries covers all rows that contain a 1 in this columns and
  then calls itself.

  It finished when there are either no more columns left. Then we have found
  a solution or there is one column with no rows. Then we have found a dead
  end and backtrack.

  This algorithm is per se not dependent on square cubes it is not dependent
  on any shape. You only need to transfere your puzzle into the matrix. Even
  William Waites puzzles should be possible. But as the square and cubes are
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
  algorithm decides to place one of the pieces that occure multiple times it
  will also place all the others and always check that these have larger
  placement numbers.

  <subsection|Disassembly>

  The disassembly algorithm is a breadth first tree search. In this tree
  every node represents one possible relative position of the pieces. To find
  out what can be moved in this node the algorithm Bill Cuttler used for his
  6 Piece Burr analysis is used. Because this analysis of the movable pieces
  a part of the information is saved within a cache. This caching is done in
  the class <verbatim|movementcache> This cache can be retained from one
  disassembly analysis and this speeding up puzzles that have a lot of
  assemblies to analyze.

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
  link algorithm. This preparation part is called the frontend.
</body>

<\initial>
  <\collection>
    <associate|font|roman>
    <associate|language|british>
    <associate|page-breaking|optimal>
    <associate|page-medium|papyrus>
    <associate|page-orientation|portrait>
    <associate|par-columns|1>
    <associate|par-hyphen|professional>
    <associate|sfactor|4>
  </collection>
</initial>

<\references>
  <\collection>
    <associate||<tuple|1.1|5>>
    <associate|MainWindowImage|<tuple|1.1|7>>
    <associate|ToolsImage|<tuple|1.2|7>>
    <associate|auto-1|<tuple|<uninit>|4>>
    <associate|auto-10|<tuple|1.2|8>>
    <associate|auto-11|<tuple|1.3|8>>
    <associate|auto-12|<tuple|1.3.4|9>>
    <associate|auto-13|<tuple|1.3.4.1|10>>
    <associate|auto-14|<tuple|1.3.5|11>>
    <associate|auto-15|<tuple|1.4|11>>
    <associate|auto-16|<tuple|2|11>>
    <associate|auto-17|<tuple|2.1|11>>
    <associate|auto-18|<tuple|2.2|12>>
    <associate|auto-19|<tuple|2.2.1|12>>
    <associate|auto-2|<tuple|1|5>>
    <associate|auto-20|<tuple|2.2.2|12>>
    <associate|auto-21|<tuple|2.2.3|12>>
    <associate|auto-22|<tuple|2.2.4|12>>
    <associate|auto-23|<tuple|2.2.5|12>>
    <associate|auto-24|<tuple|2.3|13>>
    <associate|auto-25|<tuple|2.3.1|13>>
    <associate|auto-26|<tuple|2.3.2|?>>
    <associate|auto-27|<tuple|2.4|?>>
    <associate|auto-3|<tuple|1.1|5>>
    <associate|auto-4|<tuple|1.2|5>>
    <associate|auto-5|<tuple|1.3|6>>
    <associate|auto-6|<tuple|1.3.1|6>>
    <associate|auto-7|<tuple|1.3.2|7>>
    <associate|auto-8|<tuple|1.1|7>>
    <associate|auto-9|<tuple|1.3.3|7>>
    <associate|footnote-1|<tuple|1|1>>
    <associate|shapeEditTools|<tuple|1.3|8>>
  </collection>
</references>

<\auxiliary>
  <\collection>
    <\associate|figure>
      <tuple|normal|The Main Window<label|MainWindowImage>|<pageref|auto-7>>

      <tuple|normal|The tools<label|ToolsImage>|<pageref|auto-9>>

      <tuple|normal|Shape edit tools<label|shapeEditTools>|<pageref|auto-10>>
    </associate>
    <\associate|toc>
      <vspace*|1fn><with|font-series|<quote|bold>|math-font-series|<quote|bold>|Table
      of contents> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-1><vspace|0.5fn>

      <vspace*|1fn><with|font-series|<quote|bold>|math-font-series|<quote|bold>|1<space|2spc>The
      Program> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-2><vspace|0.5fn>

      1.1<space|2spc>Introduction <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-3>

      1.2<space|2spc><with|font-shape|<quote|small-caps>|PuzzleSolver3D>
      Users <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-4>

      1.3<space|2spc>New Users <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-5>

      <with|par-left|<quote|1.5fn>|1.3.1<space|2spc>Main Window
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-6>>

      <with|par-left|<quote|1.5fn>|1.3.2<space|2spc>Piece Tab
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-8>>

      <with|par-left|<quote|1.5fn>|1.3.3<space|2spc>Problem Tab
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-11>>

      <with|par-left|<quote|3fn>|1.3.3.1<space|2spc>Grouping
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-12>>

      <with|par-left|<quote|1.5fn>|1.3.4<space|2spc>Solve Tab
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-13>>

      1.4<space|2spc>Future Plans <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-14>

      <vspace*|1fn><with|font-series|<quote|bold>|math-font-series|<quote|bold>|2<space|2spc>The
      Internals> <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-15><vspace|0.5fn>

      2.1<space|2spc>The puzzle file format
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-16>

      2.2<space|2spc>The Library <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-17>

      <with|par-left|<quote|1.5fn>|2.2.1<space|2spc>Class voxel
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-18>>

      <with|par-left|<quote|1.5fn>|2.2.2<space|2spc>Class puzzle
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-19>>

      <with|par-left|<quote|1.5fn>|2.2.3<space|2spc>Class assembler
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-20>>

      <with|par-left|<quote|1.5fn>|2.2.4<space|2spc>Class disassember
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-21>>

      <with|par-left|<quote|1.5fn>|2.2.5<space|2spc>Example
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-22>>

      2.3<space|2spc>The Algorithms <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-23>

      <with|par-left|<quote|1.5fn>|2.3.1<space|2spc>Assembly
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-24>>

      <with|par-left|<quote|1.5fn>|2.3.2<space|2spc>Disassembly
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-25>>

      2.4<space|2spc>Adding to the Library
      <datoms|<macro|x|<repeat|<arg|x>|<with|font-series|medium|<with|font-size|1|<space|0.2fn>.<space|0.2fn>>>>>|<htab|5mm>>
      <no-break><pageref|auto-26>
    </associate>
  </collection>
</auxiliary>