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

    void onPEAResultsReceived(const std::vector<PEAResult>& results);

    void startSubscription();

private:
    mutable std::mutex m_mutex;

    int m_frameIndex = 0;
    int m_timestampShiftMs = 0;
    bool m_sendAttributes = true;
    std::vector<nx::sdk::Uuid> m_trackIds;
    std::set<std::string> m_objectTypeIdsToGenerate;

private:
    bool m_subscriptionStarted = false; // Tracks whether the subscription is active for this instance
    std::mutex m_subscriptionMutex;

private:
    std::string m_login;
    std::string m_password;
    std::string m_basicAuth;
    std::string m_deviceUrl;
    int64_t m_lastVideoFrameTimestampUs = 0;
    int64_t m_y1;
    int64_t delta;

public:
    long long chooseEventTimestampUs(long long eventUs, int timestampShiftMs);
};

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
