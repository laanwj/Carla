/*
 * Carla Bridge Toolkit
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

#ifndef CARLA_BRIDGE_TOOLKIT_HPP_INCLUDED
#define CARLA_BRIDGE_TOOLKIT_HPP_INCLUDED

#include "CarlaBridge.hpp"
#include "CarlaString.hpp"

CARLA_BRIDGE_START_NAMESPACE

// -----------------------------------------------------------------------

class CarlaBridgeToolkit
{
public:
    virtual ~CarlaBridgeToolkit() {}

    virtual void init() = 0;
    virtual void exec(const bool showUI) = 0;
    virtual void quit() = 0;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void resize(const int width, const int height) = 0;

    virtual void* getContainerId() const  { return nullptr; }
    virtual void* getContainerId2() const { return nullptr; }

    static CarlaBridgeToolkit* createNew(CarlaBridgeClient* const client, const char* const windowTitle);

protected:
    CarlaBridgeClient* const kClient;
    const CarlaString        kWindowTitle;

    CarlaBridgeToolkit(CarlaBridgeClient* const client, const char* const windowTitle)
        : kClient(client),
          kWindowTitle(windowTitle),
          leakDetector_CarlaBridgeToolkit() {}

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarlaBridgeToolkit)
};

// -----------------------------------------------------------------------

CARLA_BRIDGE_END_NAMESPACE

#endif // CARLA_BRIDGE_TOOLKIT_HPP_INCLUDED
