CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
INCLUDE_DIR = inclulde
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build
TARGET = $(BUILD_DIR)/kv

SRCS = $(SRC_DIR)/gshmp.cpp $(SRC_DIR)/stats.cpp $(SRC_DIR)/utils.cpp
TEST_SRCS = $(TEST_DIR)/kv.cpp
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(TARGET): $(OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
