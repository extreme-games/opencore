#
# chucklehead Makefile
#

CXX = g++

CXXFLAGS =  -g -Wno-write-strings -Wall -pthread -shared -fPIC

INCLUDEDIRS = -I. -Iinclude -I..

LIBDIRS = -L..

LIBS = -lpthread 

DFLAGS = -g

SOURCEFILES = features.cpp rabbit.cpp chat.cpp bank.cpp query.cpp greet.cpp idle.cpp github.cpp

default:
	make features.so rabbit.so chat.so bank.so query.so greet.so idle.so github.so

features.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) features.cpp $(LIBS)

query.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) query.cpp $(LIBS)

github.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) github.cpp $(LIBS)

bank.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) bank.cpp $(LIBS)

greet.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) greet.cpp $(LIBS)

idle.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) idle.cpp $(LIBS)

rabbit.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) rabbit.cpp $(LIBS)

chat.so: $(SOURCEFILES)
	$(CXX) -o $@ $(CXXFLAGS) $(INCLUDEDIRS) $(LIBDIRS) $(DEFS) chat.cpp -lncurses $(LIBS)

clean:
	rm -fr *.o *.do features.so rabbit.so chat.so bank.so query.so greet.so idle.so github.so

