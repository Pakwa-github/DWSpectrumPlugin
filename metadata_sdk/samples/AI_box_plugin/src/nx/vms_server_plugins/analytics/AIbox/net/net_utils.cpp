
#include "net_utils.h"

#include <nx/kit/debug.h>

std::vector<PEAResult> parsePEATrajectoryData(const std::string& xmlData)
{
    std::vector<PEAResult> results;

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError eResult = doc.Parse(xmlData.c_str());
    if (eResult != tinyxml2::XML_SUCCESS)
    {
        NX_PRINT << "Failed to parse XML data: " << doc.ErrorStr();
        return results;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        NX_PRINT << "Invalid XML format: No root element.";
        return results;
    }
    std::string smartType;
    tinyxml2::XMLElement* smartTypeElem = root->FirstChildElement("smartType");
    if (smartTypeElem && smartTypeElem->GetText())
    {
        smartType = smartTypeElem->GetText();
    }
    if (smartType != "PEA")
    {
        NX_PRINT << "Unsupported smartType: " << smartType;
        return results;
    }
    std::string subscribeOption;
    tinyxml2::XMLElement* subscribeOptionElem = root->FirstChildElement("subscribeOption");
    if (subscribeOptionElem && subscribeOptionElem->GetText())
    {
        subscribeOption = subscribeOptionElem->GetText();
    }
    int64_t currentTime = 0;
    tinyxml2::XMLElement* currentTimeElem = root->FirstChildElement("currentTime");
    if (currentTimeElem && currentTimeElem->GetText())
    {
        currentTime = std::stoll(currentTimeElem->GetText());
    }
    std::string deviceMac;
    tinyxml2::XMLElement* deviceMacElem = root->FirstChildElement("deviceMac");
    if (deviceMacElem && deviceMacElem->GetText())
    {
        deviceMac = deviceMacElem->GetText();
    }
    std::string deviceName;
    tinyxml2::XMLElement* deviceNameElem = root->FirstChildElement("deviceName");
    if (deviceNameElem && deviceNameElem->GetText())
    {
        deviceName = deviceNameElem->GetText();
    }

    tinyxml2::XMLElement* trajectoryListElem = root->FirstChildElement("traject");
    if (!trajectoryListElem)
    {
        NX_PRINT << "Not found traject.";
        return results;
    }
    tinyxml2::XMLElement* itemElem = trajectoryListElem->FirstChildElement("item");
    while (itemElem)
    {
        PEAResult result;
        result.smartType = smartType;
        result.subscribeOption = subscribeOption;
        result.currentTime = currentTime;
        result.deviceMac = deviceMac;
        result.deviceName = deviceName;

        tinyxml2::XMLElement* targetIdElem = itemElem->FirstChildElement("targetId");
        if (targetIdElem && targetIdElem->GetText())
        {
            result.traject.targetId = std::stoi(targetIdElem->GetText());
        }
        tinyxml2::XMLElement* targetTypeElem = itemElem->FirstChildElement("targetType");
        if (targetTypeElem && targetTypeElem->GetText())
        {
            result.traject.targetType = targetTypeElem->GetText();
        }

        tinyxml2::XMLElement* rectElem = itemElem->FirstChildElement("rect");
        if (rectElem)
        {
            tinyxml2::XMLElement* x1Elem = rectElem->FirstChildElement("x1");
            if (x1Elem && x1Elem->GetText())
            {
                result.traject.x1 = std::stoi(x1Elem->GetText());
            }
            tinyxml2::XMLElement* y1Elem = rectElem->FirstChildElement("y1");
            if (y1Elem && y1Elem->GetText())
            {
                result.traject.y1 = std::stoi(y1Elem->GetText());
            }
            tinyxml2::XMLElement* x2Elem = rectElem->FirstChildElement("x2");
            if (x2Elem && x2Elem->GetText())
            {
                result.traject.x2 = std::stoi(x2Elem->GetText());
            }
            tinyxml2::XMLElement* y2Elem = rectElem->FirstChildElement("y2");
            if (y2Elem && y2Elem->GetText())
            {
                result.traject.y2 = std::stoi(y2Elem->GetText());
            }
        }

        results.push_back(result);
        itemElem = itemElem->NextSiblingElement("item");
    }

    return results;
}

