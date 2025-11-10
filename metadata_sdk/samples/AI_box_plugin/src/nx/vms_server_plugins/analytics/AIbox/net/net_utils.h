// net_utils.h
#pragma once

#include <string>
#include <vector>

#include <tinyxml2/tinyxml2.h>

struct TrajectoryResult
{
    int targetId;
    int x1;
    int y1;
    int x2;
    int y2;
    std::string targetType;
};

// one trajectory result
struct PEAResult
{
    std::string smartType;
    std::string subscribeOption;
    int64_t currentTime;
    std::string deviceMac;
    std::string deviceName;
    TrajectoryResult traject;
};


std::vector<PEAResult> parsePEATrajectoryData(const std::string& xmlData);