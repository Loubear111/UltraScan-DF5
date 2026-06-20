#Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

#target exe
TARGET = main

#all objects 
OBJS = main.o Bus.o spg290.o ppu.o

# Default target
all: $(TARGET)

# Link the files
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# compile main
main.o: main.cpp Bus.o spg290.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile spg290
spg290.o: spg290.cpp Bus.o 
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile bus
Bus.o: Bus.cpp ppu.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile ppu
ppu.o: ppu.cpp ppu.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET)

# ----------------------------------------------------------------
# Test target  —  requires: sudo apt-get install libgtest-dev
#
# Usage: make -f build.mk test
# ----------------------------------------------------------------
TEST_SRCS  = $(wildcard ../tests/*.cpp)
TEST_OBJS  = $(patsubst ../tests/%.cpp, ../tests/%.o, $(TEST_SRCS))
TEST_TARGET = ../tests/test_runner

.PHONY: test
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): Bus.o spg290.o ppu.o $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ -lgtest -lgtest_main -pthread

# Compile test sources with -I. so they can find Bus.h / spg290.h
../tests/%.o: ../tests/%.cpp
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

