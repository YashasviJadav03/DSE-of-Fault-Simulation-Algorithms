CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Iinclude

SRCS = src/parser.cpp \
       src/simulator.cpp \
       src/testvector.cpp \
       src/fault_collapsing.cpp \
       src/compaction.cpp \
       src/serial_sim.cpp \
       src/parallel_pattern_sim.cpp \
       src/concurrent_sim.cpp \
       src/dse_engine.cpp \
       main.cpp

OBJS = src/parser.o \
       src/simulator.o \
       src/testvector.o \
       src/fault_collapsing.o \
       src/compaction.o \
       src/serial_sim.o \
       src/parallel_pattern_sim.o \
       src/concurrent_sim.o \
       src/dse_engine.o \
       main.o

TARGET = fsim

.PHONY: all clean run-c17 run-all test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)
	@echo "Build successful! Run ./$(TARGET) --help for options."

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "Cleaned build artifacts."

test: $(TARGET)
	./$(TARGET) --circuit benchmarks/c17.txt --algo all --compact --dse

run-all: $(TARGET)
	./$(TARGET) --all-benchmarks --export
