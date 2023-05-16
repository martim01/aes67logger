#pragma once
#include "response.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include "launchmanager.h"
#include <mutex>
#include "session.h"

using postData = std::vector<pml::restgoose::partData>;

class EncoderServer
{
    public:
        EncoderServer();

        int Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response GetEncoders(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetEncoder(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response RedirectToLogin() const;



        void LoopCallback(std::chrono::milliseconds durationSince);

        bool WebsocketAuthenticate(const endpoint& theEndpoint, const query& theQuery, const userName& theUser, const ipAddress& peer);
        bool WebsocketMessage(const endpoint& theEndpoint, const Json::Value& jsData) const;
        void WebsocketClosed(const endpoint& theEndpoint, const ipAddress& peer) const;


        void EncoderCreated(const std::string& sEncoder);
        void EncoderDeleted(const std::string& sEncoder);

        const iniManager& GetConfig() const { return m_config;}

    private:


        pml::restgoose::Server m_server;
        iniManager m_config;
        LaunchManager m_launcher;
        
        void InitLogging();
        bool CreateEndpoints();
        void AddEncoderEndpoints();
        void AddEncoderEndpoints(const std::string& sEncoder);
        void RemoveEncoderEndpoints(const std::string& sEncoder);
        void DeleteEndpoints();
        
        bool AuthenticateToken(const std::string& sToken);
        bool DoAuthenticateToken(const std::string& sToken, const ipAddress& peer);

        time_t GetDateTime(time_t date, const std::vector<std::string>& vLine) const;

        void EncoderCallback(const std::string& sEncoderId, const std::string& sType, bool bAdded);
        void StatusCallback(const std::string& sEncoderId, const Json::Value& jsStatus);
        void ExitCallback(const std::string& sEncoderId, int nExit, bool bRemove);
        
        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_LOGOUT;
        static const endpoint EP_ENCODERS;
        static const endpoint EP_WS;
        static const endpoint EP_WS_ENCODERS;
        
        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGIN;
        static const std::string LOGOUT;
        static const std::string ENCODERS;
        static const std::string STATUS;
        static const std::string WS;

        unsigned int m_nTimeSinceLastCall = 0;

        ssize_t m_nLogToConsole = -1;
        ssize_t m_nLogToFile= -1;
        bool m_bLoggedThisHour = false;

        std::map<ipAddress, std::shared_ptr<SessionCookie>> m_mTokens;

        std::mutex m_mutex;

};
