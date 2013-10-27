/*
 * Carla VST Plugin
 * Copyright (C) 2011-2013 Filipe Coelho <falktx@falktx.com>
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

#include "CarlaPluginInternal.hpp"

#include "JuceHeader.h"

using juce::VSTPluginFormat;

CARLA_BACKEND_START_NAMESPACE

class JucePlugin : public CarlaPlugin
{
public:
    JucePlugin(CarlaEngine* const engine, const unsigned short id)
        : CarlaPlugin(engine, id)
    {
        carla_debug("JucePlugin::JucePlugin(%p, %i)", engine, id);
    }

    ~Vst3Plugin() override
    {
        carla_debug("JucePlugin::~JucePlugin()");

        pData->singleMutex.lock();
        pData->masterMutex.lock();

        if (pData->client != nullptr && pData->client->isActive())
            pData->client->deactivate();
    }

    // -------------------------------------------------------------------
    // Information (base)

    PluginType getType() const override
    {
        return PLUGIN_CSOUND;
    }

private:
    VSTPluginFormat format;

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JucePlugin)
};

// -----------------------------------------------------------------------

CarlaPlugin* CarlaPlugin::newCsound(const Initializer& init)
{
    carla_debug("CarlaPlugin::newCsound({%p, \"%s\", \"%s\", \"%s\"})", init.engine, init.filename, init.name, init.label);

    JucePlugin* const plugin(new JucePlugin(init.engine, init.id));

    //if (! plugin->init(init.filename, init.name, init.label))
    {
        delete plugin;
        return nullptr;
    }

    plugin->reload();

    if (init.engine->getProccessMode() == PROCESS_MODE_CONTINUOUS_RACK && ! plugin->canRunInRack())
    {
        init.engine->setLastError("Carla's rack mode can only work with Stereo VST3 plugins, sorry!");
        delete plugin;
        return nullptr;
    }

    return plugin;
}

CARLA_BACKEND_END_NAMESPACE