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

benchmark: $(BUILD_DIR)/barnes_hut_simulator
	./$(BUILD_DIR)/barnes_hut_simulator --benchmark

sweep: $(BUILD_DIR)/barnes_hut_simulator
	./$(BUILD_DIR)/barnes_hut_simulator --merger-sweep

paper-experiments: all
	python3 scripts/run_paper_experiments.py

figures:
	python3 scripts/plot_paper_figures.py

video: $(BUILD_DIR)/barnes_hut_simulator
	python3 scripts/plot_spiral_showcase.py results/merger_prograde_snapshots.csv --output-video results/spiral_galaxy_merger_showcase.mp4

clean:
	rm -f $(TARGETS)

.PHONY: all clean benchmark sweep paper-experiments figures video
