#pragma once
#include "log.h"

class JsonLog : public pml::LogOutput
{
    public:
        /** @brief Constructor
        *   @param sRootPath - the root path that the log files should live in.
        *   @param nTimestamp - the format of the timestamp that gets written in to the log
        *   @param eResolution - the resolution of the timestamp
        **/
        JsonLog(const std::string& sName, int nTimestamp=TS_TIME, pml::LogOutput::enumTS eResolution=TSR_MILLISECOND);
        ~JsonLog() final =default ;

        /** @brief Called by the LogStream when it needs to be flushed - should not be called directly
        *   @param eLogLevel the level of the current message that is being flushed
        *   @param logStream the current message
        **/
        void Flush(pml::enumLevel eLogLevel, const std::stringstream&  logStream, const std::string& sPrefix) override;

    private:

        std::string m_sName;
};
