TEAM_ID     := 46B44K32TC
DYLIB       := DragonArena.dylib
SOURCES     := Sources/main.mm
SDK         := $(shell xcrun --sdk iphoneos --show-sdk-path)
CLANG       := $(shell xcrun -f clang++)
MIN_IOS     := 14.0
ARCH        := arm64

CXXFLAGS := \
    -arch $(ARCH) \
    -isysroot $(SDK) \
    -mios-version-min=$(MIN_IOS) \
    -std=c++17 \
    -ObjC++ \
    -fvisibility=hidden \
    -O2 \
    -I Sources \
    -I Sources/imgui \
    -DTARGET_OS_IPHONE=1

IMGUI_SOURCES := \
    Sources/imgui/imgui.cpp \
    Sources/imgui/imgui_draw.cpp \
    Sources/imgui/imgui_tables.cpp \
    Sources/imgui/imgui_widgets.cpp \
    Sources/imgui/backends/imgui_impl_metal.mm

LDFLAGS := \
    -dynamiclib \
    -arch $(ARCH) \
    -isysroot $(SDK) \
    -mios-version-min=$(MIN_IOS) \
    -framework Foundation \
    -framework Metal \
    -framework MetalKit \
    -framework UIKit \
    -framework CoreGraphics \
    -install_name @rpath/$(DYLIB)

all: $(DYLIB)

$(DYLIB): $(SOURCES) $(IMGUI_SOURCES)
	$(CLANG) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Built $(DYLIB)"

sign: $(DYLIB)
	codesign -f -s "$(TEAM_ID)" --entitlements entitlements.plist $(DYLIB)

clean:
	rm -f $(DYLIB)

.PHONY: all sign clean
