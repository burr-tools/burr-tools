This repository contains the BurrTools code with some modernisations
to make it run on modern systems.

## Download and install

Grab the file for your platform from the
[Releases page](https://github.com/burr-tools/burr-tools/releases/latest).

| Platform | Download | Notes |
| :--- | :--- | :--- |
| macOS, Apple Silicon (M1/M2/M3/M4) | the `.dmg` disk image | Read [the Gatekeeper note](#macos-burrtoolsapp-is-damaged-and-cant-be-opened) below before first launch |
| macOS, Intel | — | No prebuilt binary; [build from source](BUILD.md) |
| Windows, 64-bit | the `-windows-x86_64.zip` archive | Unpack and run `burrtools.exe` |
| Linux, 64-bit | the `-linux-x86_64.tar.gz` archive | Unpack and run `./burrtools` |

The macOS downloads are built for Apple Silicon only. An `-app.zip` is also
published, containing just `BurrTools.app` without the example puzzles or
this note — prefer the `.dmg` unless you know you want only the app.

`burrTxt` and `burrTxt2` are the command-line solvers. They ship inside the
Linux and Windows archives; the macOS builds publish them as separate
downloads.

### macOS: "BurrTools.app is damaged and can't be opened"

This is expected on a first launch, and the app is not damaged. BurrTools is
distributed without an Apple Developer signature, so macOS quarantines it on
download and then reports the missing signature with that message.

To clear the quarantine flag, open the disk image, drag `BurrTools.app` to
your Applications folder, and run:

```bash
xattr -cr /Applications/BurrTools.app
```

BurrTools will then open normally, and the command does not need to be
repeated. If you would rather not use Terminal, try to open the app once,
then go to **System Settings → Privacy & Security**, scroll to the Security
section, and click **Open Anyway** next to the message about BurrTools.

Note that Control-clicking the app and choosing **Open** — the usual advice
for unsigned apps — does not get past this particular message, and no longer
bypasses Gatekeeper at all as of macOS 15 Sequoia.

BurrTools was written by Andreas Röver, small patches provided by
several authors. Original README below.

---

BurrTools is a library to solve burr-type puzzles. Bundled with the
library comes a graphical program that lets you edit the puzzles and
view the found solutions.

The real documentation is inside the executable-file as on-line help.
And also available as a pdf for off-line reading or printouts.

I've also been able to bundle a few puzzles with the program to
demonstrate its features. Thanks to the designers. You can find these
puzzles in the examples subdirectory.

The library that contains all the mathematical and algorithmical stuff
is documented using doxygen. You can generate that documentation using
the doxygen.cfg file that is in the source-code of the program. I tried
to include algorithmic as well as organisatorical comments into the
doxygen documentation.

Have fun.


   Andreas Röver

---

* [Build Instructions](BUILD.md)
* [User Guide](https://burrtools.sourceforge.net/gui-doc/toc.html)
* [Library documentation](https://burrtools.sourceforge.net/lib-doc/index.html)
