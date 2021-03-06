#!/usr/bin/make -f
# Makefile for carla-engine #
# ------------------------- #
# Created by falkTX
#

include ../Makefile.mk

# --------------------------------------------------------------

OBJS = \
	CarlaEngine.cpp.o \
	CarlaEngineClient.cpp.o \
	CarlaEngineData.cpp.o \
	CarlaEngineGraph.cpp.o \
	CarlaEngineInternal.cpp.o \
	CarlaEngineOsc.cpp.o \
	CarlaEngineOscSend.cpp.o \
	CarlaEnginePorts.cpp.o \
	CarlaEngineThread.cpp.o

OBJSa = $(OBJS) \
	CarlaEngineJack.cpp.o

ifeq ($(MACOS_OR_WIN32),true)
OBJSa += \
	CarlaEngineJuce.cpp.o
else
OBJSa += \
	CarlaEngineRtAudio.cpp.o
endif

ifneq ($(WIN32),true)
OBJSa += \
	CarlaEngineNative.cpp.o

OBJSp  = $(OBJS) \
	CarlaEngineNative.cpp.exp.o
endif

TARGETS = \
	../carla_engine.a \
	../carla_engine_plugin.a

# --------------------------------------------------------------

all: $(TARGETS)

# --------------------------------------------------------------

clean:
	$(RM) $(OBJSa) $(OBJSp)
	$(RM) $(TARGETS)

debug:
	$(MAKE) DEBUG=true

# --------------------------------------------------------------

../carla_engine.a: $(OBJSa)
	$(RM) $@
	$(AR) crs $@ $^

../carla_engine_plugin.a: $(OBJSp)
	$(RM) $@
	$(AR) crs $@ $^

# --------------------------------------------------------------

CarlaEngine.cpp.o: CarlaEngine.cpp $(CARLA_ENGINE_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineClient.cpp.o: CarlaEngineClient.cpp $(CARLA_ENGINE_CLIENT_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineData.cpp.o: CarlaEngineData.cpp $(CARLA_ENGINE_DATA_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineGraph.cpp.o: CarlaEngineGraph.cpp $(CARLA_ENGINE_GRAPH_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineInternal.cpp.o: CarlaEngineInternal.cpp $(CARLA_ENGINE_INTERNAL_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineOsc.cpp.o: CarlaEngineOsc.cpp $(CARLA_ENGINE_OSC_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineOscSend.cpp.o: CarlaEngineOscSend.cpp $(CARLA_ENGINE_OSC_SEND_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEnginePorts.cpp.o: CarlaEnginePorts.cpp $(CARLA_ENGINE_PORTS_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineThread.cpp.o: CarlaEngineThread.cpp $(CARLA_ENGINE_THREAD_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineJack.cpp.o: CarlaEngineJack.cpp $(CARLA_ENGINE_JACK_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineRtAudio.cpp.o: CarlaEngineRtAudio.cpp $(CARLA_ENGINE_RTAUDIO_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) $(RTAUDIO_FLAGS) $(RTMIDI_FLAGS) -c -o $@

CarlaEngineJuce.cpp.o: CarlaEngineJuce.cpp $(CARLA_ENGINE_JUCE_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineNative.cpp.o: CarlaEngineNative.cpp $(CARLA_ENGINE_NATIVE_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@

CarlaEngineNative.cpp.exp.o: CarlaEngineNative.cpp $(CARLA_ENGINE_NATIVE_CPP_DEPS)
	$(CXX) $< $(BUILD_CXX_FLAGS) -DCARLA_PLUGIN_EXPORT -c -o $@

# --------------------------------------------------------------
