#pragma once
#include "response.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include "session.h"

using postData = std::vector<pml::restgoose::partData>;
class PlaybackServer
{
    public:
        PlaybackServer();

        int Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetLoggerFiles(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);


        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response RedirectToLogin();



        void LoopCallback(std::chrono::milliseconds durationSince);

        bool WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer);
        bool WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData);
        void WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer);


    private:


        pml::restgoose::Server m_server;
        iniManager m_config;
        
        void InitLogging();
        bool CreateEndpoints();

        void DeleteEndpoints();
        
        bool AuthenticateToken(const std::string& sToken);
        bool DoAuthenticateToken(const std::string& sToken, const ipAddress& peer);

        time_t GetDateTime(time_t date, const std::vector<std::string>& vLine);

        void EnumLoggers();
        void RemoveAllLoggers();
        void AddLoggerEndpoints(const std::string& sName);

        
        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_LOGOUT;
        static const endpoint EP_LOGGERS;
        
        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGIN;
        static const std::string LOGOUT;
        static const std::string LOGGERS;
        

        unsigned int m_nTimeSinceLastCall;

        int m_nLogToConsole;
        int m_nLogToFile;
        bool m_bLoggedThisHour;

        std::map<ipAddress, std::shared_ptr<SessionCookie>> m_mTokens;

        std::map<std::string, std::shared_ptr<iniManager>> m_mLoggers;


};
