CXXFLAGS=-O0 -g
coroutines: coroutines.cpp
	g++-10 -std=c++20 -fsanitize=address -fcoroutines $(CXXFLAGS) ./coroutines.cpp -luv -o coroutines
all: coroutines
release: CXXFLAGS=-O3
release: coroutines
clean:
	rm -f ./coroutines
