﻿/*
 * Carla LinuxSampler Plugin
 * Copyright (C) 2011-2014 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 */

/* TODO
 * - implement buffer size changes
 * - implement sample rate changes
 * - call outDev->ReconnectAll() after changing buffer size or sample rate
 * - use CARLA_SAFE_ASSERT_RETURN with err
 */
#include "CarlaPluginInternal.hpp"
#include "CarlaEngine.hpp"

#ifdef HAVE_LINUXSAMPLER

#include "CarlaBackendUtils.hpp"
#include "CarlaMathUtils.hpp"

#include "juce_core.h"

#include "linuxsampler/EngineFactory.h"
#include <linuxsampler/Sampler.h>

// -----------------------------------------------------------------------

namespace LinuxSampler {

using CarlaBackend::CarlaEngine;
using CarlaBackend::CarlaPlugin;

// -----------------------------------------------------------------------
// LinuxSampler static values

static const float kVolumeMax = 3.16227766f; // +10 dB

// -----------------------------------------------------------------------
// LinuxSampler AudioOutputDevice Plugin

class AudioOutputDevicePlugin : public AudioOutputDevice
{
public:
    AudioOutputDevicePlugin(const CarlaEngine* const engine, const CarlaPlugin* const plugin, const bool uses16Outs)
        : AudioOutputDevice(std::map<std::string, DeviceCreationParameter*>()),
          kEngine(engine),
          kPlugin(plugin),
          leakDetector_AudioOutputDevicePlugin()
    {
        CARLA_ASSERT(engine != nullptr);
        CARLA_ASSERT(plugin != nullptr);

        AcquireChannels(uses16Outs ? 32 : 2);
    }

    ~AudioOutputDevicePlugin() override {}

    // -------------------------------------------------------------------
    // LinuxSampler virtual methods

    void Play() override {}
    void Stop() override {}

    bool IsPlaying() override
    {
        return (kEngine->isRunning() && kPlugin->isEnabled());
    }

    uint MaxSamplesPerCycle() override
    {
        return kEngine->getBufferSize();
    }

    uint SampleRate() override
    {
        return uint(kEngine->getSampleRate());
    }

    std::string Driver() override
    {
        return "AudioOutputDevicePlugin";
    }

    AudioChannel* CreateChannel(uint channelNr) override
    {
        return new AudioChannel(channelNr, nullptr, 0);
    }

    // -------------------------------------------------------------------

           bool isAutonomousDevice() override { return false; }
    static bool isAutonomousDriver()          { return false; }

    // -------------------------------------------------------------------
    // Give public access to the RenderAudio call

    int Render(const uint samples)
    {
        return RenderAudio(samples);
    }

    // -------------------------------------------------------------------

private:
    const CarlaEngine* const kEngine;
    const CarlaPlugin* const kPlugin;

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioOutputDevicePlugin)
};

// -----------------------------------------------------------------------
// LinuxSampler MidiInputPort Plugin

class MidiInputPortPlugin : public MidiInputPort
{
public:
    MidiInputPortPlugin(MidiInputDevice* const device, const int portNum)
        : MidiInputPort(device, portNum),
          leakDetector_MidiInputPortPlugin() {}

    ~MidiInputPortPlugin() override {}

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiInputPortPlugin)
};

// -----------------------------------------------------------------------
// LinuxSampler MidiInputDevice Plugin

class MidiInputDevicePlugin : public MidiInputDevice
{
public:
    MidiInputDevicePlugin(Sampler* const sampler)
        : MidiInputDevice(std::map<std::string, DeviceCreationParameter*>(), sampler),
          leakDetector_MidiInputDevicePlugin() {}

    // -------------------------------------------------------------------
    // LinuxSampler virtual methods

    void Listen()     override {}
    void StopListen() override {}

    std::string Driver() override
    {
        return "MidiInputDevicePlugin";
    }

    MidiInputPort* CreateMidiPort() override
    {
        return new MidiInputPortPlugin(this, int(Ports.size()));
    }

    // -------------------------------------------------------------------

           bool isAutonomousDevice() override { return false; }
    static bool isAutonomousDriver()          { return false; }

    // -------------------------------------------------------------------

    MidiInputPortPlugin* CreateMidiPortPlugin()
    {
        return new MidiInputPortPlugin(this, int(Ports.size()));
    }

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiInputDevicePlugin)
};

// -----------------------------------------------------------------------
// SamplerPlugin

struct SamplerPlugin {
    Sampler sampler;
    MidiInputDevicePlugin midiIn;

    SamplerPlugin()
        : sampler(),
          midiIn(&sampler) {}
};

} // namespace LinuxSampler

// -----------------------------------------------------------------------

using juce::File;
using juce::SharedResourcePointer;
using juce::StringArray;

CARLA_BACKEND_START_NAMESPACE

// -----------------------------------------------------------------------

class LinuxSamplerPlugin : public CarlaPlugin
{
public:
    LinuxSamplerPlugin(CarlaEngine* const engine, const uint id, const bool isGIG, const bool use16Outs)
        : CarlaPlugin(engine, id),
          kIsGIG(isGIG),
          kUses16Outs(use16Outs && isGIG),
          kMaxChannels(isGIG ? MAX_MIDI_CHANNELS : 1),
          fLabel(nullptr),
          fMaker(nullptr),
          fRealName(nullptr),
          fAudioOutputDevice(nullptr),
          fMidiInputPort(nullptr),
          fInstrument(nullptr),
          fInstrumentIds(),
          sSampler(),
          leakDetector_LinuxSamplerPlugin()
    {
        carla_debug("LinuxSamplerPlugin::LinuxSamplerPlugin(%p, %i, %s, %s)", engine, id, bool2str(isGIG), bool2str(use16Outs));

        carla_zeroStruct(fCurProgs,        MAX_MIDI_CHANNELS);
        carla_zeroStruct(fEngineChannels,  MAX_MIDI_CHANNELS);
        carla_zeroStruct(fSamplerChannels, MAX_MIDI_CHANNELS);

        if (use16Outs && ! isGIG)
            carla_stderr("Tried to use SFZ with 16 stereo outs, this doesn't make much sense so single stereo mode will be used instead");
    }

