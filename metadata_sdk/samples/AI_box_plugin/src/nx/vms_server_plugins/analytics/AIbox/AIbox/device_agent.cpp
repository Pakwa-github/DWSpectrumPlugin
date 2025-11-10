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
#include "stub_analytics_plugin_AIbox_ini.h"

#include "../net/subscriber.h"
#include "../utils/utils.h"
#include "../utils/log.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {

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

static inline long long chooseEventTimestampUs(long long eventUs, int timestampShiftMs)
{
    long long now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    NX_PRINT << "PAKtry nowus:" << now_us
            << "  result.currentTime:" << eventUs
            << "  m_timestampShiftMs:" << timestampShiftMs;

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
            << "  eventUs_human:" << formatTimestampUs(eventUs);

    // If device time is wildly different from system time (e.g. > 10 min), treat
    // device timestamp as unreliable and use current system time for display.

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

static Rect genBox(PEAResult result)
{
    nx::sdk::analytics::Rect boundingBox;
    boundingBox.x = static_cast<float>(result.traject.x1) / videoWidth;
    boundingBox.y = static_cast<float>(result.traject.y1) / videoHeight;
    boundingBox.width = static_cast<float>(result.traject.x2 - result.traject.x1) / videoWidth;
    boundingBox.height = static_cast<float>(result.traject.y2 - result.traject.y1) / videoHeight;
    return boundingBox;
}

static Rect genStaticBox()
{
    Rect boundingBox;
    boundingBox.width = 0.2F;
    boundingBox.height = 0.2F;
    boundingBox.x = (1.0F - boundingBox.width) / 2.0F;
    boundingBox.y = (1.0F - boundingBox.height) / 2.0F;
    return boundingBox;
}

static Rect generateBoundingBox(int frameIndex, int trackIndex, int trackCount)
{
    
    if (0)
    {
        Rect boundingBox;
        boundingBox.width = 0.2F;
        boundingBox.height = 0.2F;
        
        boundingBox.x = (1.0F - boundingBox.width) / 2.0F;
        boundingBox.y = (1.0F - boundingBox.height) / 2.0F;

        return boundingBox;
    }

    Rect boundingBox;
    boundingBox.width = std::min((1.0F - kFreeSpace) / trackCount, kMaxBoundingBoxWidth);  // min(0.9/count ,0.5)
    boundingBox.height = std::min(boundingBox.width, kMaxBoundingBoxHeight);               // min(width, 0.5)
    boundingBox.x = 1.0F / trackCount * trackIndex + kFreeSpace / (trackCount + 1);        // idx/count + 0.1 /(count+1)
    boundingBox.y = std::max(
        0.0F,
        1.0F - boundingBox.height - (1.0F / kTrackLength) * (frameIndex % kTrackLength));   // 1-height - frame/40000 > 0

    return boundingBox;
}

static std::vector<Ptr<ObjectMetadata>> generateObjects(
    const std::map<std::string, std::map<std::string, std::string>>& attributesByObjectType,
    const std::set<std::string>& objectTypeIdsToGenerate,
    bool doGenerateAttributes)
{
    std::vector<Ptr<ObjectMetadata>> result;

    for (const auto& entry: attributesByObjectType)
    {
        const std::string& objectTypeId = entry.first;
        if (objectTypeIdsToGenerate.find(objectTypeId) == objectTypeIdsToGenerate.cend())
            continue;

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(objectTypeId);

        if (doGenerateAttributes)
        {
            const std::map<std::string, std::string>& attributes = entry.second;
            for (const auto& attribute: attributes)
                objectMetadata->addAttribute(makePtr<Attribute>(attribute.first, attribute.second));
        }

        result.push_back(std::move(objectMetadata));
    }

    return result;
}

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket(int64_t frameTimestampUs)
{
    auto metadataPacket = makePtr<ObjectMetadataPacket>();
    metadataPacket->setTimestampUs(frameTimestampUs);

    std::vector<Ptr<ObjectMetadata>> objects;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        objects = generateObjects(kObjectAttributes, m_objectTypeIdsToGenerate, false);  // TVT: no send attributes
    }

    for (int i = 0; i < (int) objects.size(); ++i)
    {
        objects[i]->setBoundingBox(generateBoundingBox(m_frameIndex, i, (int)objects.size()));
        objects[i]->setTrackId(trackIdByTrackIndex(i));

        metadataPacket->addItem(objects[i].get());
    }

    return metadataPacket;
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
    NX_PRINT << "DeviceAgent created for device: " << deviceInfo->id();
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
    ++m_frameIndex;
    if ((m_frameIndex % kTrackLength) == 0)
        m_trackIds.clear();

    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(
        videoFrame->timestampUs() + m_timestampShiftMs * 1000);

    pushMetadataPacket(objectMetadataPacket.releasePtr());

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

    m_objectTypeIdsToGenerate.clear();

    const std::map<std::string, std::string>& settings = currentSettings();
    for (const auto& entry: settings)
    {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (startsWith(key, kObjectTypeGenerationSettingPrefix) && toBool(value))
            m_objectTypeIdsToGenerate.insert(key.substr(kObjectTypeGenerationSettingPrefix.size()));
        else if (key == kSendAttributesSetting)
            m_sendAttributes = toBool(value);
        else if (key == kTimeShiftSetting)
            m_timestampShiftMs = std::stoi(value);
    }

    return nullptr;
}

Uuid DeviceAgent::trackIdByTrackIndex(int trackIndex)
{
    while (trackIndex >= m_trackIds.size())
        m_trackIds.push_back(UuidHelper::randomUuid());

    return m_trackIds[trackIndex];
}

void DeviceAgent::onPEAResultsReceived(const std::vector<PEAResult>& results)
{
    for (const auto& result: results)
    {
        
        auto metatdataPacket = makePtr<nx::sdk::analytics::ObjectMetadataPacket>();

        long long chosen_ts_us = chooseEventTimestampUs(static_cast<long long>(result.currentTime), m_timestampShiftMs);
        metatdataPacket->setTimestampUs(chosen_ts_us);

        auto objectMetadata = makePtr<nx::sdk::analytics::ObjectMetadata>();

        objectMetadata->setTypeId("nx.dw_tvt.PEA.perimeterAlarm");
        // objectMetadata->setTypeId("nx.base.Person");

        std::string targetIdStr = std::to_string(result.traject.targetId);
        std::string uuidStr = "00000000-0000-0000-0000-" + std::string(12 - targetIdStr.size(), '0') + targetIdStr;
        nx::sdk::Uuid trackId = nx::sdk::UuidHelper::fromStdString(uuidStr);
        objectMetadata->setTrackId(trackId);

        // objectMetadata->setBoundingBox(genStaticBox());

        objectMetadata->setBoundingBox(genBox(result));

    // Log normalized bounding box values for easier debugging on the server.
    // const nx::sdk::analytics::Rect& nb = objectMetadata->boundingBox();
    // LOG_PRINT("box pushed from PEA result, targetId=" << result.traject.targetId
    //     << ", raw_box=(" << result.traject.x1 << "," << result.traject.y1 << ","
    //     << result.traject.x2 << "," << result.traject.y2 << ")"
    //     << ", norm_box=(" << nb.x << "," << nb.y << "," << nb.width << "," << nb.height << ")");

        metatdataPacket->addItem(objectMetadata.get());
        pushMetadataPacket(metatdataPacket.releasePtr());
    }
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

        Subscriber::startIpcSubscription("10.1.60.137", 8080, "/SetSubscribe");
        m_subscriptionStarted = true;
        NX_PRINT << "IPC Subscription started.";
    }
}

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
