OpenCore
========
This is the OpenCore botcore for SubSpace/Continuum for use on Linux systems.

API header
==========
The API header file is opencore.hpp and must be the only file from the core included by plugins. It contains the documentation for the API.

Programming style guidelines
============================
Commits to the core must be C-style with the exception of C++ Containers and match [OpenBSD's style(9)](http://www.openbsd.org/cgi-bin/man.cgi/OpenBSD-current/man9/style.9?query=style&sec=9) guidelines or otherwise be consistent with pre-existing opencore code and conventions.

Example libraries
=================
Example libraries are found in the libs/ directory.

Quickstart
==========
1. Setup and configure a subgame2.exe server for testing
2. Clone then compile the source tree with 'make'
3. Edit the following fields in types/master.conf:
    1. net.servername
    2. net.serverport
    3. login.username
    4. login.password
4. Edit ops.conf
5. Run ./opencore and then interact with the bot in-game