    ~LinuxSamplerPlugin() override
    {
        carla_debug("LinuxSamplerPlugin::~LinuxSamplerPlugin()");

        pData->singleMutex.lock();
        pData->masterMutex.lock();

        if (pData->client != nullptr && pData->client->isActive())
            pData->client->deactivate();

        if (pData->active)
        {
            deactivate();
            pData->active = false;
        }

        if (fMidiInputPort != nullptr)
        {
            for (uint i=0; i<kMaxChannels; ++i)
            {
                if (fSamplerChannels[i] != nullptr)
                {
                    if (fEngineChannels[i] != nullptr)
                    {
                        fMidiInputPort->Disconnect(fEngineChannels[i]);
                        fEngineChannels[i]->DisconnectAudioOutputDevice();
                        fEngineChannels[i] = nullptr;
                    }

                    sSampler->sampler.RemoveSamplerChannel(fSamplerChannels[i]);
                    fSamplerChannels[i] = nullptr;
                }
            }

            delete fMidiInputPort;
            fMidiInputPort = nullptr;
        }

        if (fAudioOutputDevice != nullptr)
        {
            delete fAudioOutputDevice;
            fAudioOutputDevice = nullptr;
        }

        fInstrument = nullptr;
        fInstrumentIds.clear();

        if (fLabel != nullptr)
        {
            delete[] fLabel;
            fLabel = nullptr;
        }

        if (fMaker != nullptr)
        {
            delete[] fMaker;
            fMaker = nullptr;
        }

        if (fRealName != nullptr)
        {
            delete[] fRealName;
            fRealName = nullptr;
        }

        clearBuffers();
    }

    // -------------------------------------------------------------------
    // Information (base)

    PluginType getType() const noexcept override
    {
        return kIsGIG ? PLUGIN_GIG : PLUGIN_SFZ;
    }

    PluginCategory getCategory() const noexcept override
    {
        return PLUGIN_CATEGORY_SYNTH;
    }

    // -------------------------------------------------------------------
    // Information (count)

    // nothing

    // -------------------------------------------------------------------
    // Information (current data)

    // nothing

    // -------------------------------------------------------------------
    // Information (per-plugin data)

    uint getOptionsAvailable() const noexcept override
    {
        uint options = 0x0;

        options |= PLUGIN_OPTION_MAP_PROGRAM_CHANGES;
        options |= PLUGIN_OPTION_SEND_CONTROL_CHANGES;
        options |= PLUGIN_OPTION_SEND_CHANNEL_PRESSURE;
        options |= PLUGIN_OPTION_SEND_PITCHBEND;
        options |= PLUGIN_OPTION_SEND_ALL_SOUND_OFF;

        return options;
    }

    void getLabel(char* const strBuf) const noexcept override
    {
        if (fLabel != nullptr)
        {
            std::strncpy(strBuf, fLabel, STR_MAX);
            return;
        }

        CarlaPlugin::getLabel(strBuf);
    }

    void getMaker(char* const strBuf) const noexcept override
    {
        if (fMaker != nullptr)
        {
            std::strncpy(strBuf, fMaker, STR_MAX);
            return;
        }

        CarlaPlugin::getMaker(strBuf);
    }

    void getCopyright(char* const strBuf) const noexcept override
    {
        getMaker(strBuf);
    }

    void getRealName(char* const strBuf) const noexcept override
    {
        if (fRealName != nullptr)
        {
            std::strncpy(strBuf, fRealName, STR_MAX);
            return;
        }

        CarlaPlugin::getRealName(strBuf);
    }

    // -------------------------------------------------------------------
    // Set data (state)

    void prepareForSave() override
    {
        if (kMaxChannels > 1 && fInstrumentIds.size() > 1)
        {
            char strBuf[STR_MAX+1];
            std::snprintf(strBuf, STR_MAX, "%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i:%i", fCurProgs[0],  fCurProgs[1],  fCurProgs[2],  fCurProgs[3],
                                                                                              fCurProgs[4],  fCurProgs[5],  fCurProgs[6],  fCurProgs[7],
                                                                                              fCurProgs[8],  fCurProgs[9],  fCurProgs[10], fCurProgs[11],
                                                                                              fCurProgs[12], fCurProgs[13], fCurProgs[14], fCurProgs[15]);

            CarlaPlugin::setCustomData(CUSTOM_DATA_TYPE_STRING, "programs", strBuf, false);
        }
    }

    // -------------------------------------------------------------------
    // Set data (internal stuff)

    void setCtrlChannel(const int8_t channel, const bool sendOsc, const bool sendCallback) noexcept override
    {
        if (channel >= 0 && channel < MAX_MIDI_CHANNELS)
            pData->prog.current = fCurProgs[channel];

        CarlaPlugin::setCtrlChannel(channel, sendOsc, sendCallback);
    }

    // -------------------------------------------------------------------
    // Set data (plugin-specific stuff)

