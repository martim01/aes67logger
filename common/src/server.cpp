#include "server.h"
#include "sysinfomanager.h"
#include "json/json.h"
#include <sstream>
#include "log.h"
#include "logtofile.h"
#include <spawn.h>

#include <sys/types.h>
//#include "epi_version.h"
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include "proccheck.h"
#include <chrono>
#include "aes67utils.h"
#include "jsonconsts.h"
#include <iomanip>
#include "jwt-cpp/jwt.h"

using namespace std::placeholders;
using namespace pml;

const std::string Server::ROOT        = "/";             //GET
const std::string Server::API         = "x-api";          //GET
const std::string Server::POWER       = "power";
const std::string Server::CONFIG      = "config";
const std::string Server::STATUS      = "status";
const std::string Server::INFO        = "info";
const std::string Server::UPDATE      = "update";
const std::string Server::LOGS        = "logs";
const std::string Server::WS          = "ws";        //GET PUT

const endpoint Server::EP_ROOT        = endpoint("");
const endpoint Server::EP_API         = endpoint(ROOT+API);
const endpoint Server::EP_POWER       = endpoint(EP_API.Get()+"/"+POWER);
const endpoint Server::EP_CONFIG      = endpoint(EP_API.Get()+"/"+CONFIG);
const endpoint Server::EP_UPDATE      = endpoint(EP_API.Get()+"/"+UPDATE);
const endpoint Server::EP_INFO        = endpoint(EP_API.Get()+"/"+INFO);
const endpoint Server::EP_STATUS      = endpoint(EP_API.Get()+"/"+STATUS);
const endpoint Server::EP_LOGS        = endpoint(EP_API.Get()+"/"+LOGS);
const endpoint Server::EP_WS          = endpoint(EP_API.Get()+"/"+WS);
const endpoint Server::EP_WS_INFO     = endpoint(EP_WS.Get()+"/"+INFO);
const endpoint Server::EP_WS_STATUS   = endpoint(EP_WS.Get()+"/"+STATUS);


pml::restgoose::response ConvertPostDataToJson(const postData& vData)
{
    pml::restgoose::response resp(404, std::string("No data sent or incorrect data sent"));
    if(vData.size() == 1)
    {
        auto js = ConvertToJson(vData[0].data.Get());
        if(js)
        {
            resp.nHttpCode = 200;
            resp.jsonData = *js;
        }
        else
        {
            resp.nHttpCode = 400;
        }
    }
    else if(vData.size() > 1)
    {
        resp.nHttpCode = 200;
        resp.jsonData.clear();
        for(size_t i = 0; i < vData.size(); i++)
        {
            if(vData[i].name.Get().empty() == false)
            {
                if(vData[i].filepath.empty() == true)
                {
                    resp.jsonData[vData[i].name.Get()] = vData[i].data.Get();
                }
                else
                {
                    resp.jsonData[vData[i].name.Get()][jsonConsts::name] = vData[i].data.Get();
                    resp.jsonData[vData[i].name.Get()][jsonConsts::location] = vData[i].filepath.string();
                }
            }
        }
    }
    return resp;
}


Server::Server(const std::string& sApp) : m_sApp(sApp)
{

}



void Server::InitLogging()
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
            pathLog /= m_sApp;
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



