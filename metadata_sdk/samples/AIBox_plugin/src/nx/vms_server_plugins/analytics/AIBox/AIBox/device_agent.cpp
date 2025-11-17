// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <ctime>
#include <cstdio>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/kit/debug.h>

#include "device_agent_manifest.h"
#include "object_attributes.h"
#include "ini.h"

#include "../net/subscriber.h"
#include "../net/net_utils.h"
#include "../utils/utils.h"
#include "../utils/log.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static constexpr int kTrackLength = 200;
static constexpr float kMaxBoundingBoxWidth = 0.5F;
static constexpr float kMaxBoundingBoxHeight = 0.5F;
static constexpr float kFreeSpace = 0.1F;
static constexpr int videoWidth = 10000;
static constexpr int videoHeight = 10000;
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

// static inline long long chooseEventTimestampUs(long long eventUs, int timestampShiftMs)
long long DeviceAgent::chooseEventTimestampUs(long long eventUs, int timestampShiftMs)
{
    long long now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    NX_PRINT << "PAKtry nowus:" << now_us
            << "  result.currentTime:" << eventUs
            << "  m_timestampShiftMs:" << timestampShiftMs
            << "  m_lastVideoFrameTimestampUs:" << m_lastVideoFrameTimestampUs;

    // Helper to format microsecond timestamp to human-readable string.
    auto formatTimestampUs = [](long long us) -> std::string
    {
        if (us <= 0)
            return std::string("(invalid)");

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
    };

    NX_PRINT << "PAKtry nowus_human:" << formatTimestampUs(now_us)
            << "  eventUs_human:" << formatTimestampUs(eventUs)
            << "  m_lastVideoFrameTimestampUs_human:" << formatTimestampUs(m_lastVideoFrameTimestampUs);

    return m_lastVideoFrameTimestampUs + static_cast<long long>(timestampShiftMs) * 1000LL;

    const long long kSkewThresholdUs = 10LL * 60 * 1000000; // 10 minutes
    long long absDiff = (now_us > eventUs) ? (now_us - eventUs) : (eventUs - now_us);
    if (absDiff > kSkewThresholdUs || eventUs <= 0)
    {
        NX_PRINT << "Using system time + shift -> "
                 << formatTimestampUs(now_us + static_cast<long long>(timestampShiftMs) * 1000LL);
        return now_us + static_cast<long long>(timestampShiftMs) * 1000LL;
    }

    NX_PRINT << "Using event time + shift -> " << formatTimestampUs(eventUs + static_cast<long long>(timestampShiftMs) * 1000LL);
    return eventUs + static_cast<long long>(timestampShiftMs) * 1000LL;
}

static void parseHostPortFromUrl(const std::string& url, std::string& hostOut, int& portOut)
{
    hostOut.clear();
    portOut = 8080;
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

static Rect genBox(PEAResult result)
{
    nx::sdk::analytics::Rect boundingBox;
    boundingBox.x = static_cast<float>(result.traject.x1) / videoWidth;
    boundingBox.y = static_cast<float>(result.traject.y1) / videoHeight;
    boundingBox.width = static_cast<float>(result.traject.x2 - result.traject.x1) / videoWidth;
    boundingBox.height = static_cast<float>(result.traject.y2 - result.traject.y1) / videoHeight;
    return boundingBox;
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
    m_login = deviceInfo->login();
    m_password = deviceInfo->password();
    m_basicAuth = base64Encode(m_login + ":" + m_password);
    m_deviceUrl = nx::kit::utils::toString(deviceInfo->url());

    NX_PRINT << "DeviceAgent created for device: " << deviceInfo->id();
    NX_PRINT << " login: " << m_login;
    NX_PRINT << " password: " << m_password;
    NX_PRINT << " basicAuth: " << m_basicAuth;
    NX_PRINT << " url: " << deviceInfo->url();

    startSubscription();
    Log::instance().init(1000, true);
}

DeviceAgent::~DeviceAgent()
{
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    if (m_subscriptionStarted)
    {
        Subscriber::stopIpcSubscription();
        m_subscriptionStarted = false;
        NX_PRINT << "DeviceAgent destroyed, unsubscribed from IPC";
    }
    Log::instance().shutdown();
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    m_lastVideoFrameTimestampUs = videoFrame->timestampUs();
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    const std::map<std::string, std::string>& settings = currentSettings();
    m_objectTypeIdsToGenerate.clear();
    for (const auto& entry: settings)
    {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (startsWith(key, kObjectTypeGenerationSettingPrefix) && toBool(value))
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
        Subscriber::registerPEAResultCallback([this](const std::vector<PEAResult>& results)
        {
            this->onPEAResultsReceived(results);
        });

        std::string host;
        int port = 8080;
        parseHostPortFromUrl(m_deviceUrl, host, port);
        Subscriber::startIpcSubscription(host, port, "/SetSubscribe", m_basicAuth);
        m_subscriptionStarted = true;
        NX_PRINT << "IPC Subscription started.";
    }
}

void DeviceAgent::onPEAResultsReceived(const std::vector<PEAResult>& results)
{
    auto metatdataPacket = makePtr<nx::sdk::analytics::ObjectMetadataPacket>();

    long long chosen_ts_us = 0;
    
    for (const auto& result: results)
    {
        std::string objectTypeId;
        if (result.traject.targetType == kStringPerson)
        {
            objectTypeId = kStringHuman;  // "TVT.base.Human" "nx.base.Person"
        }
        else if (result.traject.targetType == kStringCar)
        {
            objectTypeId = kStringMotor;  // "TVT.base.Motorcycle/Bicycle" "nx.base.Bike"
        }
        else if (result.traject.targetType == kStringMotor)
        {
            objectTypeId = kStringUnknown;  // "TVT.base.Unknown" "nx.base.Unknown"
        }
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (m_objectTypeIdsToGenerate.find(objectTypeId) == m_objectTypeIdsToGenerate.end())
            {
                continue;
            }
        }
        auto objectMetadata = makePtr<nx::sdk::analytics::ObjectMetadata>();
        objectMetadata->setTypeId(objectTypeId);
        
        std::string targetIdStr = std::to_string(result.traject.targetId);
        std::string macTail = result.deviceMac;
        std::string uuidStr = "00000000-0000-0000-0000-" + std::string(12 - targetIdStr.size(), '0') + targetIdStr;
        nx::sdk::Uuid trackId = nx::sdk::UuidHelper::fromStdString(uuidStr);
        objectMetadata->setTrackId(trackId);

        objectMetadata->setBoundingBox(genBox(result));

        metatdataPacket->addItem(objectMetadata.get());
         
        chosen_ts_us = chooseEventTimestampUs(static_cast<long long>(result.currentTime), m_timestampShiftMs);
        NX_PRINT << "Chosen event timestamp us: " << chosen_ts_us;
    }

    metatdataPacket->setTimestampUs(chosen_ts_us);
    pushMetadataPacket(metatdataPacket.releasePtr());
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
