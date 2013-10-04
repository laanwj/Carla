#!/usr/bin/make -f
# Makefile for juce_audio_formats #
# ------------------------------- #
# Created by falkTX
#

include ../../Makefile.mk

# --------------------------------------------------------------

BUILD_CXX_FLAGS += -I.

ifeq ($(MACOS),true)
BUILD_CXX_FLAGS += -objc++
OBJS         = juce_audio_formats.mm.o
OBJS_posix32 = juce_audio_formats.mm.posix32.o
OBJS_posix64 = juce_audio_formats.mm.posix64.o
else
OBJS         = juce_audio_formats.cpp.o
OBJS_posix32 = juce_audio_formats.cpp.posix32.o
OBJS_posix64 = juce_audio_formats.cpp.posix64.o
endif

OBJS_win32 = juce_audio_formats.cpp.win32.o
OBJS_win64 = juce_audio_formats.cpp.win64.o

# --------------------------------------------------------------

all: ../juce_audio_formats.a

posix32: ../juce_audio_formats.posix32.a
posix64: ../juce_audio_formats.posix64.a
win32:   ../juce_audio_formats.win32.a
win64:   ../juce_audio_formats.win64.a

# --------------------------------------------------------------

../juce_audio_formats.a: $(OBJS)
	$(AR) rs $@ $^

../juce_audio_formats.posix32.a: $(OBJS_posix32)
	$(AR) rs $@ $^

../juce_audio_formats.posix64.a: $(OBJS_posix64)
	$(AR) rs $@ $^

../juce_audio_formats.win32.a: $(OBJS_win32)
	$(AR) rs $@ $^

../juce_audio_formats.win64.a: $(OBJS_win64)
	$(AR) rs $@ $^

../juce_audio_formats.dll: $(OBJS)
	$(CXX) $^ -shared $(LINK_FLAGS) -o $@

../juce_audio_formats.dylib: $(OBJS)
	$(CXX) $^ -dynamiclib $(LINK_FLAGS) -o $@

../juce_audio_formats.so: $(OBJS)
	$(CXX) $^ -shared $(LINK_FLAGS) -o $@

# --------------------------------------------------------------

%.cpp.o: %.cpp
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

%.mm.o: %.mm
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

%.posix32.o: %
	$(CXX) $< $(BUILD_CXX_FLAGS) $(32BIT_FLAGS) -c -o $@

%.posix64.o: %
	$(CXX) $< $(BUILD_CXX_FLAGS) $(64BIT_FLAGS) -c -o $@

%.win32.o: %
	$(CXX) $< $(BUILD_CXX_FLAGS) $(32BIT_FLAGS) -c -o $@

%.win64.o: %
	$(CXX) $< $(BUILD_CXX_FLAGS) $(64BIT_FLAGS) -c -o $@

# --------------------------------------------------------------

clean:
	rm -f *.o ../juce_audio_formats*.a

debug:
	$(MAKE) DEBUG=true