int Server::Run(const std::string& sConfigFile)
{
    if(m_config.Read(sConfigFile) == false)
    {
        pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        pmlLog(pml::LOG_CRITICAL, "aes67") << "Could not open '" << sConfigFile << "' exiting.";
        return -1;
    }

    InitLogging();

    pmlLog(pml::LOG_INFO, "aes67") << "Core\tStart" ;

    
    m_info.SetDiskPath(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers"));

    

    if(auto addr = ipAddress(GetIpAddress(m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0"))); 
       m_server.Init(std::filesystem::path(m_config.Get(jsonConsts::api, "sslCa", "")), 
                     std::filesystem::path(m_config.Get(jsonConsts::api, "sslCert", "")), 
                     std::filesystem::path(m_config.Get(jsonConsts::api, "sslKey", "")), 
                     addr, 
                     static_cast<unsigned short>(m_config.Get(jsonConsts::api, "port", 8080L)),
                     EP_API, true,true))
    {

        m_server.SetAuthorizationTypeBearer(std::bind(&Server::AuthenticateToken, this, _1,_2), [](){return pml::restgoose::response(401, std::string("Bearer token incorrect"));}, true);

        //Derived class init
        Init();

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


bool Server::CreateEndpoints()
{

    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "CreateEndpoints" ;

    m_server.AddEndpoint(pml::restgoose::GET, EP_API, std::bind(&Server::GetApi, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_STATUS, std::bind(&Server::GetStatus, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_CONFIG, std::bind(&Server::GetConfig, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PATCH, EP_CONFIG, std::bind(&Server::PatchConfig, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_POWER, std::bind(&Server::GetPower, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PUT, EP_POWER, std::bind(&Server::PutPower, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_INFO, std::bind(&Server::GetInfo, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_UPDATE, std::bind(&Server::GetUpdate, this, _1,_2,_3,_4));
    m_server.AddEndpoint(pml::restgoose::PUT, EP_UPDATE, std::bind(&Server::PutUpdate, this, _1,_2,_3,_4));

    m_server.AddEndpoint(pml::restgoose::GET, EP_LOGS, std::bind(&Server::GetLogs, this, _1,_2,_3,_4));

    m_server.AddWebsocketEndpoint(EP_WS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_INFO, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));
    m_server.AddWebsocketEndpoint(EP_WS_STATUS, std::bind(&Server::WebsocketAuthenticate, this, _1,_2,_3,_4), std::bind(&Server::WebsocketMessage, this, _1,_2), std::bind(&Server::WebsocketClosed, this, _1,_2));

    AddCustomEndpoints();
    //Add the loop callback function
    m_server.SetLoopCallback(std::bind(&Server::LoopCallback, this, _1));

    return true;
}

void Server::DeleteEndpoints()
{

    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "DeleteEndpoints" ;

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_API);
    m_server.DeleteEndpoint(pml::restgoose::GET, EP_STATUS);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_CONFIG);
    m_server.DeleteEndpoint(pml::restgoose::PATCH, EP_CONFIG);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_POWER);
    m_server.DeleteEndpoint(pml::restgoose::PUT, EP_POWER);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_INFO);

    m_server.DeleteEndpoint(pml::restgoose::GET, EP_UPDATE);
    m_server.DeleteEndpoint(pml::restgoose::PUT, EP_UPDATE);

    DeleteCustomEndpoints();

}

pml::restgoose::response Server::GetRoot(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetRoot" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = Json::Value(Json::arrayValue);
    theResponse.jsonData.append(API);
    return theResponse;
}



pml::restgoose::response Server::PutUpdate(const query&, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse(405, std::string("not written yet"));
    return theResponse;
}


pml::restgoose::response Server::GetConfig(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetConfig" ;
    pml::restgoose::response theResponse;

    char host[256];
    gethostname(host, 256);
    theResponse.jsonData[jsonConsts::server][jsonConsts::hostname] = host;
    theResponse.jsonData[jsonConsts::server][jsonConsts::ip_address][m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0")] = GetIpAddress(m_config.Get(jsonConsts::api, jsonConsts::interface, "eth0"));

    for(const auto& [sName, pSection] : m_config.GetSections())
    {
        if(sName[0] != '_') //_ = restricted
        {
            theResponse.jsonData[sName] = Json::Value(Json::objectValue);
            for(const auto& [sKey, sValue] : pSection->GetData())
            {
                if(sKey[0] != '_')
                {
                    theResponse.jsonData[sName][sKey] = sValue;
                }
            }
        }
    }

    return theResponse;
}

pml::restgoose::response Server::GetUpdate(const query&, const postData&, const endpoint&, const userName&) const
{
    //get all the version numbers...
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetUpdate" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData = GetVersion();
    
    //get versions of other applications...
    return theResponse;
}


pml::restgoose::response Server::GetInfo(const query&, const postData&, const endpoint&, const userName&)
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetInfo" ;

    pml::restgoose::response theResponse;
    theResponse.jsonData = m_info.GetInfo();


    return theResponse;
}

pml::restgoose::response Server::GetPower(const query&, const postData&, const endpoint&, const userName&) const
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "GetPower" ;
    pml::restgoose::response theResponse;
    theResponse.jsonData[jsonConsts::status] = "On";

    return theResponse;
}


pml::restgoose::response Server::PutPower(const query& , const postData& vData, const endpoint& , const userName& )
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "PutPower: ";

    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode != 200)
    {
        return theResponse;
    }

    if(theResponse.jsonData.isMember(jsonConsts::command) == false || theResponse.jsonData[jsonConsts::command].isString() == false)
    {
        theResponse.nHttpCode = 400;
        theResponse.jsonData[jsonConsts::reason].append("No command sent");
        pmlLog(pml::LOG_ERROR, "aes67") << " no command sent" ;
    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "restart server"))
    {
        // send a message to "main" to exit the loop
        pmlLog(pml::LOG_INFO, "aes67") << " restart server" ;
        m_server.Stop();
    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "restart os"))
    {
        pmlLog(pml::LOG_INFO, "aes67") << " restart os" ;

        theResponse = Reboot(LINUX_REBOOT_CMD_RESTART);

    }
    else if(CmpNoCase(theResponse.jsonData[jsonConsts::command].asString(), "shutdown"))
    {
        pmlLog(pml::LOG_INFO, "aes67") << " shutdown" ;

        theResponse = Reboot(LINUX_REBOOT_CMD_POWER_OFF);
    }
    else
    {
        theResponse.nHttpCode = 400;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append("Invalid command sent");
        pmlLog(pml::LOG_ERROR, "aes67") << "'" << theResponse.jsonData[jsonConsts::command].asString() <<"' is not a valid command" ;
    }

    return theResponse;
}

pml::restgoose::response Server::PatchConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser)
{
    pmlLog(pml::LOG_DEBUG, "aes67") << "Endpoints\t" << "PatchConfig" ;

    auto theResponse = ConvertPostDataToJson(vData);
    if(theResponse.nHttpCode != 200)
    {
        return theResponse;
    }

    if(theResponse.jsonData.isObject() == false)
    {
        theResponse.nHttpCode  = 400;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append("Invalid JSON");
    }
    else
    {
        m_config.ReRead();    //sync with on disk version

        for(auto const& sSection : theResponse.jsonData.getMemberNames())
        {
            if(theResponse.jsonData[sSection].isObject())
            {
                if(sSection == "server")
                {
                    PatchServerConfig(theResponse.jsonData[sSection]);
                }
                else if(sSection != "restricted" && sSection != "remove")
                {
                    auto pSection = m_config.GetSection(sSection);
                    if(pSection)
                    {
                        for(auto const& sKey : theResponse.jsonData[sSection].getMemberNames())
                        {
                            if(pSection->FindData(sKey) != pSection->GetDataEnd() && theResponse.jsonData[sSection][sKey].isConvertibleTo(Json::stringValue))
                            {
                                pSection->Set(sKey, theResponse.jsonData[sSection][sKey].asString());
                            }
                        }
                    }
                }
            }
        }
        m_config.Write();
        InitLogging();  //change logging if necessary
        //m_manager.InitPaths();  //change paths if necessary
        theResponse = GetConfig(theQuery, vData, theEndpoint, theUser);
        theResponse.jsonData[jsonConsts::success] = true;
    }
    return theResponse;
}


void Server::PatchServerConfig(const Json::Value& jsData) const
{
    for(auto const& sKey : jsData.getMemberNames())
    {
        if(sKey == "hostname" && jsData[sKey].isString() && sethostname(jsData[sKey].asString().c_str(), jsData[sKey].asString().length()) != 0)
        {
            pmlLog(pml::LOG_WARN, "aes67") << "Failed to set hostname";
        }
    }
}




pml::restgoose::response Server::GetLogs(const query& theQuery, const postData&, const endpoint&, const userName&) const
{
    pml::restgoose::response theResponse(404, std::string("Log not defined"));

    auto itLogger = theQuery.find(queryKey("logger"));
    auto itStart = theQuery.find(queryKey("start_time"));
    auto itEnd = theQuery.find(queryKey("end_time"));

    if(itLogger != theQuery.end() && itStart != theQuery.end() && itEnd != theQuery.end())
    {
        return GetLog(itLogger->second.Get(), itStart->second.Get(), itEnd->second.Get());
    }
    return theResponse;
}

time_t Server::GetDateTime(time_t date, const std::vector<std::string>& vLine) const
{
    time_t timeMessage(0);
    std::tm atm= {};
    atm.tm_isdst = -1;

    if(vLine.empty() == false)
    {
        auto sTime = vLine[0];
        //remove the milliseconds
        if(auto nPos = sTime.find('.'); nPos != std::string::npos)
        {
            sTime = sTime.substr(0,nPos);
        }

        std::stringstream ssDateTime;
        ssDateTime << std::put_time(localtime(&date), "%Y-%m-%d");
        ssDateTime << " " << sTime;
        ssDateTime >> std::get_time(&atm, "%Y-%m-%d %H:%M:%S");
        if(ssDateTime.fail())
        {
            pmlLog(pml::LOG_DEBUG, "aes67") << "failed to parse date time: " << ssDateTime.str();
        }
        else
        {
            if(atm.tm_year < 100)
            {
                atm.tm_year += 100;
            }
            timeMessage = mktime(&atm);
        }
    }
    return timeMessage;
}

pml::restgoose::response Server::GetLog(const std::string& sLogger, const std::string& sStart, const std::string& sEnd) const
{
    std::filesystem::path logDir = m_config.Get(jsonConsts::path,jsonConsts::log,".");
    logDir /= sLogger;

    if(std::filesystem::exists(logDir) == false)
    {
        return pml::restgoose::response(404, "Log not found: " + logDir.string());
    }

    try
    {
        unsigned long nTimeFrom = std::stoul(sStart);
        unsigned long nTimeTo = std::stoul(sEnd);

        std::stringstream ssLog;

        //get an array of the logs we need to open
        for(auto nTime = (time_t)nTimeFrom; (nTime/3600) <= ((time_t)nTimeTo/3600); nTime+=3600)
        {
            std::stringstream ssDate;
            ssDate << std::put_time(localtime(&nTime), "%Y-%m-%dT%H.log");

            auto logFile = logDir;
            logFile /= ssDate.str();

            std::ifstream inFile;
            inFile.open(logFile,std::ios::in);
            if(!inFile.is_open())
            {
                ssLog << std::put_time(localtime(&nTime), "%Y-%m-%d") << " " << std::put_time(localtime(&nTime), "%H:00:00");
                ssLog <<  "\tERROR\t------ NO LOG FILE FOUND FOR THIS HOUR ------" << std::endl;
            }
            else
            {
                if(sLogger != m_sApp)
                {
                    pmlLog(pml::LOG_DEBUG, "aes67") << logFile << " opened";
                }

                inFile.clear();
                std::string sLine;
                while(!inFile.eof())
                {
                    getline(inFile,sLine,'\n');
                    if(sLine.empty() == false)
                    {
                        auto vSplit = SplitString(sLine, '\t', 3);
                        if(vSplit.size() > 2)
                        {
                            auto timeMessage = GetDateTime(nTime, vSplit);

                            if(timeMessage >= nTimeFrom && timeMessage <= nTimeTo)
                            {
                                ssLog << std::put_time(localtime(&nTime), "%Y-%m-%d") << " ";
                                ssLog << vSplit[0] << "\t" << vSplit[1]  << "\t" << vSplit[2] << std::endl;
                            }
                        }
                    }
                }
                inFile.close();
            }
        }

        pml::restgoose::response resp(200);
        resp.jsonData = ssLog.str();
        return resp;
    }
    catch(const std::exception& )
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Could not convert start and end times " << sStart << " " << sEnd;
        return pml::restgoose::response(400, std::string("Could not convert start and end times"));
    }

}

