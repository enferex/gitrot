CXX=g++
CXXFLAGS=-g3 -Wall -std=c++11 -pedantic
CXXOBJS=stale.o
APP=stale

all: $(APP)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $^ -o $@

$(APP): $(CXXOBJS)
	$(CXX) $^ -o $@

test: $(APP)
	./$(APP) test.c -s

clean:
	$(RM) $(CXXOBJS)
