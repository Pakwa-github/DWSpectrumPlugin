// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("stub_analytics_plugin_AIbox.ini") { reload(); }

    NX_INI_FLAG(0, enableOutput, "Whether to produce verbose log on stderr.");
    NX_INI_FLAG(0, isLicenseRequired, "Whether the Plugin declares in its manifest that it requires a license.");
    
    
    NX_INI_FLAG(1, isPak, "Try");
    NX_INI_STRING("", aiModelPath, "Path to the AI model file (e.g., model.onnx)");  // 字符串：AI模型路径
    NX_INI_INT(80, confidenceThreshold, "Minimum confidence for AI detection (0-100)");  // 整数：置信度阈值
    NX_INI_FLAG(1, enableAIDetection, "Whether to enable AI detection (1=enable, 0=disable)");
};

Ini& ini();

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
