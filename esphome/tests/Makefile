CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1

test: test_x3d_frame
	./test_x3d_frame

test_x3d_frame: test_x3d_frame.cpp ../components/x3d_rfm69/x3d_frame.cpp ../components/x3d_rfm69/x3d_frame.h
	$(CXX) $(CXXFLAGS) -o $@ test_x3d_frame.cpp ../components/x3d_rfm69/x3d_frame.cpp

clean:
	rm -f test_x3d_frame

.PHONY: test clean
