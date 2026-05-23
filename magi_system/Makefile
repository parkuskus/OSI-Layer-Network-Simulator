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
    layer2/switch.cpp \
    layer3/ipv4.cpp \
    layer3/icmp.cpp \
    layer3/acl.cpp \
    layer3/nat.cpp \
    layer3/rip.cpp \
    layer4/tcp.cpp \
    layer4/udp.cpp \
    layer4/tcp_socket.cpp \
    layer7/magi_socket.cpp \
    layer7/udp_socket.cpp \
    layer7/dhcp_server.cpp \
    layer7/dhcp_client.cpp \
    layer7/dns_server.cpp \
    layer7/dns_client.cpp \
    layer7/http_server.cpp \
    layer7/http_client.cpp

# Build artifacts
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
TEST_TARGET := unit_tests.exe
TEST_SOURCES := \
    test/test_main.cpp \
    $(wildcard test/milestone-*/test_*.cpp) \
    cli.cpp \
    core/packet.cpp \
    core/interface.cpp \
    core/link.cpp \
    core/node.cpp \
    layer2/ethernet.cpp \
    layer2/arp.cpp \
    layer2/host.cpp \
    layer2/switch.cpp \
    layer3/ipv4.cpp \
    layer3/icmp.cpp \
    layer3/acl.cpp \
    layer3/nat.cpp \
    layer3/rip.cpp \
    layer4/tcp.cpp \
    layer4/udp.cpp \
    layer4/tcp_socket.cpp \
    layer7/magi_socket.cpp \
    layer7/udp_socket.cpp \
    layer7/dhcp_server.cpp \
    layer7/dhcp_client.cpp \
    layer7/dns_server.cpp \
    layer7/dns_client.cpp \
    layer7/http_server.cpp \
    layer7/http_client.cpp

.PHONY: all run makerun clean rebuild debug release run-win clean-win mingw32-makerun test

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile with auto-generated dependency files

# Use platform-specific recipe so `make` works both on Windows (cmd) and POSIX shells
ifeq ($(OS),Windows_NT)
$(BUILD_DIR)/%.o: %.cpp ; cmd /c if not exist "$(dir $@)" mkdir "$(dir $@)" && $(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

 

else
$(BUILD_DIR)/%.o: %.cpp ; mkdir -p $(dir $@) && $(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

 

endif

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
ifeq ($(OS),Windows_NT)
clean: ; powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TARGET)' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TARGET).exe' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TEST_TARGET)' -ErrorAction SilentlyContinue"
else
clean: ; rm -rf $(BUILD_DIR) ; rm -f $(TARGET) $(TARGET).exe $(TEST_TARGET)
endif

rebuild: clean all

$(TEST_TARGET): $(TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) -o $@ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Windows-friendly aliases
run-win: $(TARGET)
	$(TARGET).exe

clean-win: clean

mingw32-makerun: run-win
