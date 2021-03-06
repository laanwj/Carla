/*
 * Carla Bridge Toolkit, Gtk version
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
#include "CarlaBridgeToolkit.hpp"

#include <gtk/gtk.h>

CARLA_BRIDGE_START_NAMESPACE

// -------------------------------------------------------------------------

static int    gargc = 0;
static char** gargv = nullptr;

// -------------------------------------------------------------------------

class CarlaBridgeToolkitGtk : public CarlaBridgeToolkit
{
public:
    CarlaBridgeToolkitGtk(CarlaBridgeClient* const client, const char* const windowTitle)
        : CarlaBridgeToolkit(client, windowTitle),
          fNeedsShow(false),
          fWindow(nullptr),
          fLastX(0),
          fLastY(0),
          fLastWidth(0),
          fLastHeight(0),
          leakDetector_CarlaBridgeToolkitGtk()
    {
        carla_debug("CarlaBridgeToolkitGtk::CarlaBridgeToolkitGtk(%p, \"%s\")", client, windowTitle);
    }

    ~CarlaBridgeToolkitGtk() override
    {
        CARLA_ASSERT(fWindow == nullptr);
        carla_debug("CarlaBridgeToolkitGtk::~CarlaBridgeToolkitGtk()");
    }

    void init() override
    {
        CARLA_ASSERT(fWindow == nullptr);
        carla_debug("CarlaBridgeToolkitGtk::init()");

        gtk_init(&gargc, &gargv);

        fWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_resize(GTK_WINDOW(fWindow), 30, 30);
        gtk_widget_hide(fWindow);
    }

    void exec(const bool showUI) override
    {
        CARLA_ASSERT(kClient != nullptr);
        CARLA_ASSERT(fWindow != nullptr);
        carla_debug("CarlaBridgeToolkitGtk::exec(%s)", bool2str(showUI));

        GtkWidget* const widget((GtkWidget*)kClient->getWidget());

        gtk_container_add(GTK_CONTAINER(fWindow), widget);

        gtk_window_set_resizable(GTK_WINDOW(fWindow), kClient->isResizable());
        gtk_window_set_title(GTK_WINDOW(fWindow), kWindowTitle);

        if (showUI || fNeedsShow)
        {
            show();
            fNeedsShow = false;
        }

        g_timeout_add(30, gtk_ui_timeout, this);
        g_signal_connect(fWindow, "destroy", G_CALLBACK(gtk_ui_destroy), this);

        // First idle
        handleTimeout();

        // Main loop
        gtk_main();
    }

    void quit() override
    {
        carla_debug("CarlaBridgeToolkitGtk::quit()");

        if (fWindow != nullptr)
        {
            gtk_widget_destroy(fWindow);
            fWindow = nullptr;

            if (gtk_main_level() != 0)
                gtk_main_quit();
        }
    }

    void show() override
    {
        carla_debug("CarlaBridgeToolkitGtk::show()");

        fNeedsShow = true;

        if (fWindow != nullptr)
            gtk_widget_show_all(fWindow);
    }

    void hide() override
    {
        carla_debug("CarlaBridgeToolkitGtk::hide()");

        fNeedsShow = false;

        if (fWindow != nullptr)
            gtk_widget_hide(fWindow);
    }

    void resize(int width, int height) override
    {
        CARLA_ASSERT(fWindow != nullptr);
        carla_debug("CarlaBridgeToolkitGtk::resize(%i, %i)", width, height);

        if (fWindow != nullptr)
            gtk_window_resize(GTK_WINDOW(fWindow), width, height);
    }

    // ---------------------------------------------------------------------

protected:
    bool fNeedsShow;
    GtkWidget* fWindow;

    gint fLastX;
    gint fLastY;
    gint fLastWidth;
    gint fLastHeight;

    void handleDestroy()
    {
        carla_debug("CarlaBridgeToolkitGtk::handleDestroy()");

        fWindow = nullptr;
    }

    gboolean handleTimeout()
    {
        if (fWindow != nullptr)
        {
            gtk_window_get_position(GTK_WINDOW(fWindow), &fLastX, &fLastY);
            gtk_window_get_size(GTK_WINDOW(fWindow), &fLastWidth, &fLastHeight);
        }

        kClient->uiIdle();
        return kClient->oscIdle();
    }

    // ---------------------------------------------------------------------

private:
    static void gtk_ui_destroy(GtkWidget*, gpointer data)
    {
        CARLA_ASSERT(data != nullptr);

        if (CarlaBridgeToolkitGtk* const _this_ = (CarlaBridgeToolkitGtk*)data)
            _this_->handleDestroy();

        gtk_main_quit();
    }

    static gboolean gtk_ui_timeout(gpointer data)
    {
        CARLA_ASSERT(data != nullptr);

        if (CarlaBridgeToolkitGtk* const _this_ = (CarlaBridgeToolkitGtk*)data)
            return _this_->handleTimeout();

        return false;
    }

    CARLA_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarlaBridgeToolkitGtk)
};

// -------------------------------------------------------------------------

CarlaBridgeToolkit* CarlaBridgeToolkit::createNew(CarlaBridgeClient* const client, const char* const windowTitle)
{
    return new CarlaBridgeToolkitGtk(client, windowTitle);
}

// -------------------------------------------------------------------------

CARLA_BRIDGE_END_NAMESPACE
