# Build & run the shared-logic unit tests with plain g++ (no PlatformIO).
#
#   make        # build + run
#   make clean  # remove the binary
#
# Only src/timetable.cpp is pulled in (plus header-only include/ClockTime.h);
# the tested code has no HAL/Arduino/SDL dependency.

CXX      ?= g++
CXXFLAGS := -std=gnu++17 -Wall -Wextra -I../include -I../src
SRCS     := $(wildcard *.cpp) ../src/timetable.cpp
HDRS     := $(wildcard *.h) ../src/timetable.h ../include/ClockTime.h
BIN      := run_tests

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

clean:
	rm -f $(BIN)
