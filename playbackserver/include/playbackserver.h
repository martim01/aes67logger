#pragma once
#include "response.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include "session.h"
#include "loggermanager.h"
#include "observer.h"

using postData = std::vector<pml::restgoose::partData>;
class LoggerObserver;


class PlaybackServer
{
    public:
        PlaybackServer();

        int Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetLoggerFiles(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response DownloadLoggerFile(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;


        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response RedirectToLogin() const;



        void LoopCallback(std::chrono::milliseconds durationSince);

        bool WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer);
        bool WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData) const;
        void WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer) const;


        void LoggerCreated(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger);
        void LoggerDeleted(const std::string& sLogger, std::shared_ptr<LoggerObserver> pLogger);

        void FileCreated(const std::string& sLogger, const std::filesystem::path& path);
        void FileDeleted(const std::string& sLogger, const std::filesystem::path& path);

        const iniManager& GetConfig() const { return m_config;}

    private:


        pml::restgoose::Server m_server;
        iniManager m_config;
        
        void InitLogging();
        bool CreateEndpoints();

        void DeleteEndpoints();
        
        bool AuthenticateToken(const std::string& sToken);
        bool DoAuthenticateToken(const std::string& sToken, const ipAddress& peer);

        time_t GetDateTime(time_t date, const std::vector<std::string>& vLine) const;

        void AddLoggerEndpoints();
        void AddLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger);
        void RemoveLoggerEndpoints(const std::string& sName, std::shared_ptr<LoggerObserver> pLogger);
        
        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_LOGOUT;
        static const endpoint EP_LOGGERS;
        static const endpoint EP_WS;
        static const endpoint EP_WS_LOGGERS;
        
        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGIN;
        static const std::string LOGOUT;
        static const std::string LOGGERS;
        static const std::string DOWNLOAD;
        static const std::string WS;

        unsigned int m_nTimeSinceLastCall = 0;

        ssize_t m_nLogToConsole = -1;
        ssize_t m_nLogToFile= -1;
        bool m_bLoggedThisHour = false;

        std::unique_ptr<LoggerManager> m_pManager;

        std::map<ipAddress, std::shared_ptr<SessionCookie>> m_mTokens;

        std::map<std::string, std::shared_ptr<iniManager>> m_mLoggers;


};
