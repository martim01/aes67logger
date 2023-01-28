#pragma once
#include <string>
#include <vector>
#include <sys/stat.h>
#include <chrono>
#include <queue>
#include "json/json.h"
#include <optional>

extern std::vector<std::string> SplitString(std::string str, char cSplit, size_t nMax=0);

extern std::string& ltrim(std::string& s);
extern std::string& rtrim(std::string& s);
extern std::string& trim(std::string& s);

extern bool CmpNoCase(const std::string& str1, const std::string& str2);


extern std::string ConvertTimeToIsoString(std::time_t theTime);
extern std::string ConvertTimeToString(std::chrono::time_point<std::chrono::system_clock> tp, bool bIncludeNano=true);
extern std::string ConvertTimeToIsoString(std::chrono::time_point<std::chrono::system_clock> tp);
extern std::string GetCurrentTimeAsString(bool bIncludeNano=true);
extern std::string GetCurrentTimeAsIsoString();



extern std::string GetIpAddress(const std::string& sInterface);
extern std::string Exec(const std::string& sCmd);


extern std::optional<Json::Value> ConvertToJson(const std::string& str);

enum enumJsonType{STRING, NUMBER, OBJECT, ARRAY, BOOLEAN, NULL_};

extern bool CheckJsonMembers(const Json::Value& jsData, const std::map<std::string, enumJsonType>& mMembers);


extern int ExtractValueFromJson(Json::Value jsValue, const std::vector<std::string>& vPath, int nDefault);
extern std::string ExtractValueFromJson(Json::Value jsValue, const std::vector<std::string>& vPath, const std::string& sDefault);
