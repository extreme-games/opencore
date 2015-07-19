#
# opencore Makefile
#

CXX = g++

#CXXFLAGS =  -Wno-write-strings -fno-strict-aliasing -Wall -pthread -Wl,-rpath,./libs/ -Wl,--export-dynamic
CXXFLAGS =  -Wno-write-strings -Wall -pthread -Wl,-rpath,./libs/ -Wl,--export-dynamic

DEFS = -DLOCK_GETADDRINFO -DCORE_BUILD

INCLUDEDIRS = -I. -Iinclude 

LIBDIRS = -L.

LIBS = -lpthread -ldl -lc -lmysqlclient

#DFLAGS = -g -pg
DFLAGS = -g

NFLAGS = -O3

SOURCEFILES = opencore.cpp botman.cpp cmd.cpp cmdexec.cpp config.cpp db.cpp encrypt.cpp lib.cpp log.cpp msg.cpp ops.cpp parsers.cpp phand.cpp player.cpp pkt.cpp psend.cpp util.cpp

opencore: $(SOURCEFILES)
	$(CXX) -o $@ $(DFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)

opencore.prod: $(SOURCEFILES)
	$(CXX) -o $@ $(NFLAGS) $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) $(SOURCEFILES) $(LIBS)

clean:
	rm -fr *.o *.do opencore

