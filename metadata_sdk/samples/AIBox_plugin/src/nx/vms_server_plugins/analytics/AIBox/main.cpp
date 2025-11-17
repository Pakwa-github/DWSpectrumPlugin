// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>

#include "AIBox/plugin.h"

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int instanceIndex)
{
    using namespace nx::vms_server_plugins::analytics;
    NX_PRINT << "Pak11";
    NX_PRINT << "Pak2222";
    switch (instanceIndex)
    {
		case 0: return new AIBox::Plugin();
        default: return nullptr;
    }
}
