CXX = g++
CXXFLAGS = -std=c++17 -pthread -Wall -Wextra -O2

all: follower lead

follower: follower.cpp network.hpp truck.cpp protocol.hpp encoder.hpp decoder.hpp
	$(CXX) $(CXXFLAGS) follower.cpp -o follower

lead: lead.cpp protocol.hpp encoder.hpp decoder.hpp
	$(CXX) $(CXXFLAGS) lead.cpp -o lead

clean:
	rm -f follower lead

.PHONY: all clean
