coroutines: coroutines.cpp
	g++-10 -std=c++20 -fsanitize=address -fcoroutines ./coroutines.cpp -luv -o coroutines

