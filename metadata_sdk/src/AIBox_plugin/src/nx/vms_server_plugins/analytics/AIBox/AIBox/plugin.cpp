// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/



#include "plugin.h"

#include <nx/kit/utils.h>
#include <nx/kit/debug.h>

#include "engine.h"
#include "ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    auto engine = new Engine();
    engine->initialize(this);
    return engine;
}

std::string Plugin::manifestString() const
{
    const static std::string manifest = /*suppress newline*/ 1 + (const char*) R"json(
    {
        "id": "TVT.AIBox",
        "name": "AIBox",
        "description": "A plugin for subscribing to TVT's ipc target track.",
        "version": "0.8.3",
        "vendor": "TVT",
        "isLicenseRequired": %s
    }
    )json";

    return nx::kit::utils::format(manifest, ini().isLicenseRequired ? "true" : "false");
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
