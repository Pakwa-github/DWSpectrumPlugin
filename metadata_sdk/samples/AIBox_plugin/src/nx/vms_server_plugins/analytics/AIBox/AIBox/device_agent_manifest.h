// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "supportedTypes":
    [
        {
            "objectTypeId": "TVT.base.Human"
        },
        {
            "objectTypeId": "TVT.base.Motor Vehicle"
        },
        {
            "objectTypeId": "TVT.base.Motorcycle/Bicycle"
        },
        {
            "objectTypeId": "TVT.base.Unknown"
        }
    ]
}
)json";

static const std::string kStringHuman =             "TVT.base.Human";
static const std::string kStringMotorVehicle =      "TVT.base.Motor Vehicle";
static const std::string kStringMotorcycleBicycle = "TVT.base.Motorcycle/Bicycle";
static const std::string kStringUnknown =           "TVT.base.Unknown";
static const std::string kStringPerson =            "person";
static const std::string kStringCar =               "car";
static const std::string kStringMotor =             "motor";
} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
