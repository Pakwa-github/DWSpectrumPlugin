// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("AIBox_plugin.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "Can use NX_OUTPUT or not.");
    NX_INI_FLAG(0, isLicenseRequired, "Whether the Plugin declares in its manifest that it requires a license.");
};

Ini& ini();

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
