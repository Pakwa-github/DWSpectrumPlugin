// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include "engine_manifest.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

class Engine: public nx::sdk::analytics::Engine
{
public:
    Engine();
    virtual ~Engine() override;

public:
    void initialize(nx::sdk::analytics::Plugin* plugin);
    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual std::string manifestString() const override;

protected:
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    void obtainPluginHomeDir();
    void loadCompatibleManifests();

private:
    nx::sdk::analytics::Plugin* m_plugin = nullptr;
    std::string m_pluginHomeDir;
    std::vector<std::string> m_manifestPaths;
};

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
