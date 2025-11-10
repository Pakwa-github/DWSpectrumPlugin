// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/



#include "plugin.h"

#include <nx/kit/utils.h>
#include <nx/kit/debug.h>

#include "engine.h"
#include "stub_analytics_plugin_AIbox_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Plugin::doObtainEngine()
{
    NX_PRINT << "PAK doObtainEngine called";
    auto engine = new Engine();
    engine->initialize(this);
    return engine;
}

std::string Plugin::manifestString() const
{
    const static std::string manifest = /*suppress newline*/ 1 + (const char*) R"json(
    {
        "id": "nx.stub.AIbox",
        "name": "Stub, Object Detection",
        "description": "HajimiNanbeiluduo.",
        "version": "0.8.3",
        "vendor": "TVT",
        "isLicenseRequired": %s
    }
    )json";

    return nx::kit::utils::format(manifest, ini().isLicenseRequired ? "true" : "false");
}

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
