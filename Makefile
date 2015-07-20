#
# opencore Makefile
#

CXX = g++

#CXXFLAGS =  -Wno-write-strings -fno-strict-aliasing -Wall -pthread -Wl,-rpath,./libs/ -Wl,--export-dynamic
CXXFLAGS =  -Wno-write-strings -Wall -pthread -Wl,-rpath,./libs/ -Wl,--export-dynamic

DEFS = -DLOCK_GETADDRINFO -DCORE_BUILD

INCLUDEDIRS = -I. -Iinclude -I./lua-5.3.1/src -I/usr/include/python2.7

LIBDIRS = -L.

LIBS = -lpthread -ldl -lc -lmysqlclient -lpython2.7

#DFLAGS = -g -pg
DFLAGS = -g

NFLAGS = -O3

HEADERFILES = *.hpp
SOURCEFILES = opencore_swig.cpp opencore.cpp botman.cpp cmd.cpp cmdexec.cpp config.cpp db.cpp encrypt.cpp lib.cpp log.cpp msg.cpp ops.cpp parsers.cpp phand.cpp player.cpp pkt.cpp psend.cpp util.cpp ./lua-5.3.1/src/liblua.a

opencore: $(SOURCEFILES) opencore_wrap.c opencore.i
#	swig -python opencore.i
	$(CXX) -o $@ $(DFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)

opencore_wrap: opencore_wrap.c
	 g++ -I/usr/include/python2.7 -shared -fPIC opencore_wrap.c -o _opencore.so

swig: $(SOURCEFILES) $(HEADERFILES)
	swig -python opencore.i


opencore.prod: $(SOURCEFILES)
	$(CXX) -o $@ $(NFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)

clean:
	rm -fr *.o *.do opencore

