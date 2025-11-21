#pragma once

#include <unordered_set>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

struct EventType
{
    std::string id;
    std::string internalName;
    std::string alarmName;
    bool restricted = false;
    int group = 0;
};

namespace ManifestStorage {
    extern std::unordered_set<std::string> supportedVendors;
    extern std::unordered_set<std::string> supportedModels;
    extern std::unordered_set<std::string> partlySupportedModels;
    extern std::vector<EventType> supportedPeaEvents;
}

class EngineManifestHelper
{
public:
    static bool loadManifest(const std::string& jsonFilePath);
    static bool loadManifests(const std::vector<std::string>& jsonFilePaths);
    static bool isVendorSupported(const std::string& vendor);
    static bool isModelSupported(const std::string& model);
    static bool isPeaEventSupported(const std::string& eventId);
    static const std::vector<EventType>& getSupportedPeaEvents();
    static void clearManifest();

private:
    static std::string toLowerSpaceless(const std::string& str);
    static void parseEvents(const nlohmann::json& eventJsonArray, std::vector<EventType>& outEvents);
};

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx