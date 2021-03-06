/*
 * Carla Plugin
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

#ifndef CARLA_PLUGIN_THREAD_HPP_INCLUDED
#define CARLA_PLUGIN_THREAD_HPP_INCLUDED

#include "CarlaBackend.h"
#include "CarlaJuceUtils.hpp"
#include "CarlaThread.hpp"

#include "juce_core.h"

using juce::ChildProcess;
using juce::ScopedPointer;

CARLA_BACKEND_START_NAMESPACE

// -----------------------------------------------------------------------

class CarlaPluginThread : public CarlaThread
{
public:
    enum Mode {
        PLUGIN_THREAD_NULL,
        PLUGIN_THREAD_DSSI_GUI,
        PLUGIN_THREAD_LV2_GUI,
        PLUGIN_THREAD_VST_GUI,
        PLUGIN_THREAD_BRIDGE
    };

    CarlaPluginThread(CarlaEngine* const engine, CarlaPlugin* const plugin, const Mode mode = PLUGIN_THREAD_NULL) noexcept;
    ~CarlaPluginThread() noexcept override;

    void setMode(const CarlaPluginThread::Mode mode) noexcept;
    void setOscData(const char* const binary, const char* const label, const char* const extra1="", const char* const extra2="") noexcept;

    uintptr_t getPid() const;

protected:
    void run() override;

private:
    CarlaEngine* const fEngine;
    CarlaPlugin* const fPlugin;

    Mode        fMode;
    CarlaString fBinary;
    CarlaString fLabel;
    CarlaString fExtra1;
    CarlaString fExtra2;

    ScopedPointer<ChildProcess> fProcess;

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarlaPluginThread)
};

// -----------------------------------------------------------------------

CARLA_BACKEND_END_NAMESPACE

#endif // CARLA_PLUGIN_THREAD_HPP_INCLUDED
