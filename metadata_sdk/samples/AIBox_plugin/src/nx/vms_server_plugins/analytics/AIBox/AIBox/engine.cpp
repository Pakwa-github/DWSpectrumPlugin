// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <cctype>
#include <algorithm>
#include <dirent.h>
#include <cstring>

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
    "TVT.base.PEA",
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
    NX_PRINT << "PAK doObtainDeviceAgent called";
    *outResult = new DeviceAgent(deviceInfo);
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
    DIR* dir = opendir(baseDir.c_str());
    if (!dir)
    {
        NX_PRINT << "Failed to open plugin home dir: " << baseDir << ", error: " << strerror(errno);
        return;
    }
    size_t added = 0;
    dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type != DT_REG)
        {
            continue;
        }
        const std::string fileName = entry->d_name;
        const char* ext = strrchr(fileName.c_str(), '.');
        if (ext == nullptr || strcmp(ext, ".json") != 0)
        {
            continue;
        }
        const std::string fullPath = baseDir + "/" + fileName;
        m_manifestPaths.push_back(fullPath);
        ++added;
    }
    closedir(dir);
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

void Engine::initialize(nx::sdk::analytics::Plugin* plugin)
{
    m_plugin = plugin;
    obtainPluginHomeDir();
    loadCompatibleManifests();
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
