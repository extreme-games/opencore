
CXX = g++
CXXFLAGS = -g -Wno-write-strings -Wall -pthread -shared -fPIC
INCLUDEDIRS = -I. -Iinclude -I../..
LIBDIRS = -L..
LIBS = -lpthread 
DFLAGS = -g -Wint-to-pointer-cast -fpermissive
DEFS = 

default:
	make duelbot.so

duelbot.so: commands.cpp db.cpp duel.cpp dueltypes.cpp elo.cpp handlers.cpp pinfo.cpp userdata.cpp util.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) commands.cpp db.cpp duel.cpp dueltypes.cpp elo.cpp handlers.cpp pinfo.cpp userdata.cpp util.cpp $(LIBS)

clean:
	rm -fr *.o *.do *.so

