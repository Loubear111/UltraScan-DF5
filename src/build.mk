#Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

#target exe
TARGET = main

#all objects 
OBJS = main.o Bus.o spg290.o

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
Bus.o: Bus.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

