/*
 * Carla Bridge OSC
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

#include "CarlaBridgeClient.hpp"
#include "CarlaMIDI.h"

CARLA_BRIDGE_START_NAMESPACE

// -----------------------------------------------------------------------

CarlaBridgeOsc::CarlaBridgeOsc(CarlaBridgeClient* const client)
    : kClient(client),
      fControlData(),
      fName(),
      fServerPath(),
      fServer(nullptr),
      leakDetector_CarlaBridgeOsc()
{
    CARLA_ASSERT(client != nullptr);
    carla_debug("CarlaBridgeOsc::CarlaBridgeOsc(%p)", client);
}

CarlaBridgeOsc::~CarlaBridgeOsc()
{
    CARLA_ASSERT(fControlData.source == nullptr); // must never be used
    CARLA_ASSERT(fName.isEmpty());
    CARLA_ASSERT(fServerPath.isEmpty());
    CARLA_ASSERT(fServer == nullptr);
    carla_debug("CarlaBridgeOsc::~CarlaBridgeOsc()");
}

void CarlaBridgeOsc::init(const char* const url)
{
    CARLA_ASSERT(fName.isEmpty());
    CARLA_ASSERT(fServerPath.isEmpty());
    CARLA_ASSERT(fServer == nullptr);
    CARLA_ASSERT(url != nullptr);
    carla_debug("CarlaBridgeOsc::init(\"%s\")", url);

    std::srand((uint)(uintptr_t)this);
    std::srand((uint)(uintptr_t)&url);

    fName  = "ui-";
    fName += CarlaString(std::rand() % 99999);

    fServer = lo_server_new_with_proto(nullptr, LO_UDP, osc_error_handler);

    CARLA_SAFE_ASSERT_RETURN(fServer != nullptr,)

    {
        char* const host = lo_url_get_hostname(url);
        char* const port = lo_url_get_port(url);
        fControlData.path   = carla_strdup_free(lo_url_get_path(url));
        fControlData.target = lo_address_new_with_proto(LO_UDP, host, port);

        std::free(host);
        std::free(port);
    }

    if (char* const tmpServerPath = lo_server_get_url(fServer))
    {
        fServerPath  = tmpServerPath;
        fServerPath += fName;
        std::free(tmpServerPath);
    }

    lo_server_add_method(fServer, nullptr, nullptr, osc_message_handler, this);

    CARLA_ASSERT(fName.isNotEmpty());
    CARLA_ASSERT(fServerPath.isNotEmpty());
}

void CarlaBridgeOsc::idle() const
{
    if (fServer == nullptr)
        return;

    for (; lo_server_recv_noblock(fServer, 0) != 0;) {}
}

void CarlaBridgeOsc::idleOnce() const
{
    if (fServer == nullptr)
        return;

    lo_server_recv_noblock(fServer, 0);
}

void CarlaBridgeOsc::close()
{
    CARLA_ASSERT(fControlData.source == nullptr); // must never be used
    CARLA_ASSERT(fName.isNotEmpty());
    CARLA_ASSERT(fServerPath.isNotEmpty());
    CARLA_ASSERT(fServer != nullptr);
    carla_debug("CarlaBridgeOsc::close()");

    fName.clear();

    if (fServer != nullptr)
    {
        lo_server_del_method(fServer, nullptr, nullptr);
        lo_server_free(fServer);
        fServer = nullptr;
    }

    fServerPath.clear();
    fControlData.clear();

    CARLA_ASSERT(fName.isEmpty());
    CARLA_ASSERT(fServerPath.isEmpty());
    CARLA_ASSERT(fServer == nullptr);
}

// -----------------------------------------------------------------------

int CarlaBridgeOsc::handleMessage(const char* const path, const int argc, const lo_arg* const* const argv, const char* const types, const lo_message msg)
{
    CARLA_SAFE_ASSERT_RETURN(fName.isNotEmpty(), 1);
    CARLA_SAFE_ASSERT_RETURN(fServerPath.isNotEmpty(), 1);
    CARLA_SAFE_ASSERT_RETURN(fServer != nullptr, 1);
    CARLA_SAFE_ASSERT_RETURN(path != nullptr, 1);
    CARLA_SAFE_ASSERT_RETURN(msg != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMessage(\"%s\", %i, %p, \"%s\", %p)", path, argc, argv, types, msg);

    const size_t nameSize(fName.length());

    // Check if message is for this client
    if (std::strlen(path) <= nameSize || std::strncmp(path+1, fName, nameSize) != 0)
    {
        carla_stderr("CarlaBridgeOsc::handleMessage() - message not for this client -> '%s' != '/%s/'", path, fName.buffer());
        return 1;
    }

    // Get method from path
    char method[32+1] = { '\0' };
    std::strncpy(method, path + (nameSize + 2), 32);

    if (method[0] == '\0')
    {
        carla_stderr("CarlaBridgeOsc::handleMessage(\"%s\", ...) - received message without method", path);
        return 1;
    }

    // Common methods
    if (std::strcmp(method, "show") == 0)
        return handleMsgShow();
    if (std::strcmp(method, "hide") == 0)
        return handleMsgHide();
    if (std::strcmp(method, "quit") == 0)
        return handleMsgQuit();

    // UI methods
    if (std::strcmp(method, "configure") == 0)
        return handleMsgConfigure(argc, argv, types);
    if (std::strcmp(method, "control") == 0)
        return handleMsgControl(argc, argv, types);
    if (std::strcmp(method, "program") == 0)
        return handleMsgProgram(argc, argv, types);
    if (std::strcmp(method, "midi-program") == 0)
        return handleMsgMidiProgram(argc, argv, types);
    if (std::strcmp(method, "midi") == 0)
        return handleMsgMidi(argc, argv, types);
    if (std::strcmp(method, "sample-rate") == 0)
        return 0; // unused

    // special
    if (std::strcmp(method, "update_url") == 0)
        return handleMsgUpdateURL(argc, argv, types);

#ifdef BRIDGE_LV2
    // LV2 methods
    if (std::strcmp(method, "lv2_atom_transfer") == 0)
        return handleMsgLv2AtomTransfer(argc, argv, types);
    if (std::strcmp(method, "lv2_urid_map") == 0)
        return handleMsgLv2UridMap(argc, argv, types);
#endif

    carla_stderr("CarlaBridgeOsc::handleMessage(\"%s\", ...) - received unsupported OSC method '%s'", path, method);
    return 1;
}

int CarlaBridgeOsc::handleMsgShow()
{
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgShow()");

    kClient->toolkitShow();

    return 0;
}

int CarlaBridgeOsc::handleMsgHide()
{
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgHide()");

    kClient->toolkitHide();

    return 0;
}

int CarlaBridgeOsc::handleMsgQuit()
{
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgQuit()");

    kClient->toolkitQuit();

    return 0;
}

int CarlaBridgeOsc::handleMsgConfigure(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(2, "ss");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgConfigure()");

    // nothing here for now
    return 0;

    // unused
    (void)argv;
}

int CarlaBridgeOsc::handleMsgControl(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(2, "if");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgControl()");

    const int32_t index = argv[0]->i;
    const float   value = argv[1]->f;

    CARLA_SAFE_ASSERT_RETURN(index != -1, 1);

    kClient->setParameter(index, value);

    return 0;
}

int CarlaBridgeOsc::handleMsgProgram(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(1, "i");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgProgram()");

    const int32_t index = argv[0]->i;

    CARLA_SAFE_ASSERT_RETURN(index >= 0, 1);

    kClient->setProgram(static_cast<uint32_t>(index));

    return 0;
}

int CarlaBridgeOsc::handleMsgMidiProgram(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(2, "ii");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgMidiProgram()");

    const int32_t bank    = argv[0]->i;
    const int32_t program = argv[1]->i;

    CARLA_SAFE_ASSERT_RETURN(bank >= 0, 1);
    CARLA_SAFE_ASSERT_RETURN(program >= 0, 1);

    kClient->setMidiProgram(static_cast<uint32_t>(bank), static_cast<uint32_t>(program));

    return 0;
}

int CarlaBridgeOsc::handleMsgMidi(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(1, "m");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgMidi()");

    const uint8_t* const data = argv[0]->m;
    uint8_t status  = data[1];
    uint8_t channel = status & MIDI_CHANNEL_BIT;

    // Fix bad note-off
    if (MIDI_IS_STATUS_NOTE_ON(status) && data[3] == 0)
        status = uint8_t(MIDI_STATUS_NOTE_OFF | (channel & MIDI_CHANNEL_BIT));

    if (MIDI_IS_STATUS_NOTE_OFF(status))
    {
        const uint8_t note = data[2];

        CARLA_SAFE_ASSERT_RETURN(note < MAX_MIDI_NOTE, 1);

        kClient->noteOff(channel, note);
    }
    else if (MIDI_IS_STATUS_NOTE_ON(status))
    {
        const uint8_t note = data[2];
        const uint8_t velo = data[3];

        CARLA_SAFE_ASSERT_RETURN(note < MAX_MIDI_NOTE, 1);
        CARLA_SAFE_ASSERT_RETURN(velo < MAX_MIDI_VALUE, 1);

        kClient->noteOn(channel, note, velo);
    }

    return 0;
}

int CarlaBridgeOsc::handleMsgUpdateURL(CARLA_BRIDGE_OSC_HANDLE_ARGS)
{
    CARLA_BRIDGE_OSC_CHECK_OSC_TYPES(1, "s");
    CARLA_SAFE_ASSERT_RETURN(kClient != nullptr, 1);
    carla_debug("CarlaBridgeOsc::handleMsgUpdateURL()");

    const char* const url = (const char*)&argv[0]->s;

    fControlData.setNewURL(url);
    return 0;
}

// -----------------------------------------------------------------------

CARLA_BRIDGE_END_NAMESPACE
