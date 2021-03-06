/*
 * Carla Native Plugins
 * Copyright (C) 2012-2014 Filipe Coelho <falktx@falktx.com>
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

#include "CarlaNative.h"

// -----------------------------------------------------------------------
// Plugin Code

#include "nekofilter/nekofilter.c"
#include "nekofilter/filter.c"
#include "nekofilter/log.c"

// -----------------------------------------------------------------------

static const NativePluginDescriptor nekofilterDesc = {
    .category  = PLUGIN_CATEGORY_FILTER,
#ifdef WANT_UI
    .hints     = PLUGIN_IS_RTSAFE|PLUGIN_HAS_UI,
#else
    .hints     = PLUGIN_IS_RTSAFE,
#endif
    .supports  = 0x0,
    .audioIns  = 1,
    .audioOuts = 1,
    .midiIns   = 0,
    .midiOuts  = 0,
    .paramIns  = GLOBAL_PARAMETERS_COUNT + BAND_PARAMETERS_COUNT*BANDS_COUNT,
    .paramOuts = 0,
    .name      = "NekoFilter",
    .label     = "nekofilter",
    .maker     = "falkTX, Nedko, Fons Adriaensen",
    .copyright = "GNU GPL v2+",

    .instantiate = nekofilter_instantiate,
    .cleanup     = nekofilter_cleanup,

    .get_parameter_count = nekofilter_get_parameter_count,
    .get_parameter_info  = nekofilter_get_parameter_info,
    .get_parameter_value = nekofilter_get_parameter_value,
    .get_parameter_text  = NULL,

    .get_midi_program_count = NULL,
    .get_midi_program_info  = NULL,

    .set_parameter_value = nekofilter_set_parameter_value,
    .set_midi_program    = NULL,
    .set_custom_data     = NULL,

#ifdef WANT_UI
    .ui_show = nekofilter_ui_show,
    .ui_idle = nekofilter_ui_idle,

    .ui_set_parameter_value = nekofilter_ui_set_parameter_value,
    .ui_set_midi_program    = NULL,
    .ui_set_custom_data     = NULL,
#else
    .ui_show = NULL,
    .ui_idle = NULL,

    .ui_set_parameter_value = NULL,
    .ui_set_midi_program    = NULL,
    .ui_set_custom_data     = NULL,
#endif

    .activate   = NULL,
    .deactivate = NULL,
    .process    = nekofilter_process,

    .get_state = NULL,
    .set_state = NULL,

    .dispatcher = NULL
};

// -----------------------------------------------------------------------

void carla_register_native_plugin_nekofilter(void);

void carla_register_native_plugin_nekofilter(void)
{
    carla_register_native_plugin(&nekofilterDesc);
}

// -----------------------------------------------------------------------
