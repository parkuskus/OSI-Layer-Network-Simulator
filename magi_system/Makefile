# Compiler and build settings
CXX := g++
STD := -std=c++11
WARN := -Wall -Wextra
INCLUDES := -I.
CPPFLAGS := $(INCLUDES)
CXXFLAGS := $(STD) $(WARN)
LDFLAGS :=

# Targets and directories
TARGET_NAME := magi_system
WEB_TARGET_NAME := magi_web
BUILD_DIR := build
PORT ?= 8080

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
RUN_PREFIX :=
LDFLAGS += -lws2_32
else
EXE_EXT :=
RUN_PREFIX := ./
endif

TARGET := $(TARGET_NAME)$(EXE_EXT)
WEB_TARGET := $(WEB_TARGET_NAME)$(EXE_EXT)

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
WEB_SOURCES := $(filter-out main.cpp,$(SOURCES)) web_main.cpp web/server.cpp
WEB_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(WEB_SOURCES))
DEPS += $(WEB_OBJECTS:.o=.d)
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

.PHONY: all run web run-web makerun clean rebuild debug release run-win clean-win mingw32-makerun test

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(WEB_TARGET): $(WEB_OBJECTS)
	$(CXX) $(WEB_OBJECTS) -o $@ $(LDFLAGS)

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
	$(RUN_PREFIX)$(TARGET)

web: $(WEB_TARGET)

run-web: $(WEB_TARGET)
	$(RUN_PREFIX)$(WEB_TARGET) $(PORT)

 

# Convenience alias used in README
makerun: run

# Build modes
debug: CXXFLAGS += -O0 -g
debug: rebuild

release: CXXFLAGS += -O2
release: rebuild

# Cleanup
ifeq ($(OS),Windows_NT)
clean: ; powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TARGET_NAME)' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TARGET_NAME).exe' -ErrorAction SilentlyContinue; Remove-Item -Force '$(WEB_TARGET_NAME)' -ErrorAction SilentlyContinue; Remove-Item -Force '$(WEB_TARGET_NAME).exe' -ErrorAction SilentlyContinue; Remove-Item -Force '$(TEST_TARGET)' -ErrorAction SilentlyContinue"
else
clean: ; rm -rf $(BUILD_DIR) ; rm -f $(TARGET_NAME) $(TARGET_NAME).exe $(WEB_TARGET_NAME) $(WEB_TARGET_NAME).exe $(TEST_TARGET)
endif

rebuild: clean all

$(TEST_TARGET): $(TEST_SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_SOURCES) -o $@ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Windows-friendly aliases
run-win: $(TARGET)
	$(TARGET)

clean-win: clean

mingw32-makerun: run-win
