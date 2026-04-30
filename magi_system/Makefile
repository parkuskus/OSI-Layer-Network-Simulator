# Compiler and build settings
CXX := g++
STD := -std=c++11
WARN := -Wall -Wextra
INCLUDES := -I.
CPPFLAGS := $(INCLUDES)
CXXFLAGS := $(STD) $(WARN)
LDFLAGS :=

# Targets and directories
TARGET := magi_system
BUILD_DIR := build

# Source files
SOURCES := \
    main.cpp \
    cli.cpp \
    core/packet.cpp \
    core/interface.cpp \
    core/link.cpp \
    core/node.cpp \
    layer2/ethernet.cpp \
    layer2/arp.cpp \
    layer2/host.cpp \
    layer2/switch.cpp

# Build artifacts
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all run makerun clean rebuild debug release run-win clean-win mingw32-makerun

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile with auto-generated dependency files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Include dependencies when present
-include $(DEPS)

# Run helpers
run: $(TARGET)
	./$(TARGET)

# Convenience alias used in README
makerun: run

# Build modes
debug: CXXFLAGS += -O0 -g
debug: rebuild

release: CXXFLAGS += -O2
release: rebuild

# Cleanup
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(TARGET).exe

rebuild: clean all

# Windows-friendly aliases
run-win: $(TARGET)
	$(TARGET).exe

clean-win: clean

mingw32-makerun: run-win
