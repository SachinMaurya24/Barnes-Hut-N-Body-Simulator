CXX ?= c++
CXXFLAGS ?= -std=c++20 -O3 -march=native -ffast-math -Wall -Wextra -pthread
SRC = src/main.cpp
BUILD_DIR = build
TARGETS = $(BUILD_DIR)/nbody $(BUILD_DIR)/barnes_hut_simulator

all: $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/nbody: $(SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

$(BUILD_DIR)/barnes_hut_simulator: $(SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
