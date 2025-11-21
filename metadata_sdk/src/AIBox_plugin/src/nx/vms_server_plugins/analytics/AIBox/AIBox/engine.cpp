// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <cctype>
#include <algorithm>
#include <string>
#if defined(__GNUC__) && __GNUC__ < 9
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

#include <nx/kit/json.h>
#include <nx/kit/debug.h>

#include "device_agent.h"
#include "device_agent_manifest.h"
#include "ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const std::set<std::string> kObjectTypeIdsGeneratedByDefault = {
    kStringHuman,
    kStringMotorVehicle,
    kStringMotorcycleBicycle
};

Engine::Engine(): 
    nx::sdk::analytics::Engine(ini().enableOutput)
{
}

Engine::~Engine()
{
    EngineManifestHelper::clearManifest();
}

void Engine::initialize(nx::sdk::analytics::Plugin* plugin)
{
    m_plugin = plugin;
    obtainPluginHomeDir();
    loadCompatibleManifests();
}

bool Engine::isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const
{
    const std::string vendor = nx::kit::utils::toString(deviceInfo->vendor());
    const std::string model = nx::kit::utils::toString(deviceInfo->model());
    if (!EngineManifestHelper::isVendorSupported(vendor))
    {
        return false;
    }
    if (!EngineManifestHelper::isModelSupported(model))
    {
        return false;
    }
    NX_PRINT << "Device compatible " << vendor << " " << model;
    return true;
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

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

void Engine::obtainPluginHomeDir()
{
    if (!m_plugin)
    {
        NX_PRINT << "Plugin pointer is null, cannot obtain home dir";
        return;
    }
    const auto utilityProvider = m_plugin->utilityProvider();
    if (!NX_KIT_ASSERT(utilityProvider))
    {
        return;
    }
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
    if (m_pluginHomeDir.empty())
    {
        NX_PRINT << "Plugin home dir is empty, skipping manifest loading";
        return;
    }
    std::string baseDir = nx::kit::utils::toString(m_pluginHomeDir);
    baseDir.erase(std::remove(baseDir.begin(), baseDir.end(), '"'), baseDir.end());
    size_t start = baseDir.find_first_not_of(" \t\n\r");
    size_t end = baseDir.find_last_not_of(" \t\n\r");
    if (start == std::string::npos)
    {
        NX_PRINT << "Plugin home dir is empty after trimming";
        return;
    }
    baseDir = baseDir.substr(start, end - start + 1);
    try
    {
        if (!fs::exists(baseDir) || !fs::is_directory(baseDir))
        {
            NX_PRINT << "Plugin home dir does not exist or is not a directory: " << baseDir;
            return;
        }
        size_t added = 0;
        for (const auto& entry : fs::directory_iterator(baseDir))
        {
            if (!fs::is_regular_file(entry.status()))
            {
                continue;
            }
            const std::string filePath = entry.path().string();
            if (entry.path().extension() != ".json")
            {
                continue;
            }
            m_manifestPaths.push_back(filePath);
            ++added;
        }
        if (added == 0)
        {
            NX_PRINT << "No manifest files found in: " << baseDir;
            return;
        }
        if (EngineManifestHelper::loadManifests(m_manifestPaths))
        {
            NX_PRINT << "Loaded " << added << " manifest files from: " << baseDir;
        }
        else
        {
            NX_PRINT << "Failed to load manifest files from: " << baseDir;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "Failed to traverse plugin home dir: " << e.what() << '\n';
    }
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
