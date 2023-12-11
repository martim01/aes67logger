#include "loggerapp.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include "inimanager.h"
#include "inisection.h"
#include "log.h"
#include "logtofile.h"
#include "asiosession.h"
#include "jsonlog.h"
#include "jsonwriter.h"
#include <fstream>
#include "jsonconsts.h"
#include "sdpparser.h"
#include "audioframe.h"
#include "aes67utils.h"

using namespace std::placeholders;

LoggerApp::LoggerApp() :
    m_tpStart(std::chrono::system_clock::now())
{

}

LoggerApp::~LoggerApp()
{
    Exit();
}

bool LoggerApp::Init(const std::filesystem::path& config)
{
    if(!LoadConfig(config))
    {
        std::cout << "Could not load config file: " << config << std::endl;
        return false;
    }

    m_pathSockets /= m_sName;
    m_pathSockets.replace_extension(std::to_string(getpid()));


    //make sure the file does not exist
    try
    {
        std::filesystem::remove(m_pathSockets);
    }
    catch(const std::filesystem::filesystem_error& ec)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Failed to remove " << m_pathSockets << "\t" << ec.what();
    }

    pmlLog(pml::LOG_INFO, "aes67") << "Create socket pipe " << m_pathSockets.string();
    //make sure the directory exists
    try
    {
        pmlLog(pml::LOG_INFO, "aes67") << "Create socket pipe " << m_pathSockets.string();
        std::filesystem::create_directories(m_pathSockets.parent_path());
        m_pServer = std::make_shared<AsioServer>(m_pathSockets);
        m_pServer->Run();

        m_heartbeatGap = std::chrono::milliseconds(m_config.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000L));
        
        

        return true;

    }
    catch(const std::filesystem::filesystem_error& ec)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Failed to create " << m_pathSockets.parent_path() << "\t" << ec.what();
        return false;
    }
    catch(const std::system_error& ec)
    {
        pmlLog(pml::LOG_WARN, "aes67") << "Failed to open socket pipe\t" << ec.what();
        return false;
    }
}

int LoggerApp::Run()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    

    if(StartRecording() == false)
    {
        return -1;
    }

    while(true)
    {
        auto now = std::chrono::system_clock::now();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::shared_ptr<pml::aoip::AudioFrame> pFrame = nullptr;
        do
        {
            auto pFrame = m_pSession->GetNextFrame();
            if(pFrame)
            {
                WriteToSoundFile(pFrame);
            }
        }while(pFrame);

        LoopCallback(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now()-now));
    }

    pmlLog(pml::LOG_INFO, "aes67") << "Application finished";
    return 0;
}


void LoggerApp::Exit()
{
    m_scheduler.Stop();
    m_pServer = nullptr;
    
    
    std::error_code ec; 
    std::filesystem::remove(m_pathSockets,ec);
}


bool LoggerApp::LoadConfig(const std::filesystem::path& config)
{
    if(m_config.Read(config))
    {
        m_sName = m_config.Get(jsonConsts::general, jsonConsts::name, "");
        CreateLogging();
        pmlLog(pml::LOG_INFO, "aes67") << "Logging created";
        m_pathWav.assign(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/audio"));
        m_pathWav /= "wav";
        m_pathWav /= m_sName;

        m_pathSockets = std::filesystem::path(m_config.Get(jsonConsts::path, jsonConsts::sockets, "/var/loggers/sockets"));

        m_bUseTransmissionTime = m_config.GetBool(jsonConsts::aoip, jsonConsts::useTransmission, false);

        m_nFileLength = m_config.Get(jsonConsts::aoip, jsonConsts::filelength, 1L);

        try
        {
            std::filesystem::create_directories(m_pathWav);
            std::filesystem::create_directories(m_pathSockets);
        }
        catch(std::filesystem::filesystem_error&)
        {
            pmlLog(pml::LOG_ERROR, "aes67") << "Could not create wav file directory " << m_pathWav;
        }

        return true;
    }
    else
    {
        return false;
    }

    return false;
}

void LoggerApp::CreateLogging()
{
    auto nConsole = m_config.Get(jsonConsts::logs, jsonConsts::console, -1L);
    auto nFile = m_config.Get(jsonConsts::logs, jsonConsts::file, 2L);
    if(nConsole > -1)
    {
        if(m_nLogOutputConsole == -1)
        {
            m_nLogOutputConsole = (int)pml::LogStream::AddOutput(std::make_unique<pml::LogOutput>());
        }
        pml::LogStream::SetOutputLevel(m_nLogOutputConsole, (pml::enumLevel)nConsole);
    }
    else if(m_nLogOutputConsole != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogOutputConsole);
        m_nLogOutputConsole = -1;
    }
    if(nFile > -1)
    {
        if(m_nLogOutputFile == -1)
        {
            std::filesystem::path pathLog = m_config.Get(jsonConsts::path,  jsonConsts::logs, "/var/loggers/log");
            pathLog.append(m_sName);
            m_nLogOutputFile = (int)pml::LogStream::AddOutput(std::make_unique<pml::LogToFile>(pathLog));
	    pmlLog(pml::LOG_INFO, "aes67") << "Log to file " << pathLog.string() << " level: " << nFile;
        }
        pml::LogStream::SetOutputLevel(m_nLogOutputFile, (pml::enumLevel)nFile);
    }
    else if(m_nLogOutputFile != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogOutputFile);
        m_nLogOutputFile = -1;
    }
}

