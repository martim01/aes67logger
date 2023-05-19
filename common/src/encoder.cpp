#include "encoder.h"
#include "log.h"
#include "jsonwriter.h"
#include "observer.h"
#include "jsonconsts.h"
#include "soundfile.h"
#include "logtofile.h"

using namespace std::placeholders;

Encoder::Encoder(const std::string& sType) : 
m_sType(sType),
m_tpStart(std::chrono::system_clock::now())
{

}

Encoder::~Encoder()
{
    m_pServer = nullptr;
    std::filesystem::remove(m_pathSockets);
}


bool Encoder::Init(const std::filesystem::path& config)
{
    if(!LoadConfig(config))
    {
        std::cout << "Could not load config file: " << config << std::endl;
        return false;
    }

    m_pathSockets /= m_sName+"_"+m_sType;
    m_pathSockets.replace_extension(std::to_string(getpid()));

    //make sure the file does not exist
    std::filesystem::remove(m_pathSockets);

    m_pServer = std::make_shared<AsioServer>(m_pathSockets);
    m_pServer->Run();

    return true;
}



bool Encoder::LoadConfig(const std::filesystem::path& config)
{
    if(m_config.Read(config))
    {
        m_sName = m_config.Get(jsonConsts::general, jsonConsts::name, "");
        CreateLogging();

        m_pathWav.assign(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/audio"));
        m_pathWav /= "wav";
        m_pathWav /= m_sName;

        m_pathEncoded.assign(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/audio"));
        m_pathEncoded /= m_sType;
        m_pathEncoded /= m_sName;

        m_pathSockets = std::filesystem::path(m_config.Get(jsonConsts::path, jsonConsts::sockets, "/var/loggers/sockets"));

        try
        {
            std::filesystem::create_directories(m_pathEncoded);
            std::filesystem::create_directories(m_pathSockets);
        }
        catch(std::filesystem::filesystem_error& e)
        {
            pmlLog(pml::LOG_ERROR) << "Could not create " << m_sType << " file directory " << m_pathEncoded;
        }

        return true;
    }
    else
    {
        return false;
    }
}


void Encoder::CreateLogging()
{
    if(m_config.Get("logging", "console", 0L) > -1 )
    {
        if(m_nLogToConsole == -1)
        {
            m_nLogToConsole = pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        }
        pml::LogStream::SetOutputLevel(m_nLogToConsole, pml::enumLevel(m_config.Get("logging", "console",(long) pml::LOG_TRACE)));
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
            pathLog /= "Encoder";
            pathLog /= m_sName;
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

std::filesystem::path Encoder::GetNextFile()
{
    std::scoped_lock<std::mutex> lg(m_mutex);
    std::filesystem::path path;
            
       
    if(m_qToEncode.empty() == false)
    {
        path = m_qToEncode.front();
        m_qToEncode.pop();
        pmlLog(pml::LOG_DEBUG) << "Queue=" << m_qToEncode.size();
    }

    OutputEncodedStats(path, 0.0);

    return path;
}

int Encoder::Run()
{
    pmlLog(pml::LOG_DEBUG) << "Run";

    CreateInitialFileQueue();
    StartObserver();

    if(InitEncoder())
    {
        while(m_bRun)
        {
            if(auto path = GetNextFile(); path.empty() == false)
            {
                EncodeFile(path);
            }
            else
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_condition.wait_for(lk, std::chrono::seconds(2), [this]{return m_qToEncode.empty() == false;});
            }
        }
	    pmlLog(pml::LOG_DEBUG) << "Finished OK";
        return 0;
    }
    pmlLog(pml::LOG_DEBUG) << "Finished ERROR";

    return -1;
}


void Encoder::CreateInitialFileQueue()
{
    pmlLog(pml::LOG_INFO) << " - enum " << m_pathWav;

    std::map<std::string, std::filesystem::path> mWavFiles;
    try
    {
        for(const auto& entry : std::filesystem::directory_iterator(m_pathWav))
        {
            if(entry.path().extension() == ".wav")
            {
                mWavFiles.try_emplace(entry.path().stem(), entry.path());
            }
        }
        pmlLog(pml::LOG_INFO) << "Found " << mWavFiles.size();
        //now remove all the files that have already been encoded
        for(const auto& entry : std::filesystem::directory_iterator(m_pathEncoded))
        {   
            if(entry.path().extension() == m_sType)
            {
                mWavFiles.erase(entry.path().stem());
            }
        }

        pmlLog(pml::LOG_INFO) << "Need to encode " << mWavFiles.size();

        //and add all the files to the queue
        for(const auto& [name, path] : mWavFiles)
        {
     	    m_qToEncode.push(path);
        }
    }
    catch(const std::exception& e)
    {
        pmlLog(pml::LOG_ERROR) << m_sName << " - Failed to enum " << m_pathWav << ": " << e.what();
        SendError("Failed to enumerate", m_pathWav);
        std::cerr << e.what() << '\n';
    }
}

void Encoder::StartObserver()
{
    auto nWatch = m_observer.AddWatch(m_pathWav, pml::filewatch::Observer::CLOSED_WRITE, false);
    if(nWatch != -1)
    {
        m_observer.AddWatchHandler(nWatch, std::bind(&Encoder::OnWavWritten, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CLOSED);
    }
    else
    {
        pmlLog(pml::LOG_ERROR) << "Failed to attach watch to " << m_pathWav;
    }
    m_observer.Run();
}

void Encoder::OnWavWritten(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    std::scoped_lock<std::mutex> lg(m_mutex);
   
     pmlLog(pml::LOG_DEBUG) << "Add " << path << " to queue now its written";

    m_qToEncode.push(path);
    m_condition.notify_one();
}

void Encoder::SendError(const std::string& sMessage, const std::filesystem::path& path)
{
    Json::Value jsStatus;
    jsStatus[jsonConsts::id] = m_pathSockets.stem().string();
    jsStatus[jsonConsts::action] = "error";
    jsStatus[jsonConsts::message] = sMessage;
    jsStatus[jsonConsts::filename] = path.stem().string();
    JsonWriter::Get().writeToSocket(jsStatus, m_pServer);
    
    jsStatus[jsonConsts::heartbeat][jsonConsts::timestamp] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    jsStatus[jsonConsts::heartbeat][jsonConsts::start_time] = std::chrono::duration_cast<std::chrono::seconds>(m_tpStart.time_since_epoch()).count();
    jsStatus[jsonConsts::heartbeat][jsonConsts::up_time] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpStart).count();
    
    pmlLog(pml::LOG_DEBUG) << jsStatus;
}

void Encoder::OutputEncodedStats(const std::filesystem::path& wavFile, double dDone)
{
    Json::Value jsStatus;
    jsStatus[jsonConsts::id] = m_pathSockets.stem().string();
    jsStatus[jsonConsts::action] = "status";
    jsStatus[jsonConsts::filename] = wavFile.stem().string();
    jsStatus[jsonConsts::queue] = m_qToEncode.size();
    jsStatus[jsonConsts::encoded] = dDone;

    jsStatus[jsonConsts::heartbeat][jsonConsts::timestamp] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    jsStatus[jsonConsts::heartbeat][jsonConsts::start_time] = std::chrono::duration_cast<std::chrono::seconds>(m_tpStart.time_since_epoch()).count();
    jsStatus[jsonConsts::heartbeat][jsonConsts::up_time] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpStart).count();

    JsonWriter::Get().writeToSocket(jsStatus, m_pServer);
    pmlLog(pml::LOG_DEBUG) << jsStatus;
}
