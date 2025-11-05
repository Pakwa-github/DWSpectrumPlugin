
#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "engine_manifest.h"
#include <fstream>
#include <cctype>
#include <algorithm>


namespace nx::vms_server_plugins::analytics::stub::AIbox::ManifestStorage {
    std::unordered_set<std::string> supportedVendors;
    std::unordered_set<std::string> supportedModels;
    std::unordered_set<std::string> partlySupportedModels;
    std::vector<EventType> supportedPeaEvents;
}

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {
    
std::string EngineManifestHelper::toLowerSpaceless(const std::string& str)
{
    std::string result;
    for (char c : str)
    {
        if (c == '"' || c == '\'')
        {
            continue;
        }
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

void EngineManifestHelper::parseEvents(const nlohmann::json& eventJsonArray, std::vector<EventType>& outEvents) {
    if (!eventJsonArray.is_array())
    {
        NX_OUTPUT << "Event JSON is not an array";
        return;
    }

    for (const auto& eventJson : eventJsonArray)
    {
        if (!eventJson.contains("internalName") || eventJson["internalName"].get<std::string>() != "PEA")
        {
            continue;
        }

        EventType peaEvent;
        peaEvent.id = eventJson.value("id", "");
        peaEvent.internalName = "PEA";
        peaEvent.alarmName = eventJson.value("name", "");
        peaEvent.restricted = eventJson.value("restricted", false);
        peaEvent.group = eventJson.value("group", 0);

        if (!peaEvent.id.empty())
        {
            outEvents.push_back(peaEvent);
            NX_PRINT << "Loaded PEA event: " << peaEvent.id;
        }
    }
}

bool EngineManifestHelper::loadManifest(const std::string& jsonFilePath)
{
    std::ifstream jsonFile(jsonFilePath);
    if (!jsonFile.is_open())
    {
        NX_OUTPUT << "Failed to open manifest file: " << jsonFilePath;
        return false;
    }
    NX_PRINT << "Successfully opened manifest file: " << jsonFilePath;
    nlohmann::json manifestJson;
    try 
    {
        jsonFile >> manifestJson;
    } 
    catch (const nlohmann::json::parse_error& e)
    {
        NX_OUTPUT << "JSON parse error in " << jsonFilePath << ": " << e.what();
        jsonFile.close();
        return false;
    }
    jsonFile.close();

    if (manifestJson.contains("supportedCameraVendors") && manifestJson["supportedCameraVendors"].is_array())
    {
        for (const auto& vendor : manifestJson["supportedCameraVendors"])
        {
            std::string lowerVendor = toLowerSpaceless(vendor.get<std::string>());
            ManifestStorage::supportedVendors.insert(lowerVendor);
        }
    }

    if (manifestJson.contains("supportedCameraModels") && manifestJson["supportedCameraModels"].is_array())
    {
        for (const auto& model : manifestJson["supportedCameraModels"])
        {
            std::string lowerModel = toLowerSpaceless(model.get<std::string>());
            ManifestStorage::supportedModels.insert(lowerModel);
        }
    }

    if (manifestJson.contains("partlySupportedCameraModels") && manifestJson["partlySupportedCameraModels"].is_array())
    {
        for (const auto& model : manifestJson["partlySupportedCameraModels"])
        {
            std::string lowerModel = toLowerSpaceless(model.get<std::string>());
            ManifestStorage::partlySupportedModels.insert(lowerModel);
        }
    }

    if (manifestJson.contains("eventTypes") && manifestJson["eventTypes"].is_array())
    {
        parseEvents(manifestJson["eventTypes"], ManifestStorage::supportedPeaEvents);
    }

    NX_PRINT << "Successfully loaded manifest: " << jsonFilePath;
    return true;
}

bool EngineManifestHelper::loadManifests(const std::vector<std::string>& jsonFilePaths)
{
    NX_PRINT << "Before loading, supportedVendors count: " << ManifestStorage::supportedVendors.size();
    bool allLoaded = true;
    for (const auto& path : jsonFilePaths)
    {
        if (!loadManifest(path))
        {
            allLoaded = false;
        }
    }
    return allLoaded;
}

bool EngineManifestHelper::isVendorSupported(const std::string& deviceVendor)
{
    const std::string lowerVendor = toLowerSpaceless(deviceVendor);
    for (const auto& supportedVendor : ManifestStorage::supportedVendors)
    {
        if (lowerVendor.find(supportedVendor) == 0)
        {
            return true;
        }
    }
    return false;
}

bool EngineManifestHelper::isModelSupported(const std::string& deviceModel)
{
    const std::string lowerModel = toLowerSpaceless(deviceModel);
    return ManifestStorage::supportedModels.count(lowerModel) > 0 
        || ManifestStorage::partlySupportedModels.count(lowerModel) > 0;
}

bool EngineManifestHelper::isPeaEventSupported(const std::string& eventId)
{
    for (const auto& event : ManifestStorage::supportedPeaEvents)
    {
        if (event.id == eventId)
        {
            return true;
        }
    }
    return false;
}

const std::vector<EventType>& EngineManifestHelper::getSupportedPeaEvents()
{
    return ManifestStorage::supportedPeaEvents;
}

void EngineManifestHelper::clearManifest() {
    ManifestStorage::supportedVendors.clear();
    ManifestStorage::supportedModels.clear();
    ManifestStorage::partlySupportedModels.clear();
    ManifestStorage::supportedPeaEvents.clear();
}

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx