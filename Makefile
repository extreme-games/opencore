#
# opencore Makefile
#

CXX = g++
CXXFLAGS =  -Wno-write-strings -Wall -pthread -Wl,-rpath,./libs/ -Wl,--export-dynamic
DEFS = -DLOCK_GETADDRINFO -DCORE_BUILD
INCLUDEDIRS = -I. -Iinclude -I/usr/include/python2.7
LIBDIRS = -L.
LIBS = -lpthread -ldl -lc -lmysqlclient -lpython2.7
DFLAGS = -g
NFLAGS = -O3

HEADERFILES = *.hpp
SOURCEFILES = opencore_swig.cpp opencore.cpp botman.cpp cmd.cpp cmdexec.cpp config.cpp db.cpp encrypt.cpp lib.cpp log.cpp msg.cpp ops.cpp parsers.cpp phand.cpp player.cpp pkt.cpp psend.cpp util.cpp

opencore: $(SOURCEFILES) opencore_wrap.c opencore.i
	make swig
	$(CXX) -o $@ $(DFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)

swig: $(SOURCEFILES) $(HEADERFILES)
	swig -python opencore.i

prod: $(SOURCEFILES)
	$(CXX) -o $@ $(NFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)
clean:
	rm -fr *.o *.do *.so opencore

