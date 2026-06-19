# ── sysmon Makefile ──────────────────────────────────────────────────────────
#
#  Targets:
#    make           Build release binary  → build/sysmon
#    make debug     Build with debug info → build/sysmon_debug
#    make clean     Remove build artefacts
#    make install   Copy binary to /usr/local/bin (needs sudo)
#    make uninstall Remove installed binary

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
INCDIR   := include
SRCDIR   := src
BUILDDIR := build

SRCS     := $(wildcard $(SRCDIR)/*.cpp)
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
TARGET   := $(BUILDDIR)/sysmon
DEBUG_T  := $(BUILDDIR)/sysmon_debug

INSTALL_DIR := /usr/local/bin

.PHONY: all debug clean install uninstall

# ── Release ───────────────────────────────────────────────────────────────────
all: CXXFLAGS += -O2 -DNDEBUG
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "  Built: $@"

# ── Debug ─────────────────────────────────────────────────────────────────────
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(OBJS) | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $(DEBUG_T) $(OBJS)
	@echo "  Built: $(DEBUG_T)"

# ── Compile objects ───────────────────────────────────────────────────────────
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $@

# ── House-keeping ─────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILDDIR)
	@echo "  Cleaned."

install: all
	install -m 755 $(TARGET) $(INSTALL_DIR)/sysmon
	@echo "  Installed to $(INSTALL_DIR)/sysmon"

uninstall:
	rm -f $(INSTALL_DIR)/sysmon
	@echo "  Removed $(INSTALL_DIR)/sysmon"
