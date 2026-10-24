#Compiler and Flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

# Path to googletest build artifacts
GTEST_LIB_DIR = ../third_party/googletest/build/lib
GTEST_INCLUDE_DIR = ../third_party/googletest/googletest/include

# Default Linker Flags
LDFLAGS = -lgtest -lgtest_main -pthread

# Configuration to support both Windows (local) and Linux (system)
# If GTEST_LIB_DIR is provided (Windows builds), we use the local paths.
# If it's empty (Linux builds), we use the system default.
ifneq ($(GTEST_LIB_DIR),)
# Windows / Local Build mode
CXXFLAGS += "-I$(GTEST_INCLUDE_DIR)"
LDFLAGS += "-L$(GTEST_LIB_DIR)"
endif

#target exe
TARGET = main

#all objects 
OBJS = main.o Bus.o spg290.o ppu.o IRQController.o

# Default target
all: $(TARGET)

# Link the files
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# compile main
main.o: main.cpp Bus.o spg290.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile spg290
spg290.o: spg290.cpp Bus.o 
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile bus
Bus.o: Bus.cpp Bus.h ppu.o IRQController.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile irq controller
IRQController.o: IRQController.cpp IRQController.h
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

$(TEST_TARGET): Bus.o spg290.o ppu.o IRQController.o $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile test sources with -I. so they can find Bus.h / spg290.h
../tests/%.o: ../tests/%.cpp
	$(CXX) $(CXXFLAGS) -I. -c $< -o $@

# ----------------------------------------------------------------
# ROM assembly target  —  requires score-elf-as from binutils 2.25
#
# Build:  make -f build.mk roms
# The generated C++ headers are committed so CI never needs the toolchain.
# ----------------------------------------------------------------
# Comment from Louis: Make sure to install the toolchain and update these paths if needed. You likely will need to download binutils 2.25 and build it yourself
SCORE_AS      = /usr/local/score-elf/bin/score-elf-as
SCORE_LD      = /usr/local/score-elf/bin/score-elf-ld
SCORE_OBJCOPY = /usr/local/score-elf/bin/score-elf-objcopy
SCORE_LDSCRIPT = ../tests/roms/score.ld
BIN_TO_CPP    = python3 ../tools/bin_to_cpp.py

ROM_SRCS = $(wildcard ../tests/roms/*.s)
ROM_OBJS = $(patsubst ../tests/roms/%.s, ../tests/roms/generated/%.o,   $(ROM_SRCS))
ROM_ELFS = $(patsubst ../tests/roms/%.s, ../tests/roms/generated/%.elf, $(ROM_SRCS))
ROM_BINS = $(patsubst ../tests/roms/%.s, ../tests/roms/generated/%.bin, $(ROM_SRCS))
ROM_HDRS = $(patsubst ../tests/roms/%.s, ../tests/roms/generated/%.h,   $(ROM_SRCS))

.PHONY: roms
roms: $(ROM_HDRS)

../tests/roms/generated/%.o: ../tests/roms/%.s
	$(SCORE_AS) -SCORE7 -I ../tests/roms -o $@ $<

../tests/roms/generated/%.elf: ../tests/roms/generated/%.o
	$(SCORE_LD) -T $(SCORE_LDSCRIPT) -o $@ $<

../tests/roms/generated/%.bin: ../tests/roms/generated/%.elf
	$(SCORE_OBJCOPY) -O binary --only-section=.text $< $@

../tests/roms/generated/%.h: ../tests/roms/generated/%.bin
	$(BIN_TO_CPP) $< $@
