// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

static const std::string kStringHuman =             "nx.base.Person";
static const std::string kStringMotorVehicle =      "nx.base.Car";
static const std::string kStringMotorcycleBicycle = "nx.base.Bike";
static const std::string kStringUnknown =           "nx.base.Unknown";
static const std::string kStringPerson =            "person";
static const std::string kStringCar =               "car";
static const std::string kStringMotor =             "motor";

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Person"
        },
        {
            "objectTypeId": "nx.base.Car"
        },
        {
            "objectTypeId": "nx.base.Bike"
        }
    ]
}
)json";

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
