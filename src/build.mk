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

# S+core7 disassembler objects (also linked into main for fault reports and into the tests)
SDIS_DIR       = ../tools/sdis
SDIS_LIB_OBJS  = $(SDIS_DIR)/scoredsm.o $(SDIS_DIR)/score_disasm.o

#all objects 
OBJS = main.o Bus.o spg290.o ppu.o IRQController.o DiscImage.o FaultReport.o TVE.o Timer.o I2C.o CdServo.o Boot.o System.o

# Default target
all: $(TARGET)

# Link the files
$(TARGET): $(OBJS) $(SDIS_LIB_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# compile main
main.o: main.cpp System.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile system (run loop)
System.o: System.cpp System.h Boot.h Bus.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile spg290
spg290.o: spg290.cpp Bus.o 
	$(CXX) $(CXXFLAGS) -c $< -o $@

# fault report (uses the S+core7 disassembler)
FaultReport.o: FaultReport.cpp FaultReport.h spg290.h $(SDIS_DIR)/score_disasm.h
	$(CXX) $(CXXFLAGS) -I$(SDIS_DIR) -c $< -o $@

# compile bus
Bus.o: Bus.cpp Bus.h ppu.o IRQController.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile irq controller
IRQController.o: IRQController.cpp IRQController.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile disc image reader
DiscImage.o: DiscImage.cpp DiscImage.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# compile ppu
ppu.o: ppu.cpp ppu.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_OBJS) $(TEST_TARGET) $(SDIS_OBJS) $(SDIS_TARGET)

# ----------------------------------------------------------------
# Test target  —  requires: sudo apt-get install libgtest-dev
#
# Usage: make -f build.mk test
# ----------------------------------------------------------------
# The conformance test links MAME's S+core7 disassembler as an independent decoder.

TEST_SRCS  = $(wildcard ../tests/*.cpp)
TEST_OBJS  = $(patsubst ../tests/%.cpp, ../tests/%.o, $(TEST_SRCS))
TEST_TARGET = ../tests/test_runner

# Any change to a src header rebuilds every object (class layouts are shared).
HDRS = $(wildcard *.h)
$(OBJS) $(TEST_OBJS): $(HDRS)
# Tests that embed a generated ROM header must rebuild when it changes.
$(TEST_OBJS): $(wildcard ../tests/roms/generated/*.h)

.PHONY: test
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): Bus.o spg290.o ppu.o IRQController.o DiscImage.o FaultReport.o TVE.o Timer.o I2C.o CdServo.o Boot.o System.o $(SDIS_LIB_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile test sources with -I. so they can find Bus.h / spg290.h (and -I$(SDIS_DIR) for score_disasm.h)
../tests/%.o: ../tests/%.cpp ../tests/test_helpers.h
	$(CXX) $(CXXFLAGS) -I. -I$(SDIS_DIR) -c $< -o $@

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

# ----------------------------------------------------------------
# S+core7 disassembler (MAME scoredsm.cpp, BSD-3-Clause; see THIRD_PARTY_NOTICES.md)
#
# Usage: make -f build.mk sdis
#        ../tools/sdis/sdis <file> <base> [start] [end]
# score_disasm.o (score7Disasm) is also linked by tests as an independent decoder.
# ----------------------------------------------------------------
SDIS_DIR    = ../tools/sdis
SDIS_OBJS   = $(SDIS_DIR)/scoredsm.o $(SDIS_DIR)/score_disasm.o $(SDIS_DIR)/main.o
SDIS_TARGET = $(SDIS_DIR)/sdis

.PHONY: sdis
sdis: $(SDIS_TARGET)

$(SDIS_TARGET): $(SDIS_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# MAME's source trips -Wextra warnings we do not want to patch; keep the copy unmodified.
$(SDIS_DIR)/scoredsm.o: $(SDIS_DIR)/scoredsm.cpp $(SDIS_DIR)/scoredsm.h $(SDIS_DIR)/scorem.h $(SDIS_DIR)/emu.h
	$(CXX) $(CXXFLAGS) -Wno-unused-parameter -c $< -o $@

$(SDIS_DIR)/score_disasm.o: $(SDIS_DIR)/score_disasm.cpp $(SDIS_DIR)/score_disasm.h $(SDIS_DIR)/scoredsm.h $(SDIS_DIR)/emu.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SDIS_DIR)/main.o: $(SDIS_DIR)/main.cpp $(SDIS_DIR)/score_disasm.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
