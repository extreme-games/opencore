#
# opencore example libraries Makefile
#

CXX = g++
CXXFLAGS = -g -Wno-write-strings -Wall -pthread -shared -fPIC
INCLUDEDIRS = -I. -Iinclude -I..
LIBDIRS = -L..
LIBS = -lpthread 
DFLAGS = -g
DEFS = 


default:
	make features.so rabbit.so chat.so query.so chess.so egc.so

query.so: query.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) query.cpp $(LIBS)

egc.so: egc.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) egc.cpp $(LIBS)

chess.so: chess.cpp
	$(CXX) -o $@ $(CXXFLAGS) -Wno-int-to-pointer-cast $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) chess.cpp $(LIBS)

features.so: features.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) features.cpp $(LIBS)

rabbit.so: rabbit.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) rabbit.cpp $(LIBS)

chat.so: chat.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) chat.cpp -lncurses $(LIBS)

clean:
	rm -fr *.o *.do *.so