    void setCustomData(const char* const type, const char* const key, const char* const value, const bool sendGui) override
    {
        CARLA_SAFE_ASSERT_RETURN(type != nullptr && type[0] != '\0',);
        CARLA_SAFE_ASSERT_RETURN(key != nullptr && key[0] != '\0',);
        CARLA_SAFE_ASSERT_RETURN(value != nullptr && value[0] != '\0',);
        carla_debug("LinuxSamplerPlugin::setCustomData(%s, \"%s\", \"%s\", %s)", type, key, value, bool2str(sendGui));

        if (std::strcmp(type, CUSTOM_DATA_TYPE_STRING) != 0)
            return carla_stderr2("LinuxSamplerPlugin::setCustomData(\"%s\", \"%s\", \"%s\", %s) - type is not string", type, key, value, bool2str(sendGui));

        if (std::strcmp(key, "programs") != 0)
            return carla_stderr2("LinuxSamplerPlugin::setCustomData(\"%s\", \"%s\", \"%s\", %s) - type is not string", type, key, value, bool2str(sendGui));

        if (kMaxChannels > 1 && fInstrumentIds.size() > 1)
        {
            StringArray programList(StringArray::fromTokens(value, ":", ""));

            if (programList.size() == MAX_MIDI_CHANNELS)
            {
                uint8_t channel = 0;
                for (juce::String *it=programList.begin(), *end=programList.end(); it != end; ++it)
                {
                    const int index(it->getIntValue());

                    if (index >= 0 && index < static_cast<int>(pData->prog.count))
                    {
                        LinuxSampler::EngineChannel* const engineChannel(fEngineChannels[channel]);
                        CARLA_SAFE_ASSERT_CONTINUE(engineChannel != nullptr);

                        const uint32_t uindex(static_cast<uint32_t>(index));

                        if (pData->engine->isOffline())
                        {
                            try {
                                engineChannel->PrepareLoadInstrument(pData->filename, uindex);
                                engineChannel->LoadInstrument();
                            } CARLA_SAFE_EXCEPTION("LoadInstrument");
                        }
                        else
                        {
                            try {
                                fInstrument->LoadInstrumentInBackground(fInstrumentIds[uindex], engineChannel);
                            } CARLA_SAFE_EXCEPTION("LoadInstrumentInBackground");
                        }

                        fCurProgs[channel] = index;

                        if (pData->ctrlChannel == static_cast<int32_t>(channel))
                        {
                            pData->prog.current = index;
                            pData->engine->callback(ENGINE_CALLBACK_PROGRAM_CHANGED, pData->id, index, 0, 0.0f, nullptr);
                        }
                    }

                    if (++channel >= MAX_MIDI_CHANNELS)
                        break;
                }
                CARLA_SAFE_ASSERT(channel == MAX_MIDI_CHANNELS);
            }
        }

        CarlaPlugin::setCustomData(type, key, value, sendGui);
    }

    void setProgram(const int32_t index, const bool sendGui, const bool sendOsc, const bool sendCallback) noexcept override
    {
        CARLA_SAFE_ASSERT_RETURN(index >= -1 && index < static_cast<int32_t>(pData->prog.count),);

        const int8_t channel(kIsGIG ? pData->ctrlChannel : int8_t(0));

        if (index >= 0 && channel >= 0 && channel < MAX_MIDI_CHANNELS)
        {
            LinuxSampler::EngineChannel* const engineChannel(fEngineChannels[channel]);
            CARLA_SAFE_ASSERT_RETURN(engineChannel != nullptr,);

            const uint32_t uindex(static_cast<uint32_t>(index));

            const ScopedSingleProcessLocker spl(this, (sendGui || sendOsc || sendCallback));

            if (pData->engine->isOffline())
            {
                try {
                    engineChannel->PrepareLoadInstrument(pData->filename, uindex);
                    engineChannel->LoadInstrument();
                } CARLA_SAFE_EXCEPTION("LoadInstrument");
            }
            else
            {
                try {
                    fInstrument->LoadInstrumentInBackground(fInstrumentIds[uindex], engineChannel);
                } CARLA_SAFE_EXCEPTION("LoadInstrumentInBackground");
            }

            fCurProgs[channel] = index;
        }

        CarlaPlugin::setProgram(index, sendGui, sendOsc, sendCallback);
    }

    // -------------------------------------------------------------------
    // Set ui stuff

    // nothing

    // -------------------------------------------------------------------
    // Plugin state

    void reload() override
    {
        CARLA_SAFE_ASSERT_RETURN(pData->engine != nullptr,);
        CARLA_SAFE_ASSERT_RETURN(fInstrument != nullptr,);
        carla_debug("LinuxSamplerPlugin::reload() - start");

        const EngineProcessMode processMode(pData->engine->getProccessMode());

        // Safely disable plugin for reload
        const ScopedDisabler sd(this);

        if (pData->active)
            deactivate();

        clearBuffers();

        uint32_t aOuts;
        aOuts = kUses16Outs ? 32 : 2;

        pData->audioOut.createNew(aOuts);

        const uint portNameSize(pData->engine->getMaxPortNameSize());
        CarlaString portName;

        // ---------------------------------------
        // Audio Outputs

        if (kUses16Outs)
        {
            for (uint32_t i=0; i < 32; ++i)
            {
                portName.clear();

                if (processMode == ENGINE_PROCESS_MODE_SINGLE_CLIENT)
                {
                    portName  = pData->name;
                    portName += ":";
                }

                portName += "out-";

                if ((i+2)/2 < 9)
                    portName += "0";

                portName += CarlaString((i+2)/2);

                if (i % 2 == 0)
                    portName += "L";
                else
                    portName += "R";

                portName.truncate(portNameSize);

                pData->audioOut.ports[i].port   = (CarlaEngineAudioPort*)pData->client->addPort(kEnginePortTypeAudio, portName, false);
                pData->audioOut.ports[i].rindex = i;
            }
        }
        else
        {
            // out-left
            portName.clear();

            if (processMode == ENGINE_PROCESS_MODE_SINGLE_CLIENT)
            {
                portName  = pData->name;
                portName += ":";
            }

            portName += "out-left";
            portName.truncate(portNameSize);

            pData->audioOut.ports[0].port   = (CarlaEngineAudioPort*)pData->client->addPort(kEnginePortTypeAudio, portName, false);
            pData->audioOut.ports[0].rindex = 0;

            // out-right
            portName.clear();

            if (processMode == ENGINE_PROCESS_MODE_SINGLE_CLIENT)
            {
                portName  = pData->name;
                portName += ":";
            }

            portName += "out-right";
            portName.truncate(portNameSize);

            pData->audioOut.ports[1].port   = (CarlaEngineAudioPort*)pData->client->addPort(kEnginePortTypeAudio, portName, false);
            pData->audioOut.ports[1].rindex = 1;
        }

        // ---------------------------------------
        // Event Input

        {
            portName.clear();

            if (processMode == ENGINE_PROCESS_MODE_SINGLE_CLIENT)
            {
                portName  = pData->name;
                portName += ":";
            }

            portName += "events-in";
            portName.truncate(portNameSize);

            pData->event.portIn = (CarlaEngineEventPort*)pData->client->addPort(kEnginePortTypeEvent, portName, true);
        }

        // ---------------------------------------

        // plugin hints
        pData->hints  = 0x0;
        pData->hints |= PLUGIN_IS_SYNTH;
        pData->hints |= PLUGIN_CAN_VOLUME;

        if (! kUses16Outs)
            pData->hints |= PLUGIN_CAN_BALANCE;

        // extra plugin hints
        pData->extraHints  = 0x0;
        pData->extraHints |= PLUGIN_EXTRA_HINT_HAS_MIDI_IN;

        if (! kUses16Outs)
            pData->extraHints |= PLUGIN_EXTRA_HINT_CAN_RUN_RACK;

        if (fInstrumentIds.size() > 1)
            pData->extraHints |= PLUGIN_EXTRA_HINT_USES_MULTI_PROGS;

        bufferSizeChanged(pData->engine->getBufferSize());
        reloadPrograms(true);

        if (pData->active)
            activate();

        carla_debug("LinuxSamplerPlugin::reload() - end");
    }

