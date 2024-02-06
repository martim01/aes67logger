#include "webserver.h"
#include "json/json.h"
#include <sstream>
#include "log.h"
#include "logtofile.h"
#include "aes67utils.h"
#include "jsonconsts.h"
#include "jwt-cpp/jwt.h"


using namespace std::placeholders;

const std::string WebServer::ROOT        = "/";             //GET
const std::string WebServer::API         = "x-api";          //GET
const std::string WebServer::LOGIN       = "login";
const std::string WebServer::USERS       = "users";
const std::string WebServer::PERMISSIONS       = "permissions";

const std::string WebServer::HASH_KEY    = "PPcBf0tOC7aWvcu";

const endpoint WebServer::EP_ROOT        = endpoint("");
const endpoint WebServer::EP_API         = endpoint(ROOT+API);
const endpoint WebServer::EP_LOGIN       = endpoint(EP_API.Get()+"/"+LOGIN);
const endpoint WebServer::EP_USERS       = endpoint(EP_API.Get()+"/"+USERS);
const endpoint WebServer::EP_PERMISSIONS       = endpoint(EP_API.Get()+"/"+PERMISSIONS);

std::string Hash(const std::string& sKey, const std::string& sData)
{
    auto digest = HMAC(EVP_sha256(), sKey.c_str(), sKey.length(), (unsigned char*)sData.c_str(), sData.length(), nullptr, nullptr);

    std::stringstream ss;

    for(int i = 0; i < 32; i++)
    {
        ss << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)digest[i];
    }
    return ss.str();
}

std::optional<Json::Value> ConvertPostDataToJson(const postData& vData)
{
    if(vData.size() == 1)
    {
        pmlLog(pml::LOG_DEBUG, "aes67") << "ConvertPostDataToJson " << vData[0].data.Get();
        return ConvertToJson(vData[0].data.Get());
    }
    else if(vData.size() > 1)
    {
        Json::Value jsData;
        for(size_t i = 0; i < vData.size(); i++)
        {
            if(vData[i].name.Get().empty() == false)
            {
                if(vData[i].filepath.empty() == true)
                {
                    jsData[vData[i].name.Get()] = vData[i].data.Get();
                }
                else
                {
                    jsData[vData[i].name.Get()][jsonConsts::name] = vData[i].data.Get();
                    jsData[vData[i].name.Get()][jsonConsts::location] = vData[i].filepath.string();
                }
            }
        }
        return jsData;
    }
    return {};
}


WebServer::WebServer() : m_el(m_rd())
{

}



void WebServer::InitLogging()
{
    if(m_config.Get("logging", "console", 0L) > -1 )
    {
        if(m_nLogToConsole == -1)
        {
            m_nLogToConsole = pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        }
        pml::LogStream::SetOutputLevel(m_nLogToConsole, pml::enumLevel(m_config.Get("logging", "console", (long)pml::LOG_TRACE)));
    }
    else if(m_nLogToConsole != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogToConsole);
        m_nLogToConsole = -1;
    }

    if(m_config.Get("logging", "file", (long)pml::LOG_INFO) > -1)
    {
        if(m_nLogToFile == -1)
        {
            std::filesystem::path pathLog = m_config.Get(jsonConsts::path,jsonConsts::log,".");
            pathLog /= "webserver";
            m_nLogToFile = pml::LogStream::AddOutput(std::make_unique<pml::LogToFile>(pathLog));
        }
        pml::LogStream::SetOutputLevel(m_nLogToFile, pml::enumLevel(m_config.Get("logging", "file", (long)pml::LOG_INFO)));
    }
    else if(m_nLogToFile != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogToFile);
        m_nLogToFile = -1;
    }

}



