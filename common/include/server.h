#pragma once
#include "response.h"
#include "sysinfomanager.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include "session.h"

using postData = std::vector<pml::restgoose::partData>;
extern pml::restgoose::response ConvertPostDataToJson(const postData& vData);

class Server
{
    public:
        explicit Server(const std::string& sApp);
        virtual ~Server()=default;
        
        int Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        virtual pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const=0;
        virtual pml::restgoose::response GetStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const=0;

        pml::restgoose::response GetConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PatchConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetInfo(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response GetLogs(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response GetPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PutPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PutUpdate(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response RedirectToLogin() const;



        void LoopCallback(std::chrono::milliseconds durationSince);

        bool WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer);
        bool WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData);
        void WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer);

        const iniManager& GetIniManager() const { return m_config;}

    protected:

        virtual void Init()=0;
        virtual void AddCustomEndpoints() = 0;
        virtual void DeleteCustomEndpoints()=0;
        virtual Json::Value GetVersion()const=0;
        
        void InitLogging();
        bool CreateEndpoints();

        void DeleteEndpoints();
        pml::restgoose::response Reboot(int nCommand) const;

        void PatchServerConfig(const Json::Value& jsData) const;
        bool AuthenticateToken(const methodpoint&, const std::string& sToken);
        bool DoAuthenticateToken(const std::string& sToken, const ipAddress& peer);

        time_t GetDateTime(time_t date, const std::vector<std::string>& vLine) const;

        pml::restgoose::response GetLog(const std::string& sLogger, const std::string& sStart, const std::string& sEnd) const;

        virtual Json::Value GetCustomStatusSummary() const=0;
    
       
        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_LOGS;
        static const endpoint EP_STATUS;
        static const endpoint EP_POWER;
        static const endpoint EP_CONFIG;
        static const endpoint EP_INFO;
        static const endpoint EP_UPDATE;
        static const endpoint EP_WS;
        static const endpoint EP_WS_INFO;
        static const endpoint EP_WS_STATUS;

        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGS;
        static const std::string LOGIN;
        static const std::string LOGOUT;
        static const std::string POWER;
        static const std::string CONFIG;
        static const std::string STATUS;
        static const std::string INFO;
        static const std::string UPDATE;
        static const std::string WS;

        
        pml::restgoose::Server& GetServer() { return m_server;}
        std::mutex& GetMutex() { return m_mutex;}

    private:

        pml::restgoose::Server m_server;
        SysInfoManager m_info;
        iniManager m_config;
    
        std::string m_sApp;
        std::mutex m_mutex;

        unsigned int m_nTimeSinceLastCall = 0;
        
        ssize_t m_nLogToConsole = -1;
        ssize_t m_nLogToFile     = -1;
        bool m_bLoggedThisHour = false;

        std::map<ipAddress, std::shared_ptr<SessionCookie>> m_mTokens;
};
