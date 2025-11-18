
#include "net_utils.h"

#include <nx/kit/debug.h>

PEAResult parsePEATrajectoryData(const std::string& xmlData)
{
    PEAResult result{};

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError e = doc.Parse(xmlData.c_str());
    if (e != tinyxml2::XML_SUCCESS)
    {
        NX_PRINT << "Failed to parse XML data: " << doc.ErrorStr();
        return result;
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
    {
        NX_PRINT << "Invalid XML format: No root element.";
        return result;
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
        return result;
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
    tinyxml2::XMLElement* deviceMacElem = root->FirstChildElement("mac");
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
        return result;
    }
    result.smartType = smartType;
    result.subscribeOption = subscribeOption;
    result.currentTime = currentTime;
    result.deviceMac = deviceMac;
    result.deviceName = deviceName;

    tinyxml2::XMLElement* itemElem = trajectoryListElem->FirstChildElement("item");
    while (itemElem)
    {
        TrajectoryResult tr{};
        tinyxml2::XMLElement* targetTypeElem = itemElem->FirstChildElement("targetType");
        if (targetTypeElem && targetTypeElem->GetText())
            tr.targetType = targetTypeElem->GetText();

        tinyxml2::XMLElement* targetIdElem = itemElem->FirstChildElement("targetId");
        if (targetIdElem && targetIdElem->GetText())
            tr.targetId = std::stoi(targetIdElem->GetText());

        tinyxml2::XMLElement* rectElem = itemElem->FirstChildElement("rect");
        if (rectElem)
        {
            tinyxml2::XMLElement* x1Elem = rectElem->FirstChildElement("x1");
            if (x1Elem && x1Elem->GetText())
            {
                tr.x1 = std::stoi(x1Elem->GetText());
            }
            tinyxml2::XMLElement* y1Elem = rectElem->FirstChildElement("y1");
            if (y1Elem && y1Elem->GetText())
            {
                tr.y1 = std::stoi(y1Elem->GetText());
            }
            tinyxml2::XMLElement* x2Elem = rectElem->FirstChildElement("x2");
            if (x2Elem && x2Elem->GetText())
            {
                tr.x2 = std::stoi(x2Elem->GetText());
            }
            tinyxml2::XMLElement* y2Elem = rectElem->FirstChildElement("y2");
            if (y2Elem && y2Elem->GetText())
            {
                tr.y2 = std::stoi(y2Elem->GetText());
            }
        }

        result.trajects.push_back(std::move(tr));
        itemElem = itemElem->NextSiblingElement("item");
    }
    return result;
}


static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
std::string base64Encode(const std::string& in)
{
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
    {
        out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4)
    {
        out.push_back('=');
    }
    return out;
}
