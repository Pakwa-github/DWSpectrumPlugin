// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/
#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "AIbox/plugin.h"

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPluginByIndex(int instanceIndex)
{
    using namespace nx::vms_server_plugins::analytics::stub;
    NX_PRINT << "Pak11";
    NX_OUTPUT << "Pak2222";
    switch (instanceIndex)
    {
		case 0: return new AIbox::Plugin();
        default: return nullptr;
    }
}
