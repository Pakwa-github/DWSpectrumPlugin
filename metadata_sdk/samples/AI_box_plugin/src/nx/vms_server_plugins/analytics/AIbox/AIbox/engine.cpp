// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <cctype>
#include <algorithm>

#include <nx/kit/json.h>
#include <nx/kit/debug.h>

#include "device_agent.h"
#include "device_agent_manifest.h"
#include "stub_analytics_plugin_AIbox_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const std::set<std::string> kObjectTypeIdsGeneratedByDefault = {
    "nx.base.Bike",
    "nx.base.Bus",
    "nx.base.LicensePlate",
    "nx.base.Face",
    "nx.base.Person"
};

Engine::Engine(): 
    nx::sdk::analytics::Engine(ini().enableOutput)
{
    NX_PRINT << "PAK Engine created";
}

Engine::~Engine()
{
    EngineManifestHelper::clearManifest();
    NX_PRINT << "PAK Engine destroyed";
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    // if (isCompatible(deviceInfo))
    // {
        NX_PRINT << "PAK doObtainDeviceAgent called";
        *outResult = new DeviceAgent(deviceInfo);
    // }
}

std::string Engine::manifestString() const
{
    using namespace nx::kit;

    std::string errors;
    Json deviceAgentManifest = Json::parse(kDeviceAgentManifest, errors).object_items();

    Json::array generationSettings;

    Json::object timeShiftSetting = {
        {"type", "SpinBox"},
        {"name", DeviceAgent::kTimeShiftSetting},
        {"caption", "Timestamp shift"},
        {"description", "Metadata timestamp shift in milliseconds"},
        {"defaultValue", 0}
    };
    generationSettings.push_back(std::move(timeShiftSetting));

    Json::object attributesSetting = {
        {"type", "CheckBox"},
        {"name", DeviceAgent::kSendAttributesSetting},
        {"caption", "Send object attributes"},
        {"defaultValue", true}
    };
    generationSettings.push_back(std::move(attributesSetting));

    generationSettings.push_back(Json::object{ {"type", "Separator"} });

    for (const auto& supportedType : deviceAgentManifest["supportedTypes"].array_items())
    {
        Json::object supportedTypeObject = supportedType.object_items();
        const std::string& objectTypeId = supportedTypeObject["objectTypeId"].string_value();
        Json::object generationSetting = {
            {"type", "CheckBox"},
            {"name", DeviceAgent::kObjectTypeGenerationSettingPrefix + objectTypeId},
            {"caption", objectTypeId},
            {
                "defaultValue",
                kObjectTypeIdsGeneratedByDefault.find(objectTypeId)
                    != kObjectTypeIdsGeneratedByDefault.cend()
            }
        };

        generationSettings.push_back(std::move(generationSetting));
    }

    Json::object settingsModel = {
        {"type", "Settings"},
        {"items", generationSettings}
    };

    Json::object engineManifest = {
        {"streamTypeFilter", "compressedVideo"},
        {"deviceAgentSettingsModel", settingsModel}
    };

    return Json(engineManifest).dump();
}

bool Engine::isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const
{
    // return true;
    
    // three times
    const std::string vendor = nx::kit::utils::toString(deviceInfo->vendor());
    const std::string model = nx::kit::utils::toString(deviceInfo->model());
    if (!EngineManifestHelper::isVendorSupported(vendor))
    {
        NX_PRINT << "Device vendor not supported: " << vendor;
        return false;
    }
    if (!EngineManifestHelper::isModelSupported(model))
    {
        NX_PRINT << "Device model not supported: " << model;
        return false;
    }
    NX_PRINT << "PAKPAK Device is compatible!! " << vendor << " " << model;
    return true;
}

void Engine::obtainPluginHomeDir()
{
    if (!m_plugin)
    {
        NX_PRINT << "Plugin pointer is null, cannot obtain home dir";
        return;
    }
    const auto utilityProvider = m_plugin->utilityProvider();
    NX_KIT_ASSERT(utilityProvider);

    m_pluginHomeDir = utilityProvider->homeDir();

    if (m_pluginHomeDir.empty())
    {
        NX_PRINT << "Plugin home dir: absent";
    }
    else
    {
        NX_PRINT << "Plugin home dir: " << nx::kit::utils::toString(m_pluginHomeDir);
    }
}

void Engine::loadCompatibleManifests()
{
    std::string manifestDir;
    // if (m_pluginHomeDir.empty())
    // {
    //     manifestDir = "\\static-resources\\dw_mx9\\";
    // }
    // else
    // {
    //     manifestDir = m_pluginHomeDir + "\\static-resources\\dw_mx9\\";
    // }
    // twice
    // manifestDir = "E:\\TVT\\code\\SpectrumPlugin\\dwspectrum-metadata_sdk-6.0.6.41837-universal\\metadata_sdk\\samples\\AI_box_plugin\\static-resources\\dw_mx9\\";
    manifestDir = "/Data/code/DWSpectrum/DWSpectrum_Official_SDK/1/metadata_sdk/samples/AI_box_plugin/static-resources/dw_mx9/";
    m_manifestPaths.push_back(manifestDir + "manifest_dw_mx9.json");
    m_manifestPaths.push_back(manifestDir + "manifest_evision.json");
    if (EngineManifestHelper::loadManifests(m_manifestPaths))
    {
        NX_PRINT << "Manifest files loaded from: " << manifestDir;
    }
    else
    {
        NX_PRINT << "Failed to open manifest files from: " << manifestDir;
    }
}

void Engine::initialize(nx::sdk::analytics::Plugin* plugin)
{
    // m_plugin = plugin;
    // obtainPluginHomeDir();
    loadCompatibleManifests();
}

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
