// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#define NX_DEBUG_ENABLE_OUTPUT true
#include <nx/kit/debug.h>

#include "device_agent.h"

#include <chrono>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "device_agent_manifest.h"
#include "object_attributes.h"
#include "../utils.h"
#include "stub_analytics_plugin_AIbox_ini.h"

#include "../net/subscriber.h"

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
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

static Rect generateBoundingBox(int frameIndex, int trackIndex, int trackCount)
{
    
    if (1)
    {
        Rect boundingBox;
        // 1. 固定矩形宽高（占画面20%，可根据需要调整，比如0.3=30%）
        boundingBox.width = 0.2F;  // 宽度：20%
        boundingBox.height = 0.2F; // 高度：20%（和宽度一致，是正方形；要长方形可改这里，比如0.1）
        
        // 2. 计算中心坐标：(1-宽)/2 确保x居中，(1-高)/2 确保y居中
        boundingBox.x = (1.0F - boundingBox.width) / 2.0F;  // x轴居中
        boundingBox.y = (1.0F - boundingBox.height) / 2.0F; // y轴居中

        // （原函数的frameIndex、trackIndex等动态参数不再使用，直接返回固定坐标）

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

    // NX_PRINT << "Pak33";
    // NX_PRINT << __FILE__ << " " << __LINE__ << " " << __func__;
    // TODO
    

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
    NX_PRINT << "Subscription started in DeviceAgent constructor.";
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
        int videoWidth = 1920;  // TODO: get from device info or video frame
        int videoHeight = 1080;
        auto metatdataPacket = makePtr<nx::sdk::analytics::ObjectMetadataPacket>();
        metatdataPacket->setTimestampUs(result.currentTime);

        auto objectMetadata = makePtr<nx::sdk::analytics::ObjectMetadata>();
        objectMetadata->setTypeId("nx.dw_tvt.PEA.perimeterAlarm");
        std::string targetIdStr = std::to_string(result.traject.targetId);
        std::string uuidStr = "00000000-0000-0000-0000-" + std::string(12 - targetIdStr.size(), '0') + targetIdStr;
        nx::sdk::Uuid trackId = nx::sdk::UuidHelper::fromStdString(uuidStr);
        objectMetadata->setTrackId(trackId);

        nx::sdk::analytics::Rect boundingBox;
        boundingBox.x = static_cast<float>(result.traject.x1) / videoWidth;
        boundingBox.y = static_cast<float>(result.traject.y1) / videoHeight;
        boundingBox.width = static_cast<float>(result.traject.x2 - result.traject.x1) / videoWidth;
        boundingBox.height = static_cast<float>(result.traject.y2 - result.traject.y1) / videoHeight;
        objectMetadata->setBoundingBox(boundingBox);

        metatdataPacket->addItem(objectMetadata.get());

        pushMetadataPacket(metatdataPacket.releasePtr());
        NX_PRINT << "box pushed from PEA result, targetId=" << result.traject.targetId << ", box=("
                << result.traject.x1 << "," << result.traject.y1 << "," << result.traject.x2 << "," << result.traject.y2 << ")";

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
