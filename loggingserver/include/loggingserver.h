#pragma once
#include "launchmanager.h"
#include "server.h"

class LoggingServer : public Server
{
    public:
        LoggingServer();


        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;
        pml::restgoose::response GetSources(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response GetStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;

        pml::restgoose::response GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PostLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLoggerStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PatchLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PutLoggerPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);


        void StatusCallback(const std::string& sLoggerId, const Json::Value& jsStatus);
        void ExitCallback(const std::string& sLoggerId, int nPid, bool bRemove);


    private:

        void Init() final;
        void AddCustomEndpoints() final;
        void DeleteCustomEndpoints() final;
        Json::Value GetVersion() const final;
        Json::Value GetCustomStatusSummary() const final;

        void AddLoggerEndpoints(const std::string& sName);
        void ReadDiscoveryConfig();

        LaunchManager m_launcher;
        iniManager m_discovery;

        Json::Value GetDiscoveredRtspSources() const;
        Json::Value GetDiscoveredSdpSources() const;
        
        static const endpoint EP_LOGGERS;
        static const endpoint EP_STATUS;
        static const endpoint EP_SOURCES;
        static const endpoint EP_WS_LOGGERS;
        
        static const std::string LOGGERS;
        static const std::string SOURCES;
        
};
