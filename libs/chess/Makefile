#
# Chess Makefile
#

CXX = g++

CXXFLAGS =  -g -Wall -pthread -shared -fPIC

INCLUDEDIRS = -I. -Iinclude -I..

LIBDIRS = -L..

LIBS = -lpthread 

DFLAGS = -g

SOURCEFILES = chess.cpp

default:
	make chess.so

chess.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) chess.cpp $(LIBS)

clean:
	rm -fr chess.so

