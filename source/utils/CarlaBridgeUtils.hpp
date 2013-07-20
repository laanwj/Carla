/*
 * Carla Bridge utils imported from dssi-vst code
 * Copyright (C) 2004-2010 Chris Cannam <cannam@all-day-breakfast.com>
 * Copyright (C) 2013 Filipe Coelho <falktx@falktx.com>
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
 * For a full copy of the GNU General Public License see the GPL.txt file
 */

#ifndef __CARLA_BRIDGE_UTILS_HPP__
#define __CARLA_BRIDGE_UTILS_HPP__

#include "CarlaRingBuffer.hpp"

// -----------------------------------------------------------------------

enum PluginBridgeInfoType {
    kPluginBridgeAudioCount = 0,
    kPluginBridgeMidiCount,
    kPluginBridgeParameterCount,
    kPluginBridgeProgramCount,
    kPluginBridgeMidiProgramCount,
    kPluginBridgePluginInfo,
    kPluginBridgeParameterInfo,
    kPluginBridgeParameterData,
    kPluginBridgeParameterRanges,
    kPluginBridgeProgramInfo,
    kPluginBridgeMidiProgramInfo,
    kPluginBridgeConfigure,
    kPluginBridgeSetParameterValue,
    kPluginBridgeSetDefaultValue,
    kPluginBridgeSetProgram,
    kPluginBridgeSetMidiProgram,
    kPluginBridgeSetCustomData,
    kPluginBridgeSetChunkData,
    kPluginBridgeUpdateNow,
    kPluginBridgeError
};

enum PluginBridgeOpcode {
    kPluginBridgeOpcodeNull           = 0,
    kPluginBridgeOpcodeSetAudioPool   = 1, // long
    kPluginBridgeOpcodeSetBufferSize  = 2, // int
    kPluginBridgeOpcodeSetSampleRate  = 3, // float
    kPluginBridgeOpcodeSetParameter   = 4, // int, float
    kPluginBridgeOpcodeSetProgram     = 5, // int
    kPluginBridgeOpcodeSetMidiProgram = 6, // int
    kPluginBridgeOpcodeMidiEvent      = 7, // long, int, char[] (long = timeFrame, int = size max 4)
    kPluginBridgeOpcodeProcess        = 8,
    kPluginBridgeOpcodeQuit           = 9
};

const char* const CARLA_BRIDGE_MSG_HIDE_GUI   = "CarlaBridgeHideGUI";   //!< Plugin -> Host call, tells host GUI is now hidden
const char* const CARLA_BRIDGE_MSG_SAVED      = "CarlaBridgeSaved";     //!< Plugin -> Host call, tells host state is saved
#if 0
const char* const CARLA_BRIDGE_MSG_SAVE_NOW   = "CarlaBridgeSaveNow";   //!< Host -> Plugin call, tells plugin to save state now
const char* const CARLA_BRIDGE_MSG_SET_CHUNK  = "CarlaBridgeSetChunk";  //!< Host -> Plugin call, tells plugin to set chunk in file \a value
const char* const CARLA_BRIDGE_MSG_SET_CUSTOM = "CarlaBridgeSetCustom"; //!< Host -> Plugin call, tells plugin to set a custom data set using \a value ("type·key·rvalue").
//If \a type is 'chunk' or 'binary' \a rvalue refers to chunk file.
#endif

// -----------------------------------------------------------------------

struct BridgeShmControl {
    // 32 and 64-bit binaries align semaphores differently.
    // Let's make sure there's plenty of room for either one.
    union {
        void* runServer;
        char _alignServer[128];
    };
    union {
        void* runClient;
        char _alignClient[128];
    };
    RingBuffer ringBuffer;

    CARLA_DECLARE_NON_COPY_STRUCT(BridgeShmControl)
};

// -----------------------------------------------------------------------

static inline
const char* PluginBridgeInfoType2str(const PluginBridgeInfoType type)
{
    switch (type)
    {
    case kPluginBridgeAudioCount:
        return "kPluginBridgeAudioCount";
    case kPluginBridgeMidiCount:
        return "kPluginBridgeMidiCount";
    case kPluginBridgeParameterCount:
        return "kPluginBridgeParameterCount";
    case kPluginBridgeProgramCount:
        return "kPluginBridgeProgramCount";
    case kPluginBridgeMidiProgramCount:
        return "kPluginBridgeMidiProgramCount";
    case kPluginBridgePluginInfo:
        return "kPluginBridgePluginInfo";
    case kPluginBridgeParameterInfo:
        return "kPluginBridgeParameterInfo";
    case kPluginBridgeParameterData:
        return "kPluginBridgeParameterData";
    case kPluginBridgeParameterRanges:
        return "kPluginBridgeParameterRanges";
    case kPluginBridgeProgramInfo:
        return "kPluginBridgeProgramInfo";
    case kPluginBridgeMidiProgramInfo:
        return "kPluginBridgeMidiProgramInfo";
    case kPluginBridgeConfigure:
        return "kPluginBridgeConfigure";
    case kPluginBridgeSetParameterValue:
        return "kPluginBridgeSetParameterValue";
    case kPluginBridgeSetDefaultValue:
        return "kPluginBridgeSetDefaultValue";
    case kPluginBridgeSetProgram:
        return "kPluginBridgeSetProgram";
    case kPluginBridgeSetMidiProgram:
        return "kPluginBridgeSetMidiProgram";
    case kPluginBridgeSetCustomData:
        return "kPluginBridgeSetCustomData";
    case kPluginBridgeSetChunkData:
        return "kPluginBridgeSetChunkData";
    case kPluginBridgeUpdateNow:
        return "kPluginBridgeUpdateNow";
    case kPluginBridgeError:
        return "kPluginBridgeError";
    }

    carla_stderr("CarlaBackend::PluginBridgeInfoType2str(%i) - invalid type", type);
    return nullptr;
}

static inline
const char* PluginBridgeOpcode2str(const PluginBridgeOpcode opcode)
{
    switch (opcode)
    {
    case kPluginBridgeOpcodeNull:
        return "kPluginBridgeOpcodeNull";
    case kPluginBridgeOpcodeSetAudioPool:
        return "kPluginBridgeOpcodeSetAudioPool";
    case kPluginBridgeOpcodeSetBufferSize:
        return "kPluginBridgeOpcodeSetBufferSize";
    case kPluginBridgeOpcodeSetSampleRate:
        return "kPluginBridgeOpcodeSetSampleRate";
    case kPluginBridgeOpcodeSetParameter:
        return "kPluginBridgeOpcodeSetParameter";
    case kPluginBridgeOpcodeSetProgram:
        return "kPluginBridgeOpcodeSetProgram";
    case kPluginBridgeOpcodeSetMidiProgram:
        return "kPluginBridgeOpcodeSetMidiProgram";
    case kPluginBridgeOpcodeMidiEvent:
        return "kPluginBridgeOpcodeMidiEvent";
    case kPluginBridgeOpcodeProcess:
        return "kPluginBridgeOpcodeProcess";
    case kPluginBridgeOpcodeQuit:
        return "kPluginBridgeOpcodeQuit";
    }

    carla_stderr("CarlaBackend::PluginBridgeOpcode2str(%i) - invalid opcode", opcode);
    return nullptr;
}

#endif // __CARLA_BRIDGE_UTILS_HPP__