    void reloadPrograms(bool doInit) override
    {
        carla_debug("LinuxSamplerPlugin::reloadPrograms(%s)", bool2str(doInit));

        // Delete old programs
        pData->prog.clear();

        // Query new programs
        const uint32_t count(static_cast<uint32_t>(fInstrumentIds.size()));

        // sound kits must always have at least 1 midi-program
        CARLA_SAFE_ASSERT_RETURN(count > 0,);

        pData->prog.createNew(count);

        // Update data
        LinuxSampler::InstrumentManager::instrument_info_t info;

        for (uint32_t i=0; i < pData->prog.count; ++i)
        {
            try {
                info = fInstrument->GetInstrumentInfo(fInstrumentIds[i]);
            } CARLA_SAFE_EXCEPTION_CONTINUE("GetInstrumentInfo");

            pData->prog.names[i] = carla_strdup_safe(info.InstrumentName.c_str());
        }

#ifndef BUILD_BRIDGE
        // Update OSC Names
        if (pData->engine->isOscControlRegistered())
        {
            pData->engine->oscSend_control_set_program_count(pData->id, count);

            for (uint32_t i=0; i < count; ++i)
                pData->engine->oscSend_control_set_program_name(pData->id, i, pData->prog.names[i]);
        }
#endif

        if (doInit)
        {
            for (uint i=0; i<kMaxChannels; ++i)
            {
                LinuxSampler::EngineChannel* const engineChannel(fEngineChannels[i]);
                CARLA_SAFE_ASSERT_CONTINUE(engineChannel != nullptr);

                try {
                    fInstrument->LoadInstrumentInBackground(fInstrumentIds[0], engineChannel);
                } CARLA_SAFE_EXCEPTION("LoadInstrumentInBackground");

                fCurProgs[i] = 0;
            }

            pData->prog.current = 0;
        }
        else
        {
            pData->engine->callback(ENGINE_CALLBACK_RELOAD_PROGRAMS, pData->id, 0, 0, 0.0f, nullptr);
        }
    }

    // -------------------------------------------------------------------
    // Plugin processing

#if 0
    void activate() override
    {
        for (int i=0; i < MAX_MIDI_CHANNELS; ++i)
        {
            if (fAudioOutputDevices[i] != nullptr)
                fAudioOutputDevices[i]->Play();
        }
    }

    void deactivate() override
    {
        for (int i=0; i < MAX_MIDI_CHANNELS; ++i)
        {
            if (fAudioOutputDevices[i] != nullptr)
                fAudioOutputDevices[i]->Stop();
        }
    }
#endif

