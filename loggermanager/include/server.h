#pragma once
#include "response.h"
#include "sysinfomanager.h"
#include "launchmanager.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include "session.h"

using postData = std::vector<pml::restgoose::partData>;
class Server
{
    public:
        Server();

        void Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLoggers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PostLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetLoggerStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PutLoggerConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PutLoggerPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogger(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);


        pml::restgoose::response GetConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PatchConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetInfo(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetLogs(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PutPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PutUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response RedirectToLogin();



        void StatusCallback(const std::string& sLoggerId, const Json::Value& jsStatus);
        void ExitCallback(const std::string& sLoggerId, int nPid);
        void LoopCallback(std::chrono::milliseconds durationSince);

        bool WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer);
        bool WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData);
        void WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer);


    private:

        void AddLoggerEndpoints(const std::string& sName);

        LaunchManager m_launcher;
        pml::restgoose::Server m_server;
        SysInfoManager m_info;
        iniManager m_config;

        void InitLogging();
        bool CreateEndpoints();

        void DeleteEndpoints();
        pml::restgoose::response Reboot(int nCommand);

        void GetInitialLoggerStatus();

        void PatchServerConfig(const Json::Value& jsData);
        Json::Value m_jsStatus;
        bool AuthenticateToken(const std::string& sToken);
        bool DoAuthenticateToken(const std::string& sToken, const ipAddress& peer);

        time_t GetDateTime(time_t date, const std::vector<std::string>& vLine);

        pml::restgoose::response GetLog(const std::string& sLogger, const std::string& sStart, const std::string& sEnd);

         /**
        x-epi                               GET
        x-epi/status                        GET PUT
        x-epi/files                         GET     POST
        x-epi/files/xxx                     GET PUT
        x-epi/playlists                     GET     POST
        x-epi/playlists/xxx                 GET PUT
        x-epi/schedules                     GET     POST
        x-epi/schedules/xxx                 GET PUT
        x-epi/power                         GET PUT
        x-epi/config                        GET PUT
        x-epi/info                          GET
        **/

        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_LOGS;
        static const endpoint EP_LOGOUT;
        static const endpoint EP_LOGGERS;
        static const endpoint EP_STATUS;
        static const endpoint EP_POWER;
        static const endpoint EP_CONFIG;
        static const endpoint EP_INFO;
        static const endpoint EP_UPDATE;
        static const endpoint EP_WS;
        static const endpoint EP_WS_LOGGERS;
        static const endpoint EP_WS_INFO;
        static const endpoint EP_WS_STATUS;

        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGS;
        static const std::string LOGIN;
        static const std::string LOGOUT;
        static const std::string LOGGERS;
        static const std::string POWER;
        static const std::string CONFIG;
        static const std::string STATUS;
        static const std::string INFO;
        static const std::string UPDATE;
        static const std::string WS;

        std::mutex m_mutex;

        unsigned int m_nTimeSinceLastCall;

        int m_nLogToConsole;
        int m_nLogToFile;
        bool m_bLoggedThisHour;

        std::map<ipAddress, std::shared_ptr<SessionCookie>> m_mTokens;
};
