// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <thread>
#include <vector>
#include <mutex>
#include <nx/sdk/analytics/helpers/object_metadata.h>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"
#include "../net/net_utils.h"
#include "../net/subscriber.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    static const std::string kTimeShiftSetting;
    static const std::string kSendAttributesSetting;
    static const std::string kObjectTypeGenerationSettingPrefix;

public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushCompressedVideoFrame(
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::Uuid trackIdByTrackIndex(int trackIndex);

    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket(
        int64_t frameTimestampUs);

    void onPEAResultReceived(const PEAResult& result);

    void startSubscription();
    void stopSubscription();

private:
    mutable std::mutex m_mutex;
    int m_frameIndex = 0;
    int m_timestampShiftMs = 0;
    bool m_sendAttributes = true;
    std::vector<nx::sdk::Uuid> m_trackIds;
    std::set<std::string> m_objectTypeIdsToGenerate;

private:
    bool m_subscriptionStarted = false;
    std::mutex m_subscriptionMutex;
    Subscriber m_subscriber;

private:
    std::string m_login;
    std::string m_password;
    std::string m_basicAuth;
    std::string m_deviceUrl;
    int64_t m_lastVideoFrameTimestampUs = 0;
    int64_t m_y1;
    int64_t delta;

private:
    std::mutex m_timeSyncMutex;
    bool m_havePeaAnchor = false;
    int64_t m_anchorPeaTs = 0;
    int64_t m_anchorVideoTs = 0;
    static constexpr int64_t kAnchorMaxDriftUs = 10LL * 1000000; // 10 seconds
    int64_t PEATimestampUs(int64_t peaTs, int timestampShiftMs);
};

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
