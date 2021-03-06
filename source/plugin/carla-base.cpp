/*
 * Carla Native Plugins
 * Copyright (C) 2013-2014 Filipe Coelho <falktx@falktx.com>
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

#include "CarlaEngine.hpp"
#include "CarlaPlugin.hpp"
#include "CarlaHost.h"
#include "CarlaNative.h"

#include "CarlaBackendUtils.hpp"
#include "LinkedList.hpp"

#ifdef CARLA_NATIVE_PLUGIN_DSSI
# include "dssi/dssi.h"
#endif

#ifdef CARLA_NATIVE_PLUGIN_LV2
# include "lv2/lv2.h"
#endif

using CarlaBackend::CarlaPlugin;

// -----------------------------------------------------------------------
// Plugin List

struct PluginListManager {
    PluginListManager()
#ifdef CARLA_NATIVE_PLUGIN_DSSI
        : dssiDescs(),
#endif
#ifdef CARLA_NATIVE_PLUGIN_LV2
        : lv2Descs(),
#endif
#ifdef CARLA_NATIVE_PLUGIN_VST
        : _dummy(0),
#endif
          descs()
    {
        for (size_t i=0, count = CarlaPlugin::getNativePluginCount(); i < count; ++i)
        {
            const NativePluginDescriptor* const desc(CarlaPlugin::getNativePluginDescriptor(i));

            // Open/Save not possible in plugins
            if (desc->hints & PLUGIN_NEEDS_UI_OPEN_SAVE)
                continue;

            // skip some plugins
            if (std::strcmp(desc->label, "vexsynth") == 0)
            {
                continue;
            }

            // skip midi plugins, not implemented yet
            if (desc->audioIns == 0 && desc->audioOuts == 0 && desc->midiIns == 1 && desc->midiOuts >= 1)
                continue;

            descs.append(desc);
        }
    }

    ~PluginListManager()
    {
#ifdef CARLA_NATIVE_PLUGIN_DSSI
        for (LinkedList<const DSSI_Descriptor*>::Itenerator it = dssiDescs.begin(); it.valid(); it.next())
        {
            const DSSI_Descriptor* const dssiDesc(it.getValue());
            //delete[] lv2Desc->URI;
            delete dssiDesc;
        }
        dssiDescs.clear();
#endif

#ifdef CARLA_NATIVE_PLUGIN_LV2
        for (LinkedList<const LV2_Descriptor*>::Itenerator it = lv2Descs.begin(); it.valid(); it.next())
        {
            const LV2_Descriptor* const lv2Desc(it.getValue());
            delete[] lv2Desc->URI;
            delete lv2Desc;
        }
        lv2Descs.clear();
#endif

        descs.clear();
    }

    static PluginListManager& getInstance()
    {
        static PluginListManager plm;
        return plm;
    }

#ifdef CARLA_NATIVE_PLUGIN_DSSI
    LinkedList<const DSSI_Descriptor*> dssiDescs;
#endif
#ifdef CARLA_NATIVE_PLUGIN_LV2
    LinkedList<const LV2_Descriptor*> lv2Descs;
#endif
#ifdef CARLA_NATIVE_PLUGIN_VST
    char _dummy;
#endif
    LinkedList<const NativePluginDescriptor*> descs;
};

// -----------------------------------------------------------------------
