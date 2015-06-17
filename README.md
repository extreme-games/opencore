OpenCore
========

This is the OpenCore botcore for SubSpace/Continuum.

API header
==========
The API header file is opencore.hpp and must be the only file included by plugins.

Programming style guidelines
============================
Commits to the core must be C-style with the exception of C++ Containers and match the OpenBSD style(9) guidelines or otherwise be consistent with pre-existing opencore code:
http://www.openbsd.org/cgi-bin/man.cgi/OpenBSD-current/man9/style.9?query=style&sec=9

Commits to plugins must match the programming style they were written in and all files within a plugin's source files must have consistent styling.

Example libraries
=================
Example libraries are found in the libs/ directory.

Quickstart
==========
1) Clone and compile the source tree with 'make'
2) Edit types/master.conf:
2a)  net.servername
2b)  net.serverport
2c)  login.username
2d)  login.password
3) Edit ops.conf
4) Run ./opencore and then interact through the bot in-game
