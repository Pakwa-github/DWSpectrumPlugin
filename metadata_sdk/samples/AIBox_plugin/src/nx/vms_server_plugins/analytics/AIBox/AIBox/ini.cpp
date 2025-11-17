// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
