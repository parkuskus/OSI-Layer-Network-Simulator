# Compiler settings
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -I.
LDFLAGS = 

# Target executable
TARGET = magi_system

# Build directory for object files
BUILD_DIR = build

# Source files
SOURCES = main.cpp \
          cli.cpp \
          core/packet.cpp \
          core/interface.cpp \
          core/link.cpp \
          core/node.cpp

# Object files (in build directory)
OBJECTS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

# Header files
HEADERS = cli.hpp \
          core/packet.hpp \
          core/interface.hpp \
          core/link.hpp \
          core/node.hpp

# Default target
all: $(TARGET)

# Create build directories and compile
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files to object files (in build directory)
$(BUILD_DIR)/%.o: %.cpp $(HEADERS)
	@mkdir -p $(BUILD_DIR)/core
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the simulator
run: $(TARGET)
	./$(TARGET)

.PHONY: clean rebuild all run

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TARGET).exe

# Clean and rebuild
rebuild: clean all

# Run on Windows
run-win: $(TARGET)
	$(TARGET).exe

# Clean on Windows
clean-win: clean

# Phony targets
.PHONY: all run clean rebuild clean-win run-win