    void process(const float** const, float** const audioOut, const float** const, float** const, const uint32_t frames) override
    {
        // --------------------------------------------------------------------------------------------------------
        // Check if active

        if (! pData->active)
        {
            // disable any output sound
            for (uint32_t i=0; i < pData->audioOut.count; ++i)
                FloatVectorOperations::clear(audioOut[i], static_cast<int>(frames));
            return;
        }

        // --------------------------------------------------------------------------------------------------------
        // Check if needs reset

        if (pData->needsReset)
        {
            if (pData->options & PLUGIN_OPTION_SEND_ALL_SOUND_OFF)
            {
                for (uint i=0; i < MAX_MIDI_CHANNELS; ++i)
                {
                    fMidiInputPort->DispatchControlChange(MIDI_CONTROL_ALL_NOTES_OFF, 0, i);
                    fMidiInputPort->DispatchControlChange(MIDI_CONTROL_ALL_SOUND_OFF, 0, i);
                }
            }
            else if (pData->ctrlChannel >= 0 && pData->ctrlChannel < MAX_MIDI_CHANNELS)
            {
                for (uint8_t i=0; i < MAX_MIDI_NOTE; ++i)
                    fMidiInputPort->DispatchNoteOff(i, 0, uint(pData->ctrlChannel));
            }

            pData->needsReset = false;
        }

        // --------------------------------------------------------------------------------------------------------
        // Event Input and Processing

        {
            // ----------------------------------------------------------------------------------------------------
            // MIDI Input (External)

            if (pData->extNotes.mutex.tryLock())
            {
                for (RtLinkedList<ExternalMidiNote>::Itenerator it = pData->extNotes.data.begin(); it.valid(); it.next())
                {
                    const ExternalMidiNote& note(it.getValue());

                    CARLA_SAFE_ASSERT_CONTINUE(note.channel >= 0 && note.channel < MAX_MIDI_CHANNELS);

                    if (note.velo > 0)
                        fMidiInputPort->DispatchNoteOn(note.note, note.velo, static_cast<uint>(note.channel));
                    else
                        fMidiInputPort->DispatchNoteOff(note.note, note.velo, static_cast<uint>(note.channel));
                }

                pData->extNotes.data.clear();
                pData->extNotes.mutex.unlock();

            } // End of MIDI Input (External)

            // ----------------------------------------------------------------------------------------------------
            // Event Input (System)

#ifndef BUILD_BRIDGE
            bool allNotesOffSent = false;
#endif
            bool sampleAccurate  = (pData->options & PLUGIN_OPTION_FIXED_BUFFERS) == 0;

            uint32_t startTime  = 0;
            uint32_t timeOffset = 0;

            for (uint32_t i=0, numEvents=pData->event.portIn->getEventCount(); i < numEvents; ++i)
            {
                const EngineEvent& event(pData->event.portIn->getEvent(i));

                CARLA_SAFE_ASSERT_CONTINUE(event.time < frames);
                CARLA_SAFE_ASSERT_BREAK(event.time >= timeOffset);

                if (event.time > timeOffset && sampleAccurate)
                {
                    if (processSingle(audioOut, event.time - timeOffset, timeOffset))
                    {
                        startTime  = 0;
                        timeOffset = event.time;
                    }
                    else
                        startTime += timeOffset;
                }

                // Control change
                switch (event.type)
                {
                case kEngineEventTypeNull:
                    break;

                case kEngineEventTypeControl:
                {
                    const EngineControlEvent& ctrlEvent = event.ctrl;

                    switch (ctrlEvent.type)
                    {
                    case kEngineControlEventTypeNull:
                        break;

                    case kEngineControlEventTypeParameter:
                    {
#ifndef BUILD_BRIDGE
                        // Control backend stuff
                        if (event.channel == pData->ctrlChannel)
                        {
                            float value;

                            if (MIDI_IS_CONTROL_BREATH_CONTROLLER(ctrlEvent.param) && (pData->hints & PLUGIN_CAN_DRYWET) != 0)
                            {
                                value = ctrlEvent.value;
                                setDryWet(value, false, false);
                                pData->postponeRtEvent(kPluginPostRtEventParameterChange, PARAMETER_DRYWET, 0, value);
                            }

                            if (MIDI_IS_CONTROL_CHANNEL_VOLUME(ctrlEvent.param) && (pData->hints & PLUGIN_CAN_VOLUME) != 0)
                            {
                                value = ctrlEvent.value*127.0f/100.0f;
                                setVolume(value, false, false);
                                pData->postponeRtEvent(kPluginPostRtEventParameterChange, PARAMETER_VOLUME, 0, value);
                            }

                            if (MIDI_IS_CONTROL_BALANCE(ctrlEvent.param) && (pData->hints & PLUGIN_CAN_BALANCE) != 0)
                            {
                                float left, right;
                                value = ctrlEvent.value/0.5f - 1.0f;

                                if (value < 0.0f)
                                {
                                    left  = -1.0f;
                                    right = (value*2.0f)+1.0f;
                                }
                                else if (value > 0.0f)
                                {
                                    left  = (value*2.0f)-1.0f;
                                    right = 1.0f;
                                }
                                else
                                {
                                    left  = -1.0f;
                                    right = 1.0f;
                                }

                                setBalanceLeft(left, false, false);
                                setBalanceRight(right, false, false);
                                pData->postponeRtEvent(kPluginPostRtEventParameterChange, PARAMETER_BALANCE_LEFT, 0, left);
                                pData->postponeRtEvent(kPluginPostRtEventParameterChange, PARAMETER_BALANCE_RIGHT, 0, right);
                            }
                        }
#endif
                        // Control plugin parameters
                        for (uint32_t k=0; k < pData->param.count; ++k)
                        {
                            if (pData->param.data[k].midiChannel != event.channel)
                                continue;
                            if (pData->param.data[k].midiCC != ctrlEvent.param)
                                continue;
                            if (pData->param.data[k].hints != PARAMETER_INPUT)
                                continue;
                            if ((pData->param.data[k].hints & PARAMETER_IS_AUTOMABLE) == 0)
                                continue;

                            float value;

                            if (pData->param.data[k].hints & PARAMETER_IS_BOOLEAN)
                            {
                                value = (ctrlEvent.value < 0.5f) ? pData->param.ranges[k].min : pData->param.ranges[k].max;
                            }
                            else
                            {
                                value = pData->param.ranges[k].getUnnormalizedValue(ctrlEvent.value);

                                if (pData->param.data[k].hints & PARAMETER_IS_INTEGER)
                                    value = std::rint(value);
                            }

                            setParameterValue(k, value, false, false, false);
                            pData->postponeRtEvent(kPluginPostRtEventParameterChange, static_cast<int32_t>(k), 0, value);
                        }

                        if ((pData->options & PLUGIN_OPTION_SEND_CONTROL_CHANGES) != 0 && ctrlEvent.param <= 0x5F)
                        {
                            fMidiInputPort->DispatchControlChange(uint8_t(ctrlEvent.param), uint8_t(ctrlEvent.value*127.0f), event.channel, static_cast<int32_t>(sampleAccurate ? startTime : event.time));
                        }

                        break;
                    }

                    case kEngineControlEventTypeMidiBank:
                        break;

                    case kEngineControlEventTypeMidiProgram:
                        if (event.channel < MAX_MIDI_CHANNELS && (pData->options & PLUGIN_OPTION_MAP_PROGRAM_CHANGES) != 0)
                        {
                            const uint32_t programId(ctrlEvent.param);
                            CARLA_SAFE_ASSERT_CONTINUE(programId < fInstrumentIds.size());

                            LinuxSampler::EngineChannel* const engineChannel(fEngineChannels[kIsGIG ? event.channel : 0]);
                            CARLA_SAFE_ASSERT_CONTINUE(engineChannel != nullptr);

                            if (pData->engine->isOffline())
                            {
                                try {
                                    engineChannel->PrepareLoadInstrument(pData->filename, programId);
                                    engineChannel->LoadInstrument();
                                } CARLA_SAFE_EXCEPTION("LoadInstrument");
                            }
                            else
                            {
                                try {
                                    fInstrument->LoadInstrumentInBackground(fInstrumentIds[programId], engineChannel);
                                } CARLA_SAFE_EXCEPTION("LoadInstrumentInBackground");
                            }

                            fCurProgs[event.channel] = static_cast<int32_t>(programId);

                            if (pData->ctrlChannel == event.channel)
                                pData->postponeRtEvent(kPluginPostRtEventProgramChange, static_cast<int32_t>(programId), 0, 0.0f);
                        }
                        break;

                    case kEngineControlEventTypeAllSoundOff:
                        if (pData->options & PLUGIN_OPTION_SEND_ALL_SOUND_OFF)
                        {
                            fMidiInputPort->DispatchControlChange(MIDI_CONTROL_ALL_SOUND_OFF, 0, event.channel, static_cast<int32_t>(sampleAccurate ? startTime : event.time));
                        }
                        break;

                    case kEngineControlEventTypeAllNotesOff:
                        if (pData->options & PLUGIN_OPTION_SEND_ALL_SOUND_OFF)
                        {
#ifndef BUILD_BRIDGE
                            if (event.channel == pData->ctrlChannel && ! allNotesOffSent)
                            {
                                allNotesOffSent = true;
                                sendMidiAllNotesOffToCallback();
                            }
#endif

                            fMidiInputPort->DispatchControlChange(MIDI_CONTROL_ALL_NOTES_OFF, 0, event.channel, static_cast<int32_t>(sampleAccurate ? startTime : event.time));
                        }
                        break;
                    }
                    break;
                }

                case kEngineEventTypeMidi: {
                    const EngineMidiEvent& midiEvent(event.midi);

                    const uint8_t* const midiData(midiEvent.size > EngineMidiEvent::kDataSize ? midiEvent.dataExt : midiEvent.data);

                    uint8_t status = uint8_t(MIDI_GET_STATUS_FROM_DATA(midiData));

                    if (status == MIDI_STATUS_CHANNEL_PRESSURE && (pData->options & PLUGIN_OPTION_SEND_CHANNEL_PRESSURE) == 0)
                        continue;
                    if (status == MIDI_STATUS_CONTROL_CHANGE && (pData->options & PLUGIN_OPTION_SEND_CONTROL_CHANGES) == 0)
                        continue;
                    if (status == MIDI_STATUS_POLYPHONIC_AFTERTOUCH && (pData->options & PLUGIN_OPTION_SEND_NOTE_AFTERTOUCH) == 0)
                        continue;
                    if (status == MIDI_STATUS_PITCH_WHEEL_CONTROL && (pData->options & PLUGIN_OPTION_SEND_PITCHBEND) == 0)
                        continue;

                    // Fix bad note-off
                    if (status == MIDI_STATUS_NOTE_ON && midiData[2] == 0)
                        status = MIDI_STATUS_NOTE_OFF;

                    // put back channel in data
                    uint8_t midiData2[midiEvent.size];
                    midiData2[0] = uint8_t(status | (event.channel & MIDI_CHANNEL_BIT));
                    std::memcpy(midiData2+1, midiData+1, static_cast<std::size_t>(midiEvent.size-1));

                    fMidiInputPort->DispatchRaw(midiData2, static_cast<int32_t>(sampleAccurate ? startTime : event.time));

                    if (status == MIDI_STATUS_NOTE_ON)
                        pData->postponeRtEvent(kPluginPostRtEventNoteOn, event.channel, midiData[1], midiData[2]);
                    else if (status == MIDI_STATUS_NOTE_OFF)
                        pData->postponeRtEvent(kPluginPostRtEventNoteOff, event.channel, midiData[1], 0.0f);
                } break;
                }
            }

            pData->postRtEvents.trySplice();

            if (frames > timeOffset)
                processSingle(audioOut, frames - timeOffset, timeOffset);

        } // End of Event Input and Processing
    }

