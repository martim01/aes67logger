#include "log.h"
#include "aes67utils.h"
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <syslog.h>
#include "json/json.h"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>


std::vector<std::string> SplitString(std::string_view str, char cSplit, size_t nMax)
{
    std::vector<std::string> vSplit;
    std::istringstream f(str.data());
    std::string s;

    while (getline(f, s, cSplit))
    {
        if(s.empty() == false)
        {
            if(nMax == 0 || vSplit.size() < nMax)
            {
                vSplit.push_back(s);
            }
            else
            {
                vSplit[nMax-1] = vSplit[nMax-1]+cSplit+s;
            }
        }
    }
    return vSplit;
}

bool CmpNoCase(const std::string& str1, const std::string& str2)
{
    return ((str1.size() == str2.size()) && equal(str1.begin(), str1.end(), str2.begin(), [](char c1, char c2)
    {
        return (c1==c2 || toupper(c1)==toupper(c2));
    }));
}

std::string GetCurrentTimeAsIsoString()
{
    return ConvertTimeToIsoString(std::chrono::system_clock::now());
}

std::string ConvertTimeToIsoString(const std::chrono::time_point<std::chrono::system_clock>& tp)
{
    return ConvertTimeToString(tp, "%FT%T%z");
}

std::string ConvertTimeToString(const std::chrono::time_point<std::chrono::system_clock>& tp, const std::string& sFormat)
{
    std::time_t  t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;

    ss << std::put_time(std::localtime(&t), sFormat.c_str());
    return ss.str();
}


std::string GetIpAddress(const std::string& sInterface)
{
    int fd = socket(AF_INET, SOCK_DGRAM,0);
    ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy((char*)ifr.ifr_ifrn.ifrn_name, sInterface.c_str(), IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    return inet_ntoa((((sockaddr_in*)&ifr.ifr_addr)->sin_addr));

}


std::string Exec(const std::string& sCmd)
{
    std::array<char, 128> buffer;
    std::string sResult;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(sCmd.c_str(), "r"), &pclose);
    if(!pipe)
    {
        return "";
    }
    while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        sResult += buffer.data();
    }
    return rtrim(sResult);
}


std::string& ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int,int>(std::isspace))));
    return s;
}
std::string& rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int,int>(std::isspace))).base(), s.end());
    return s;
}
std::string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}


std::optional<Json::Value> ConvertToJson(const std::string& str)
{
    try
    {
        Json::Value jsData;
        std::stringstream ss;
        ss.str(str);

        ss >> jsData;

        return jsData;
    }
    catch(const Json::RuntimeError& e)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not convert '" << str << "' to JSON: " << e.what();
        return {};
    }
    catch(const Json::LogicError& e)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not convert '" << str << "' to JSON: " << e.what();
        return {};
    }


}


bool CheckJsonMembers(const Json::Value& jsData, const std::map<std::string, enumJsonType>& mMembers)
{
    for(const auto& [sKey, nType] : mMembers)
    {
        if(jsData.isMember(sKey) == false)
        {
            return false;
        }
        switch(nType)
        {
            case enumJsonType::STRING:
                if(jsData[sKey].isString() == false)
                {
                    return false;
                }
                break;
            case enumJsonType::NUMBER:
                if(jsData[sKey].isNumeric() == false)
                {
                    return false;
                }
                break;
            case enumJsonType::OBJECT:
                if(jsData[sKey].isObject() == false)
                {
                    return false;
                }
                break;
            case enumJsonType::ARRAY:
                if(jsData[sKey].isArray() == false)
                {
                    return false;
                }
                break;
            case enumJsonType::BOOLEAN:
                if(jsData[sKey].isBool() == false)
                {
                    return false;
                }
                break;
            case enumJsonType::NULL_:
                if(jsData[sKey].isNull() == false)
                {
                    return false;
                }
                break;
        }
    }
    return true;
}

std::string ExtractValueFromJson(Json::Value jsValue, const std::vector<std::string>& vPath, const std::string& sDefault)
{
    for(const auto& sNode : vPath)
    {
        if(jsValue.isMember(sNode))
        {
            jsValue = jsValue[sNode];
        }
        else
        {
            return sDefault;
        }
    }
    return jsValue.asString();
}

int ExtractValueFromJson(Json::Value jsValue, const std::vector<std::string>& vPath, int nDefault)
{
    for(const auto& sNode : vPath)
    {
        if(jsValue.isMember(sNode))
        {
            jsValue = jsValue[sNode];
        }
        else
        {
            return nDefault;
        }
    }
    if(jsValue.isInt())
    {
        return jsValue.asInt();
    }
    return nDefault;
}


std::optional<std::chrono::time_point<std::chrono::system_clock>> ConvertStringToTimePoint(const std::string& sTime, const std::string& sFormat)
{
    try
    {
        std::tm tm = {};
        std::stringstream ss(sTime);
        ss >> std::get_time(&tm, sFormat.c_str());
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    catch(const std::exception& e)
    {
        return {};
    }
}


std::string GetCurrentTimeAsString(bool bIncludeNano)
{
    std::chrono::time_point<std::chrono::system_clock> tp(std::chrono::system_clock::now());
    return std::to_string(tp.time_since_epoch().count());
}

