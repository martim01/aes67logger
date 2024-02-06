#pragma once
#include "response.h"
#include "json/json.h"
#include "RestGoose.h"
#include "inimanager.h"
#include <set>
#include <optional>
#include <random>
#include "jwt-cpp/jwt.h"

using postData = std::vector<pml::restgoose::partData>;
extern std::optional<Json::Value> ConvertPostDataToJson(const postData& vData);

class WebServer
{
    public:
        WebServer();
        virtual ~WebServer()=default;
        
        int Run(const std::string& sConfigFile);



        pml::restgoose::response GetRoot(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetUsers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetPermissions(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PostUser(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PatchUsers(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);
        pml::restgoose::response PostLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response DeleteLogin(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        
    protected:

        void InitLogging();
        bool CreateEndpoints();

        void DeleteEndpoints();

        pml::restgoose::response CreateJwt(const std::string& sUsername) const;

        bool AuthenticateUser(const std::string& sUsername, const std::string& sPassword) const;
        
        bool AuthenticateToken(const methodpoint& thePoint, const std::string& sToken) const;
        
        void SaveUserPermission(const std::string& sSection, const std::string& sSetting, const Json::Value& jsData, bool bDefault);

        bool GetPermission(const jwt::decoded_jwt<jwt::traits::kazuho_picojson>& decoded, const std::string& sPermission) const;

        static const endpoint EP_ROOT;
        static const endpoint EP_API;
        static const endpoint EP_LOGIN;
        static const endpoint EP_USERS;
        static const endpoint EP_PERMISSIONS;
        
        static const std::string ROOT;
        static const std::string API;
        static const std::string LOGIN;
        static const std::string USERS;
        static const std::string PERMISSIONS;
        static const std::string HASH_KEY;

        
    private:

        pml::restgoose::Server m_server;
        iniManager m_config;
    
        
        unsigned int m_nTimeSinceLastCall = 0;
        
        ssize_t m_nLogToConsole = -1;
        ssize_t m_nLogToFile     = -1;
        bool m_bLoggedThisHour = false;

        std::random_device m_rd;
        std::default_random_engine m_el;

};
