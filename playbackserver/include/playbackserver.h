#pragma once
#include "server.h"
#include "loggermanager.h"
#include "observer.h"

using postData = std::vector<pml::restgoose::partData>;
class LoggerObserver;


class PlaybackServer : public Server
{
    public:
        PlaybackServer();

        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;
        pml::restgoose::response GetStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;

        pml::restgoose::response GetDashboard(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLoggerFiles(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response DownloadLoggerFile(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;


        void LoggerCreated(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger);
        void LoggerDeleted(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger);

        void FileCreated(const std::string& sLogger, const std::filesystem::path& path);
        void FileDeleted(const std::string& sLogger, const std::filesystem::path& path);

        void DownloadFileMessage(const std::string& sId, unsigned int nHttpCode, const std::string& sMessage);
        void DownloadFileProgress(const Json::Value& jsProgress);
        void DownloadFileDone(const std::string& sId, const std::string& sLocation);


    private:

        void Init() final;
        void AddCustomEndpoints() final;
        void DeleteCustomEndpoints() final;
        Json::Value GetVersion() const final;
        Json::Value GetCustomStatusSummary() const final;

        void AddLoggerEndpoints();
        void AddLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger);
        void RemoveLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger);

        static const endpoint EP_LOGGERS;
        static const endpoint EP_WS_LOGGERS;
        static const endpoint EP_DASHBOARD;
        static const endpoint EP_WS_DOWNLOAD;
        static const std::string LOGGERS;
        static const std::string DOWNLOAD;
        static const std::string DASHBOARD;

        std::unique_ptr<LoggerManager> m_pManager;
        std::map<std::string, std::shared_ptr<iniManager>> m_mLoggers;


};