//m_launcher.GetStatusSummary()

void Server::LoopCallback(std::chrono::milliseconds durationSince)
{
    m_nTimeSinceLastCall += durationSince.count();


    if(m_nTimeSinceLastCall > 2000)
    {
        m_server.SendWebsocketMessage({EP_WS_INFO}, m_info.GetInfo());
        m_server.SendWebsocketMessage({EP_WS_STATUS}, GetCustomStatusSummary());
        m_nTimeSinceLastCall = 0;
    }

    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    tm local_time = *localtime(&now);
    if(local_time.tm_min == 0 && local_time.tm_sec == 0)
    {
        if(m_bLoggedThisHour == false)
        {
            m_bLoggedThisHour = true;
            pmlLog(pml::LOG_INFO, "aes67") << GetCurrentTimeAsIsoString() ;
        }
    }
    else
    {
        m_bLoggedThisHour = false;
    }
}



pml::restgoose::response Server::Reboot(int nCommand) const
{
    pml::restgoose::response theResponse;
    sync(); //make sure filesystem is synced
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    if(int nError = reboot(nCommand); nError == -1)
    {
        theResponse.nHttpCode = 500;
        theResponse.jsonData[jsonConsts::success] = false;
        theResponse.jsonData[jsonConsts::reason].append(strerror(errno));
    }
    else
    {
        theResponse.jsonData[jsonConsts::success] = true;
    }
    return theResponse;
}