bool LoggerApp::StartRecording()
{
    pmlLog(pml::LOG_INFO, "aes67") << "StartRecording";
    
    
    std::stringstream ssSdp;
    if(m_config.Get(jsonConsts::source, jsonConsts::sdp,"").empty() == false)
    {
        std::ifstream ifs;
        ifs.open(m_config.Get(jsonConsts::source, jsonConsts::sdp,""));
        if(ifs.is_open())
        {

            ssSdp << ifs.rdbuf();
        }
        else
        {
            pmlLog(pml::LOG_CRITICAL, "aes67") << "Could not load SDP '" << m_config.Get(jsonConsts::source, jsonConsts::sdp,"") << "'";
            return false;
        }
    }
    else
    {
        return false;
    }

    
    pml::aoip::SdpParser parser;
    m_pSession = parser.CreateSessionFromSdp(ssSdp.str(),100, false);
    if(m_pSession == nullptr)
    {
        pmlLog(pml::LOG_CRITICAL, "aes67") << "Could not create session from SDP";
        return false;
    }
    m_pSession->AssignStatisticsCallback(std::bind(&LoggerApp::StatsCallback, this, _1, _2), std::chrono::milliseconds(1000));

    m_scheduler.AddInputSession(m_config.Get(jsonConsts::source, jsonConsts::name, "test"), m_pSession);
    m_scheduler.Run();

    return true;
}

bool LoggerApp::StatsCallback(const std::string& sGroup, const pml::aoip::rtpStats& stats)
{
    OutputStatsJson(sGroup, stats);
    return false;
}

void LoggerApp::OutputStatsJson(const std::string& sGroup, const pml::aoip::rtpStats& stats)
{
    m_jsStatus[jsonConsts::id] = m_sName;
        
    m_jsStatus["group"] = sGroup;
    m_jsStatus[jsonConsts::qos]["startedAt"] = ConvertTimeToIsoString(stats.tpStartedRecording);
    m_jsStatus["streaming"] = stats.bReceiving;
    m_jsStatus["session"] = m_pSession->GetSessionName();
     
    if(stats.lastReceived)
    {
        m_jsStatus[jsonConsts::qos]["now"] = ConvertTimeToIsoString(*stats.lastReceived);
        
        m_jsStatus[jsonConsts::qos]["interpacketGap"]["max"] = static_cast<double>(stats.intergap.max.count())/1000000.0;
        m_jsStatus[jsonConsts::qos]["interpacketGap"]["min"] = static_cast<double>(stats.intergap.min.count())/1000000.0;
        m_jsStatus[jsonConsts::qos]["interpacketGap"]["average"] = static_cast<double>(stats.intergap.average.count())/1000000.0;
        
        m_jsStatus[jsonConsts::qos]["jitter"] = 1000.0*stats.dJitter/48000;
        m_jsStatus[jsonConsts::qos]["tsdf"] = 1000.0*static_cast<double>(stats.tsdf.nTsdf)/48000;
        
        m_jsStatus[jsonConsts::qos]["kbits/s"]["max"] = stats.rate.dKbitsPerSecMax;
        m_jsStatus[jsonConsts::qos]["kbits/s"]["min"] = stats.rate.dKbitsPerSecMin;
        m_jsStatus[jsonConsts::qos]["kbits/s"]["average"] = stats.rate.dKbitsPerSecAverage;
        m_jsStatus[jsonConsts::qos]["kbits/s"]["current"] = stats.rate.dKbitsPerSecCurrent;
        
        m_jsStatus[jsonConsts::qos]["buffer"]["depth"]["max"] = stats.buffer.nJitterBufferMax;
        m_jsStatus[jsonConsts::qos]["buffer"]["depth"]["min"] = stats.buffer.nJitterBufferMin;
        m_jsStatus[jsonConsts::qos]["buffer"]["depth"]["current"] = stats.buffer.nJitterBufferDepth;
        m_jsStatus[jsonConsts::qos]["buffer"]["depth"]["empty"] = stats.buffer.nEmpty;
        m_jsStatus[jsonConsts::qos]["buffer"]["depth"]["overflow"]=stats.buffer.nOverflow;

        m_jsStatus[jsonConsts::qos]["buffer"]["packets"]["out_of_order"] = stats.buffer.nPacketsOutOfOrder;
        m_jsStatus[jsonConsts::qos]["buffer"]["packets"]["missing"] = stats.buffer.nPacketsMissing;
        
        m_jsStatus[jsonConsts::qos]["packets"]["total"]["received"] = static_cast<Json::UInt64>(stats.buffer.nTotalPacketsReceived);
        m_jsStatus[jsonConsts::qos]["packets"]["total"]["used"] = static_cast<Json::UInt64>(stats.buffer.nTotalFramesUsed);
        m_jsStatus[jsonConsts::qos]["packets"]["lastCallback"]["received"] = stats.buffer.nPacketsReceivedSinceLastCallback;
        m_jsStatus[jsonConsts::qos]["packets"]["lastCallback"]["used"] = stats.buffer.nFramesUsedSinceLastCallback;

        m_jsStatus[jsonConsts::qos]["bits"]["total"]["received"] = static_cast<Json::UInt64>(stats.buffer.nTotalBitsReceived);
        
    }
    
    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);
}