    bool processSingle(float** const outBuffer, const uint32_t frames, const uint32_t timeOffset)
    {
        CARLA_SAFE_ASSERT_RETURN(outBuffer != nullptr, false);
        CARLA_SAFE_ASSERT_RETURN(frames > 0, false);

        // --------------------------------------------------------------------------------------------------------
        // Try lock, silence otherwise

        if (pData->engine->isOffline())
        {
            pData->singleMutex.lock();
        }
        else if (! pData->singleMutex.tryLock())
        {
            for (uint32_t i=0; i < pData->audioOut.count; ++i)
            {
                for (uint32_t k=0; k < frames; ++k)
                    outBuffer[i][k+timeOffset] = 0.0f;
            }

            return false;
        }

        // --------------------------------------------------------------------------------------------------------
        // Run plugin

        for (uint32_t i=0; i < pData->audioOut.count; ++i)
        {
            if (LinuxSampler::AudioChannel* const outDev = fAudioOutputDevice->Channel(i))
                outDev->SetBuffer(outBuffer[i] + timeOffset);
        }

        fAudioOutputDevice->Render(frames);

#ifndef BUILD_BRIDGE
        // --------------------------------------------------------------------------------------------------------
        // Post-processing (dry/wet, volume and balance)

        {
            const bool doVolume  = (pData->hints & PLUGIN_CAN_VOLUME) != 0 && ! carla_compareFloats(pData->postProc.volume, 1.0f);
            const bool doBalance = (pData->hints & PLUGIN_CAN_BALANCE) != 0 && ! (carla_compareFloats(pData->postProc.balanceLeft, -1.0f) && carla_compareFloats(pData->postProc.balanceRight, 1.0f));

            float oldBufLeft[doBalance ? frames : 1];

            for (uint32_t i=0; i < pData->audioOut.count; ++i)
            {
                // Balance
                if (doBalance)
                {
                    if (i % 2 == 0)
                        FloatVectorOperations::copy(oldBufLeft, outBuffer[i], static_cast<int>(frames));

                    float balRangeL = (pData->postProc.balanceLeft  + 1.0f)/2.0f;
                    float balRangeR = (pData->postProc.balanceRight + 1.0f)/2.0f;

                    for (uint32_t k=0; k < frames; ++k)
                    {
                        if (i % 2 == 0)
                        {
                            // left
                            outBuffer[i][k]  = oldBufLeft[k]     * (1.0f - balRangeL);
                            outBuffer[i][k] += outBuffer[i+1][k] * (1.0f - balRangeR);
                        }
                        else
                        {
                            // right
                            outBuffer[i][k]  = outBuffer[i][k] * balRangeR;
                            outBuffer[i][k] += oldBufLeft[k]   * balRangeL;
                        }
                    }
                }

                // Volume
                if (doVolume)
                {
                    for (uint32_t k=0; k < frames; ++k)
                        outBuffer[i][k+timeOffset] *= pData->postProc.volume;
                }
            }

        } // End of Post-processing
#endif

        // --------------------------------------------------------------------------------------------------------

        pData->singleMutex.unlock();
        return true;
    }

#ifndef CARLA_OS_WIN // FIXME, need to update linuxsampler win32 build
    void bufferSizeChanged(const uint32_t) override
    {
        CARLA_SAFE_ASSERT_RETURN(fAudioOutputDevice != nullptr,);

        fAudioOutputDevice->ReconnectAll();
    }

