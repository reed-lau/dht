all:dht

dht:dht.cc
	g++ $< -o $@ -std=c++14 -O2 -luv
