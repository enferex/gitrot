CXX=g++
CXXFLAGS=-g3 -Wall -std=c++11 -DDEBUG
CXXOBJS=stale.o
APP=stale

all: $(APP)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $^ -o $@

$(APP): $(CXXOBJS)
	$(CXX) $^ -o $@

clean:
	$(RM) $(CXXOBJS)