    void sampleRateChanged(const double) override
    {
        CARLA_SAFE_ASSERT_RETURN(fAudioOutputDevice != nullptr,);

        fAudioOutputDevice->ReconnectAll();
    }
#endif

    // -------------------------------------------------------------------
    // Plugin buffers

    // nothing

    // -------------------------------------------------------------------

    const void* getExtraStuff() const noexcept override
    {
        static const char xtrue[]  = "true";
        static const char xfalse[] = "false";
        return kUses16Outs ? xtrue : xfalse;
    }

    bool init(const char* const filename, const char* const name, const char* const label)
    {
        CARLA_SAFE_ASSERT_RETURN(pData->engine != nullptr, false);

        // ---------------------------------------------------------------
        // first checks

        if (pData->client != nullptr)
        {
            pData->engine->setLastError("Plugin client is already registered");
            return false;
        }

        if (filename == nullptr || filename[0] == '\0')
        {
            pData->engine->setLastError("null filename");
            return false;
        }

        // ---------------------------------------------------------------
        // Init LinuxSampler stuff

        fAudioOutputDevice = new LinuxSampler::AudioOutputDevicePlugin(pData->engine, this, kUses16Outs);
        fMidiInputPort     = sSampler->midiIn.CreateMidiPortPlugin();

        for (uint i=0; i<kMaxChannels; ++i)
        {
            fSamplerChannels[i] = sSampler->sampler.AddSamplerChannel();
            CARLA_SAFE_ASSERT_CONTINUE(fSamplerChannels[i] != nullptr);

            fSamplerChannels[i]->SetEngineType(kIsGIG ? "GIG" : "SFZ");
            fSamplerChannels[i]->SetAudioOutputDevice(fAudioOutputDevice);

            fEngineChannels[i] = fSamplerChannels[i]->GetEngineChannel();
            CARLA_SAFE_ASSERT_CONTINUE(fEngineChannels[i] != nullptr);

            fEngineChannels[i]->Pan(0.0f);
            fEngineChannels[i]->Volume(kIsGIG ? LinuxSampler::kVolumeMax/10.0f : LinuxSampler::kVolumeMax); // FIXME
            fEngineChannels[i]->Connect(fAudioOutputDevice);

            if (kUses16Outs)
            {
                fEngineChannels[i]->SetOutputChannel(0, i*2);
                fEngineChannels[i]->SetOutputChannel(1, i*2 +1);
                fMidiInputPort->Connect(fEngineChannels[i], static_cast<LinuxSampler::midi_chan_t>(i));
            }
            else
            {
                fEngineChannels[i]->SetOutputChannel(0, 0);
                fEngineChannels[i]->SetOutputChannel(1, 1);
                fMidiInputPort->Connect(fEngineChannels[i], LinuxSampler::midi_chan_all);
            }
        }

        // ---------------------------------------------------------------
        // Get Engine

        LinuxSampler::Engine* const engine(fEngineChannels[0]->GetEngine());

        if (engine == nullptr)
        {
            pData->engine->setLastError("Failed to get LinuxSampler instrument manager");
            return false;
        }

        // ---------------------------------------------------------------
        // Get the Engine's Instrument Manager

        fInstrument = engine->GetInstrumentManager();

        if (fInstrument == nullptr)
        {
            pData->engine->setLastError("Failed to get LinuxSampler instrument manager");
            return false;
        }

        // ---------------------------------------------------------------
        // Load the Instrument via filename

        try {
            fInstrumentIds = fInstrument->GetInstrumentFileContent(filename);
        }
        catch (const LinuxSampler::InstrumentManagerException& e)
        {
            pData->engine->setLastError(e.what());
            return false;
        }

        // ---------------------------------------------------------------
        // Get info

        if (fInstrumentIds.size() == 0)
        {
            pData->engine->setLastError("Failed to find any instruments");
            return false;
        }

        LinuxSampler::InstrumentManager::instrument_info_t info;

        try {
            info = fInstrument->GetInstrumentInfo(fInstrumentIds[0]);
        }
        catch (const LinuxSampler::InstrumentManagerException& e)
        {
            pData->engine->setLastError(e.what());
            return false;
        }

        CarlaString label2(label != nullptr ? label : File(filename).getFileNameWithoutExtension().toRawUTF8());

        if (kUses16Outs && ! label2.endsWith(" (16 outs)"))
            label2 += " (16 outs)";

        fLabel    = label2.dup();
        fMaker    = carla_strdup(info.Artists.c_str());
        fRealName = carla_strdup(info.InstrumentName.c_str());

        pData->filename = carla_strdup(filename);

        if (name != nullptr && name[0] != '\0')
            pData->name = pData->engine->getUniquePluginName(name);
        else if (fRealName[0] != '\0')
            pData->name = pData->engine->getUniquePluginName(fRealName);
        else
            pData->name = pData->engine->getUniquePluginName(fLabel);

        // ---------------------------------------------------------------
        // register client

        pData->client = pData->engine->addClient(this);

        if (pData->client == nullptr || ! pData->client->isOk())
        {
            pData->engine->setLastError("Failed to register plugin client");
            return false;
        }

        // ---------------------------------------------------------------
        // set default options

        pData->options  = 0x0;
        pData->options |= PLUGIN_OPTION_MAP_PROGRAM_CHANGES;
        pData->options |= PLUGIN_OPTION_SEND_CHANNEL_PRESSURE;
        pData->options |= PLUGIN_OPTION_SEND_PITCHBEND;
        pData->options |= PLUGIN_OPTION_SEND_ALL_SOUND_OFF;

        return true;
    }

