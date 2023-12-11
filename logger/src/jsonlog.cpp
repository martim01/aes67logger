#include "jsonlog.h"
#include "jsonwriter.h"
#include "jsonconsts.h"

JsonLog::JsonLog(const std::string& sName, int nTimestamp, pml::LogOutput::enumTS eResolution) : pml::LogOutput(nTimestamp, eResolution),
m_sName(sName)
{

}

void JsonLog::Flush(pml::enumLevel eLogLevel, const std::stringstream&  logStream, const std::string& sPrefix)
{
    if(eLogLevel >= m_eLevel)
    {
        Json::Value jsValue;
        jsValue[jsonConsts::id] = m_sName;
        jsValue[jsonConsts::event] = jsonConsts::log;
        jsValue[jsonConsts::data][jsonConsts::timestamp] = Timestamp().str();
        jsValue[jsonConsts::data][jsonConsts::level][jsonConsts::value] = Json::Int(eLogLevel);
        jsValue[jsonConsts::data][jsonConsts::level][jsonConsts::text] = pml::LogStream::STR_LEVEL[eLogLevel];
        jsValue[jsonConsts::data][jsonConsts::message] = logStream.str();
	jsValue[jsonConsts::data]["prefix"] = sPrefix;

        JsonWriter::Get().writeToStdOut(jsValue);

    }
}