void LoggerApp::LoopCallback(std::chrono::microseconds duration)
{
    m_timeSinceLastHeartbeat += duration;
    if(std::chrono::duration_cast<std::chrono::milliseconds>(m_timeSinceLastHeartbeat) >= m_heartbeatGap)
    {
        m_timeSinceLastHeartbeat = std::chrono::microseconds(0);
        pmlLog(pml::LOG_TRACE, "aes67") << "Send heartbeat... " << (m_bHeartbeat ? "ON" : "OFF");

        OutputHeartbeatJson();
    }
}

void LoggerApp::OutputHeartbeatJson()
{
    m_jsStatus[jsonConsts::id] = m_sName;
    m_jsStatus[jsonConsts::heartbeat][jsonConsts::heartbeat] = m_bHeartbeat;
    m_jsStatus[jsonConsts::heartbeat][jsonConsts::timestamp] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    m_jsStatus[jsonConsts::heartbeat][jsonConsts::start_time] = std::chrono::duration_cast<std::chrono::seconds>(m_tpStart.time_since_epoch()).count();
    m_jsStatus[jsonConsts::heartbeat][jsonConsts::up_time] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpStart).count();
    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);
}

void LoggerApp::WriteToSoundFile(std::shared_ptr<pml::aoip::AudioFrame> pFrame)
{
    if(m_pSession->GetNumberOfChannels() > 0)
    {
        auto filePath = m_pathWav;
        unsigned long nFileName = 0;
        if(m_bUseTransmissionTime)
        {
            nFileName = std::chrono::duration_cast<std::chrono::minutes>(pFrame->GetTransmittedTime().time_since_epoch()).count();
        }
        else
        {
            nFileName = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        nFileName -= (nFileName % m_nFileLength);   //round so files are the correct length
        filePath /= std::to_string(nFileName)+".wav";

        if(m_sf.GetFile() != filePath)
        {
            m_sf.Close();
            m_sf.OpenToWrite(filePath, m_pSession->GetNumberOfChannels(), 48000, 24);

            OutputFileJson();
        }

        if(m_sf.IsOpen())
        {
            m_sf.WriteAudio(pFrame->GetAudio());
        }
        else
        {
            //what do we do next time we get a buffer??
        }
    }
}

void LoggerApp::OutputFileJson()
{
    m_jsStatus[jsonConsts::id] = m_sName;
    m_jsStatus[jsonConsts::file][jsonConsts::filename] = m_sf.GetFile().stem().string();
    m_jsStatus[jsonConsts::file][jsonConsts::filepath] = m_sf.GetFile().string();
    m_jsStatus[jsonConsts::file][jsonConsts::open] = m_sf.IsOpen();
    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);

}
