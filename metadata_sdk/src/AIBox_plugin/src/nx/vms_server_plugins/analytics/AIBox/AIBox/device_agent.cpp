// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/kit/debug.h>

#include "device_agent_manifest.h"
#include "ini.h"

#include "../net/subscriber.h"
#include "../net/net_utils.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static constexpr float kVideoWidth = 10000.0f;
static constexpr float kVideoHeight = 10000.0f;
static constexpr int kPort = 8080;
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

static void parseHostPortFromUrl(const std::string& url, std::string& hostOut)
{
    hostOut.clear();
    const std::string::size_type schemePos = url.find("://");
    std::string after = (schemePos == std::string::npos) ? url : url.substr(schemePos + 3);
    auto slashPos = after.find('/');
    std::string hostPort = (slashPos == std::string::npos) ? after : after.substr(0, slashPos);
    auto atPos = hostPort.rfind('@');
    if (atPos != std::string::npos) hostPort = hostPort.substr(atPos + 1);
    auto colonPos = hostPort.find(':');
    if (colonPos != std::string::npos)
    {
        hostOut = hostPort.substr(0, colonPos);
    }
    else
    {
        hostOut = hostPort;
    }
    // trim spaces
    hostOut.erase(hostOut.begin(), std::find_if(hostOut.begin(), hostOut.end(), [](int ch){ return !std::isspace(ch); }));
    hostOut.erase(std::find_if(hostOut.rbegin(), hostOut.rend(), [](int ch){ return !std::isspace(ch); }).base(), hostOut.end());
}

static std::string formatTimestampUs(long long us)
{
    if (us <= 0)
    {
        return std::string("(invalid)");
    }
    std::time_t sec = static_cast<std::time_t>(us / 1000000LL);
    int micro = static_cast<int>(us % 1000000LL);
    std::tm tm{};
#if defined(_MSC_VER)
    localtime_s(&tm, &sec);
#else
    localtime_r(&sec, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    char out[80];
    std::snprintf(out, sizeof(out), "%s.%06d", buf, micro);
    return std::string(out);
}

static nx::sdk::Uuid makeTrackUuid(const std::string& mac, int targetId)
{
    std::string macNoColons;
    macNoColons.reserve(mac.size());
    for (char ch: mac)
    {
        if (ch == ':') 
        {
            continue;
        }
        if (std::isxdigit(static_cast<unsigned char>(ch)))
        {
            macNoColons.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    std::string macHex = macNoColons.substr(0, 12);
    if (macHex.size() < 12)
    {
        macHex += std::string(12 - macHex.size(), '0');
    }
        
    std::ostringstream ss;
    ss << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << targetId;
    std::string targetHex = ss.str();

    std::string fullHex = std::string(8, '0') + macHex + std::string(4, '0') + targetHex;
    std::stringstream uuidSs;
    uuidSs << fullHex.substr(0, 8) << "-"
       << fullHex.substr(8, 4) << "-"
       << fullHex.substr(12, 4) << "-"
       << fullHex.substr(16, 4) << "-"
       << fullHex.substr(20, 12);
    std::string uuidStr = uuidSs.str();
    return nx::sdk::UuidHelper::fromStdString(uuidStr);
}

static Rect genBox(const TrajectoryResult& traject)
{
    nx::sdk::analytics::Rect boundingBox;
    boundingBox.x = static_cast<float>(traject.x1) / kVideoWidth;
    boundingBox.y = static_cast<float>(traject.y1) / kVideoHeight;
    boundingBox.width = static_cast<float>(traject.x2 - traject.x1) / kVideoWidth;
    boundingBox.height = static_cast<float>(traject.y2 - traject.y1) / kVideoHeight;
    return boundingBox;
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
    m_login = deviceInfo->login();
    m_password = deviceInfo->password();
    m_basicAuth = base64Encode(m_login + ":" + m_password);
    m_deviceUrl = nx::kit::utils::toString(deviceInfo->url());

    NX_PRINT << "DeviceAgent created for device: " << deviceInfo->vendor() << " " << deviceInfo->model();

    startSubscription();
}

DeviceAgent::~DeviceAgent()
{
    stopSubscription();
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    {
        std::lock_guard<std::mutex> lock(m_frameTimeMutex);
        m_lastVideoFrameTimestampUs = videoFrame->timestampUs();
    }    
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::map<std::string, std::string>& settings = currentSettings();
    m_objectTypeIdsToGenerate.clear();
    for (const auto& entry: settings)
    {
        const std::string& key = entry.first;
        std::string value = entry.second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if ((0 == key.rfind(kObjectTypeGenerationSettingPrefix, 0)) && (value == "true" || value == "1"))
        {
            m_objectTypeIdsToGenerate.insert(key.substr(kObjectTypeGenerationSettingPrefix.size()));
        }
        else if (key == kTimeShiftSetting)
        {
            m_timestampShiftMs = std::stoi(value);
        }
    }
    return nullptr;
}

void DeviceAgent::startSubscription()
{
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    if (!m_subscriptionStarted)
    {
        m_subscriber.registerPEAResultCallback([this](const PEAResult& result)
        {
            this->onPEAResultReceived(result);
        });

        std::string host;
        parseHostPortFromUrl(m_deviceUrl, host);
        NX_PRINT << "IPC Subscription starting...";
        m_subscriber.startIpcSubscription(host, kPort, "/SetSubscribe", m_basicAuth);
        m_subscriptionStarted = true;
    }
}

void DeviceAgent::stopSubscription()
{
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    if (m_subscriptionStarted)
    {
        m_subscriber.stopIpcSubscription();
        m_subscriptionStarted = false;
        NX_PRINT << "IPC Subscription stopped.";
    }
}

void DeviceAgent::onPEAResultReceived(const PEAResult& result)
{
    int64_t frameTimestampUs = 0;
    int64_t timestampShiftMs = 0;
    {
        std::lock_guard<std::mutex> lock(m_frameTimeMutex);
        frameTimestampUs = m_lastVideoFrameTimestampUs;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        timestampShiftMs = m_timestampShiftMs;
    }

    if (frameTimestampUs <= 0)
    {
        return;
    }
    
    auto metadataPacket = makePtr<nx::sdk::analytics::ObjectMetadataPacket>();
    
    // currentTime
    metadataPacket->setTimestampUs(frameTimestampUs + (static_cast<int64_t>(timestampShiftMs) * 1000LL));

    // Mac
    std::string macTail = result.deviceMac;

    // trajects
    for (const auto& traject: result.trajects)
    {
        std::string objectTypeId;
        if (traject.targetType == kStringPerson)
        {
            objectTypeId = kStringHuman;
        }
        else if (traject.targetType == kStringCar)
        {
            objectTypeId = kStringMotorVehicle;
        }
        else if (traject.targetType == kStringMotor)
        {
            objectTypeId = kStringMotorcycleBicycle;
        }
        else
        {
            objectTypeId = kStringUnknown;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_objectTypeIdsToGenerate.find(objectTypeId) == m_objectTypeIdsToGenerate.end())
            {
                continue;
            }
        }
        
        auto objectMetadata = makePtr<nx::sdk::analytics::ObjectMetadata>();

        // targetType
        objectMetadata->setTypeId(objectTypeId);

        nx::sdk::Uuid trackId = makeTrackUuid(result.deviceMac, traject.targetId);
        objectMetadata->setTrackId(trackId);

        // rect
        objectMetadata->setBoundingBox(genBox(traject));

        metadataPacket->addItem(objectMetadata.get());
    }

    pushMetadataPacket(metadataPacket.releasePtr());
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