bool Server::WebsocketAuthenticate(const endpoint& anEndpoint, const query&, const userName& theUser, const ipAddress& peer) const
{
    ipAddress theSocket = peer;
    //strip off the port number if it's been included
    if(auto nPos = theSocket.Get().find(':'); nPos != std::string::npos)
    {
        theSocket = ipAddress(theSocket.Get().substr(0, nPos));
    }
    pmlLog(pml::LOG_INFO, "aes67") << "Websocket connection request from " << theSocket;
    return DoAuthenticateToken(theUser.Get(), theSocket);
}

bool Server::WebsocketMessage(const endpoint&, const Json::Value& jsData) const
{
    pmlLog(pml::LOG_INFO, "aes67") << "Websocket message '" << jsData << "'";
    return true;
}

void Server::WebsocketClosed(const endpoint&, const ipAddress& peer) const
{
    pmlLog(pml::LOG_INFO, "aes67") << "Websocket closed from " << peer;
}

bool Server::AuthenticateToken(const methodpoint& , const std::string& sToken) const
{
    return DoAuthenticateToken( sToken, m_server.GetCurrentPeer(false));
}

bool Server::DoAuthenticateToken(const std::string& sToken, const ipAddress& peer) const
{
    pmlLog(pml::LOG_INFO, "aes67") << "AuthenticateToken " << peer << "=" << sToken;
    
    //If we haven't got a secret then just say ok - only for debugging!!
    if(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "").empty())
    {
        return true;
    }

    auto bAllowed = false;
    try
    {  
        auto decoded_token = jwt::decode(sToken);
        auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::secret, "")))
                        .expires_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .issued_at_leeway(m_config.Get(jsonConsts::restricted_authentication, jsonConsts::leeway, 60L))
                        .with_claim(m_sApp, jwt::claim(picojson::value(true)));
        verifier.verify(decoded_token);
        
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


