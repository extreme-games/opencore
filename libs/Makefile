#
# chucklehead Makefile
#

CXX = g++

CXXFLAGS =  -g -Wno-write-strings -Wall -pthread -shared -fPIC

INCLUDEDIRS = -I. -Iinclude -I..

LIBDIRS = -L..

LIBS = -lpthread 

DFLAGS = -g

SOURCEFILES = basicbot.cpp features.cpp rabbit.cpp chat.cpp query.cpp greet.cpp

default:
	make basicbot.so features.so rabbit.so chat.so query.so greet.so

basicbot.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) basicbot.cpp $(LIBS)

features.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) features.cpp $(LIBS)

query.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) query.cpp $(LIBS)

greet.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) greet.cpp $(LIBS)

rabbit.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) rabbit.cpp $(LIBS)

chat.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) chat.cpp -lncurses $(LIBS)

clean:
	rm -fr *.o *.do basicbot.so features.so rabbit.so chat.so query.so greet.so