    // -------------------------------------------------------------------

private:
    const bool kIsGIG; // SFZ if false
    const bool kUses16Outs;
    const uint kMaxChannels; // 1 or 16 depending on format

    const char* fLabel;
    const char* fMaker;
    const char* fRealName;

    int32_t fCurProgs[MAX_MIDI_CHANNELS];

    LinuxSampler::SamplerChannel* fSamplerChannels[MAX_MIDI_CHANNELS];
    LinuxSampler::EngineChannel*  fEngineChannels[MAX_MIDI_CHANNELS];

    LinuxSampler::AudioOutputDevicePlugin* fAudioOutputDevice;
    LinuxSampler::MidiInputPortPlugin*     fMidiInputPort;

    LinuxSampler::InstrumentManager* fInstrument;
    std::vector<LinuxSampler::InstrumentManager::instrument_id_t> fInstrumentIds;

    SharedResourcePointer<LinuxSampler::SamplerPlugin> sSampler;

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinuxSamplerPlugin)
};

CARLA_BACKEND_END_NAMESPACE

#endif // HAVE_LINUXSAMPLER

CARLA_BACKEND_START_NAMESPACE

// -------------------------------------------------------------------------------------------------------------------

CarlaPlugin* CarlaPlugin::newLinuxSampler(const Initializer& init, const char* const format, const bool use16Outs)
{
    carla_debug("LinuxSamplerPlugin::newLinuxSampler({%p, \"%s\", \"%s\", \"%s\", " P_INT64 "}, %s, %s)", init.engine, init.filename, init.name, init.label, init.uniqueId, format, bool2str(use16Outs));

#ifdef HAVE_LINUXSAMPLER
    CarlaString sformat(format);
    sformat.toLower();

    if (sformat != "gig" && sformat != "sfz")
    {
        init.engine->setLastError("Unsupported format requested for LinuxSampler");
        return nullptr;
    }

    if (init.engine->getProccessMode() == ENGINE_PROCESS_MODE_CONTINUOUS_RACK && use16Outs)
    {
        init.engine->setLastError("Carla's rack mode can only work with Stereo modules, please choose the 2-channel only sample-library version");
        return nullptr;
    }

    // -------------------------------------------------------------------
    // Check if file exists

    if (! File(init.filename).existsAsFile())
    {
        init.engine->setLastError("Requested file is not valid or does not exist");
        return nullptr;
    }

    LinuxSamplerPlugin* const plugin(new LinuxSamplerPlugin(init.engine, init.id, (sformat == "gig"), use16Outs));

    if (! plugin->init(init.filename, init.name, init.label))
    {
        delete plugin;
        return nullptr;
    }

    plugin->reload();

    return plugin;
#else
    init.engine->setLastError("linuxsampler support not available");
    return nullptr;

    // unused
    (void)format;
    (void)use16Outs;
#endif
}

CarlaPlugin* CarlaPlugin::newFileGIG(const Initializer& init, const bool use16Outs)
{
    carla_debug("CarlaPlugin::newFileGIG({%p, \"%s\", \"%s\", \"%s\"}, %s)", init.engine, init.filename, init.name, init.label, bool2str(use16Outs));
#ifdef HAVE_LINUXSAMPLER
    return newLinuxSampler(init, "GIG", use16Outs);
#else
    init.engine->setLastError("GIG support not available");
    return nullptr;

    // unused
    (void)use16Outs;
#endif
}

CarlaPlugin* CarlaPlugin::newFileSFZ(const Initializer& init)
{
    carla_debug("CarlaPlugin::newFileSFZ({%p, \"%s\", \"%s\", \"%s\"})", init.engine, init.filename, init.name, init.label);
#ifdef HAVE_LINUXSAMPLER
    return newLinuxSampler(init, "SFZ", false);
#else
    init.engine->setLastError("SFZ support not available");
    return nullptr;
#endif
}

// -------------------------------------------------------------------------------------------------------------------

CARLA_BACKEND_END_NAMESPACE