int WebServer::Run(const std::string& sConfigFile)
{
    if(m_config.Read(sConfigFile) == false)
    {
        pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        pmlLog(pml::LOG_CRITICAL, "aes67") << "Could not open '" << sConfigFile << "' exiting.";
        return -1;
    }

    InitLogging();

    pmlLog(pml::LOG_INFO, "aes67") << "Core\tStart" ;
    

    if(auto addr = ipAddress(GetIpAddress(m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0"))); 
       m_server.Init(std::filesystem::path(m_config.Get(jsonConsts::api, "sslCa", "")), 
                     std::filesystem::path(m_config.Get(jsonConsts::api, "sslCert", "")), 
                     std::filesystem::path(m_config.Get(jsonConsts::api, "sslKey", "")), 
                     addr, 
                     static_cast<unsigned short>(m_config.Get(jsonConsts::api, "port", 8080L)),
                     EP_API, false,false))
    {
        
        m_server.SetAuthorizationTypeBearer(std::bind(&WebServer::AuthenticateToken, this, _1,_2), [](){
                                pml::restgoose::response theResponse(302);
                                theResponse.mHeaders = {{headerName("Location"), headerValue("/")}};
                                return theResponse;
                                }, false);

        m_server.SetUnprotectedEndpoints({methodpoint(pml::restgoose::GET, endpoint("")),
                                          methodpoint(pml::restgoose::GET, endpoint("/index.html")),
                                          methodpoint(pml::restgoose::POST, EP_LOGIN),
                                          methodpoint(pml::restgoose::GET, endpoint("/js/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/uikit/*")),
                                          methodpoint(pml::restgoose::GET, endpoint("/images/*"))});

        m_server.SetStaticDirectory(m_config.Get(jsonConsts::api,jsonConsts::static_pages, "/var/www"));
        //add server callbacks
        CreateEndpoints();

        //start the server loop
        m_server.Run(false, std::chrono::milliseconds(50));

        pmlLog(pml::LOG_INFO, "aes67") << "Core\tStop" ;
        DeleteEndpoints();

        return 0;
    }
    return -2;
}


bool WebServer::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "CreateEndpoints" ;

    m_server.AddEndpoint(pml::restgoose::GET, EP_API, std::bind(&WebServer::GetApi, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::POST, EP_LOGIN, std::bind(&WebServer::PostLogin, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::HTTP_DELETE, EP_LOGIN, std::bind(&WebServer::DeleteLogin, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_PERMISSIONS, std::bind(&WebServer::GetPermissions, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::GET, EP_USERS, std::bind(&WebServer::GetUsers, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::POST, EP_USERS, std::bind(&WebServer::PostUser, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PATCH, EP_USERS, std::bind(&WebServer::PatchUsers, this, _1,_2,_3,_4));
    
    return true;
}

void WebServer::DeleteEndpoints()
{

    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "DeleteEndpoints" ;

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_API);
    
}

pml::restgoose::response WebServer::GetRoot(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(API);
    return theResponse;
}

pml::restgoose::response WebServer::GetApi(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(LOGIN);
    theResponse.jsonData.append(USERS);
    theResponse.jsonData.append(PERMISSIONS);
    return theResponse;
}

pml::restgoose::response WebServer::PostLogin(const query&, const postData& theData, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "PostLogin" ;
    auto login = ConvertPostDataToJson(theData);
    if(login)
    {
        if(login->isMember("username") && login->isMember("password"))
        {
            if(AuthenticateUser((*login)["username"].asString(), (*login)["password"].asString()))
            {
                return CreateJwt("{"+(*login)["username"].asString()+"}");
            }
            else
            {
                return pml::restgoose::response(401, std::string("Username does not exist or password incorrect"));
            }
        }
        else
        {
            return pml::restgoose::response(400, std::string("Username or password not sent"));
        }
    }
    else
    {
        return pml::restgoose::response(400, std::string("Invalid data sent"));
    }
}

pml::restgoose::response WebServer::DeleteLogin(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "DeleteLogin" ;
    pml::restgoose::response theResponse;
    return theResponse;
}

pml::restgoose::response WebServer::GetUsers(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetUsers" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);

    for(const auto& [sSection, pSection] : m_config.GetSections())
    {
        if(sSection.front() == '{' && sSection.back() == '}')   //user section
        {
            Json::Value jsUser;
            jsUser[jsonConsts::username] = sSection.substr(1, sSection.length()-2);
            jsUser[jsonConsts::logger_server] = pSection->Get(jsonConsts::logger_server, false);
            jsUser[jsonConsts::encoder_server] = pSection->Get(jsonConsts::encoder_server, false);
            jsUser[jsonConsts::playback_server] = pSection->Get(jsonConsts::playback_server, false);
            jsUser[jsonConsts::admin] = pSection->Get(jsonConsts::admin, false);
            jsUser[jsonConsts::webserver] = true;

            theResponse.jsonData.append(jsUser);
        }
        
    }
    
    return theResponse;
}

bool WebServer::GetPermission(const jwt::decoded_jwt<jwt::traits::kazuho_picojson>& decoded_token, const std::string& sPermission) const
{
    try
    {
        return decoded_token.get_payload_claim(sPermission).as_boolean();
    }
    catch(const jwt::error::claim_not_present_exception& e)
    {
        return false;
    }
    
}

pml::restgoose::response WebServer::GetPermissions(const query&, const postData&, const endpoint&, const userName& token) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetPermissions" ;
    pml::restgoose::response theResponse;
    
    try
    {  
        auto decoded_token = jwt::decode(token.Get());
        
        theResponse.jsonData[jsonConsts::logger_server] = GetPermission(decoded_token, jsonConsts::logger_server);
        theResponse.jsonData[jsonConsts::encoder_server] = GetPermission(decoded_token, jsonConsts::encoder_server);
        theResponse.jsonData[jsonConsts::playback_server] = GetPermission(decoded_token, jsonConsts::playback_server);
        theResponse.jsonData[jsonConsts::admin] = GetPermission(decoded_token, jsonConsts::admin);

        return theResponse;        
    }
    catch(const jwt::error::token_verification_exception& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not verify token " << e.what();
        return pml::restgoose::response(400, std::string("Could not verify token - ") +e.what());
    }
    
    catch(const std::invalid_argument& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid format " << e.what();
        return pml::restgoose::response(400, std::string("Could not decode toekn - ") +e.what());
    }
    catch(const std::bad_cast& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid format " << e.what();
        return pml::restgoose::response(400, std::string("Could not decode toekn - ") +e.what());
    }
    catch(const std::runtime_error& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid base64 or json " << e.what();
        return pml::restgoose::response(400, std::string("Could not decode toekn - ") +e.what());
    }

}

pml::restgoose::response WebServer::PostUser(const query&, const postData& theData, const endpoint&, const userName&)
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "PostUser" ;
       
    if(auto user = ConvertPostDataToJson(theData); user)
    {
        if(user->isMember(jsonConsts::username) && user->isMember(jsonConsts::password))
        {
            //check that user does not already exist
            auto sSection = "{"+(*user)[jsonConsts::username].asString()+"}";
        
            if(m_config.GetSection(sSection))
            {
                return pml::restgoose::response(409, std::string("User already exists"));
            }

            m_config.Set(sSection, jsonConsts::password, Hash(HASH_KEY, (*user)[jsonConsts::password].asString()));

            SaveUserPermission(sSection, jsonConsts::logger_server, *user, false);
            SaveUserPermission(sSection, jsonConsts::encoder_server, *user, false);
            SaveUserPermission(sSection, jsonConsts::playback_server, *user, false);
            SaveUserPermission(sSection, jsonConsts::admin, *user, false);
            SaveUserPermission(sSection, jsonConsts::webserver, *user, true);
            SaveUserPermission(sSection, jsonConsts::logger_server, *user, false);
	    m_config.Write();
            return pml::restgoose::response(201, std::string("User added"));
        }
        else
        {
            return pml::restgoose::response(400, std::string("Username or password missing"));        
        }
    }
    return pml::restgoose::response(400, std::string("Invalid json"));
}

void WebServer::SaveUserPermission(const std::string& sSection, const std::string& sSetting, const Json::Value& jsData, bool bDefault)
{
    if(jsData.isMember(sSetting) && jsData[sSetting].isBool())
    {
        m_config.Set(sSection, sSetting, jsData[sSetting].asString());
    }
    else
    {
        m_config.Set(sSection, sSetting, bDefault ? "true" : "false");
    }
}

pml::restgoose::response WebServer::PatchUsers(const query&, const postData& theData, const endpoint&, const userName&)
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "PatchUsers" ;
    if(auto user = ConvertPostDataToJson(theData); user && user->isMember(jsonConsts::username))
    {
        //check that user does not already exist
        auto sSection = "{"+(*user)[jsonConsts::username].asString()+"}";
        
        if(auto pSection = m_config.GetSection(sSection); pSection)
        {
            if(user->isMember(jsonConsts::password))
            {
                m_config.Set(sSection, jsonConsts::password, Hash(HASH_KEY, (*user)[jsonConsts::password].asString()));
            }

            SaveUserPermission(sSection, jsonConsts::logger_server, *user, pSection->Get(jsonConsts::logger_server, false));
            SaveUserPermission(sSection, jsonConsts::encoder_server, *user, pSection->Get(jsonConsts::encoder_server, false));
            SaveUserPermission(sSection, jsonConsts::playback_server, *user, pSection->Get(jsonConsts::playback_server, false));
            SaveUserPermission(sSection, jsonConsts::admin, *user, pSection->Get(jsonConsts::admin, false));
            SaveUserPermission(sSection, jsonConsts::webserver, *user, pSection->Get(jsonConsts::webserver, false));
            SaveUserPermission(sSection, jsonConsts::logger_server, *user, pSection->Get(jsonConsts::logger_server, false));

            return pml::restgoose::response(200, std::string("User updated"));
        }
        else
        {
            return pml::restgoose::response(404, std::string("User not found"));
        }
    }
    else
    {
        return pml::restgoose::response(400, std::string("Invalid json"));
    }
}

pml::restgoose::response WebServer::CreateJwt(const std::string& sUsername) const
{
    auto token = jwt::create().set_issuer("WebServer")
                .set_id(sUsername)
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now()+std::chrono::hours(6))
                .set_payload_claim(jsonConsts::logger_server, jwt::claim(picojson::value(m_config.GetBool(sUsername, jsonConsts::logger_server, false))))
                .set_payload_claim(jsonConsts::encoder_server, jwt::claim(picojson::value(m_config.GetBool(sUsername, jsonConsts::encoder_server, false))))
                .set_payload_claim(jsonConsts::playback_server, jwt::claim(picojson::value(m_config.GetBool(sUsername, jsonConsts::playback_server, false))))
                .set_payload_claim(jsonConsts::admin, jwt::claim(picojson::value(m_config.GetBool(sUsername, jsonConsts::admin, false))))
                .set_payload_claim(jsonConsts::webserver, jwt::claim(picojson::value(m_config.GetBool(sUsername, jsonConsts::webserver, false))))
                .sign(jwt::algorithm::hs256{m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")});

    pml::restgoose::response resp(200);
    resp.jsonData["access_token"] = token;
    return resp;
}


bool WebServer::AuthenticateUser(const std::string& sUsername, const std::string& sPassword) const
{
    pmlLog(pml::LOG_INFO, "aes67") << "AuthenticateUser: " << Hash(HASH_KEY, sPassword);
    return (Hash(HASH_KEY, sPassword) == m_config.Get("{"+sUsername+"}", jsonConsts::password, ""));
}

bool WebServer::AuthenticateToken(const methodpoint& aPoint, const std::string& sToken) const
{

    pmlLog(pml::LOG_INFO, "aes67") << "AuthenticateToken " << m_server.GetCurrentPeer() << "=" << sToken;
    
    auto vSplit = SplitString(aPoint.second.Get(),'/');
    auto bAllowed = false;
    try
    {  
        auto decoded_token = jwt::decode(sToken);
        if(aPoint.second == EP_USERS || vSplit[0] == "admin")
        {
            auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .with_claim(jsonConsts::admin, jwt::claim(picojson::value(true)));
            verifier.verify(decoded_token);
        }
        else if(vSplit[0] == "loggingserver")
        {
            auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .with_claim(jsonConsts::logger_server, jwt::claim(picojson::value(true)));
            verifier.verify(decoded_token);
        }
        else if(vSplit[0] == "encodingserver")
        {
            auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .with_claim(jsonConsts::encoder_server, jwt::claim(picojson::value(true)));
            verifier.verify(decoded_token);
        }
        else if(vSplit[0] == "playbackserver")
        {
            auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .with_claim(jsonConsts::playback_server, jwt::claim(picojson::value(true)));
            verifier.verify(decoded_token);
        }
        else
        {
            auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L));
            verifier.verify(decoded_token);
        }
        
        //if from BNCS driver then extra check as should be new token for each message
        pmlLog(pml::LOG_DEBUG, "aes67") << "Token verified";
        bAllowed = true;
    }
    catch(const jwt::error::token_verification_exception& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not verify token " << e.what();
    }
    catch(const std::invalid_argument& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid format " << e.what();
    }
    catch(const std::bad_cast& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid format " << e.what();
    }
    catch(const std::runtime_error& e)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not decode token - invalid base64 or json " << e.what();
    }

    return bAllowed;
    
    pmlLog(pml::LOG_WARN, "aes67") << "AuthenticateToken failed";
    return false;
}
