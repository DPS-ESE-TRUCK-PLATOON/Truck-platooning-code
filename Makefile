CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2
CUNIT_LIBS = -lcunit

all: follower lead

follower: follower.cpp network.hpp truck.cpp protocol.hpp encoder.hpp decoder.hpp
	$(CXX) $(CXXFLAGS) follower.cpp -o follower

lead: lead.cpp protocol.hpp encoder.hpp decoder.hpp
	$(CXX) $(CXXFLAGS) lead.cpp -o lead

test: tests/cunit_tests.cpp truck.cpp network.cpp network.hpp protocol.hpp encoder.hpp decoder.hpp
	$(CXX) $(CXXFLAGS) tests/cunit_tests.cpp truck.cpp network.cpp -o tests/test_runner $(CUNIT_LIBS) 
	./tests/test_runner

clean:
	rm -f follower lead tests/test_runner

.PHONY: all clean test
