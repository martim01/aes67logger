#include "loggerapp.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include "inimanager.h"
#include "inisection.h"
#include "timedbuffer.h"
#include "log.h"
#include "logtofile.h"
#include "jsonlog.h"
#include "jsonwriter.h"
#include "aoipsourcemanager.h"
#include <fstream>
#include "jsonconsts.h"
#include "asiosession.h"


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
        pmlLog(pml::LOG_WARN) << "Failed to remove " << m_pathSockets << "\t" << ec.what();
    }

    pmlLog() << "Create socket pipe " << m_pathSockets.string();
    //make sure the directory exists
    try
    {
        pmlLog() << "Create socket pipe " << m_pathSockets.string();
        std::filesystem::create_directories(m_pathSockets.parent_path());
        m_pServer = std::make_shared<AsioServer>(m_pathSockets);
        m_pServer->Run();

        m_heartbeatGap = std::chrono::milliseconds(m_config.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000L));

        return true;

    }
    catch(const std::filesystem::filesystem_error& ec)
    {
        pmlLog(pml::LOG_WARN) << "Failed to create " << m_pathSockets.parent_path() << "\t" << ec.what();
        return false;
    }
    catch(const std::system_error& ec)
    {
        pmlLog(pml::LOG_WARN) << "Failed to open socket pipe\t" << ec.what();
        return false;
    }
}

int LoggerApp::Run()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    StartRecording();

    pmlLog() << "Application finished";
    return 0;
}


void LoggerApp::Exit()
{
    if(m_pClient)
    {
        m_pClient->Stop();
        m_pClient = nullptr;
    }
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
        pmlLog() << "Logging created";
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
        catch(std::filesystem::filesystem_error& e)
        {
            pmlLog(pml::LOG_ERROR) << "Could not create wav file directory " << m_pathWav;
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
	    pmlLog() << "Log to file " << pathLog.string() << " level: " << nFile;
        }
        pml::LogStream::SetOutputLevel(m_nLogOutputFile, (pml::enumLevel)nFile);
    }
    else if(m_nLogOutputFile != -1)
    {
        pml::LogStream::RemoveOutput(m_nLogOutputFile);
        m_nLogOutputFile = -1;
    }
}

void LoggerApp::StartRecording()
{
    pmlLog() << "StartRecording";
    m_pClient = std::make_unique<pml::aoip::AoipClient>(m_config.Get(jsonConsts::aoip, jsonConsts::buffer, 4096L));

    m_pClient->AddAudioCallback(std::bind(&LoggerApp::WriteToSoundFile, this, _1, _2));
    m_pClient->AddQosCallback(std::bind(&LoggerApp::QoSCallback, this, _1, _2));
    m_pClient->AddSessionCallback(std::bind(&LoggerApp::SessionCallback, this, _1, _2));
    m_pClient->AddStreamCallback(std::bind(&LoggerApp::StreamCallback, this, _1, _2));
    m_pClient->AddLoopCallback(std::bind(&LoggerApp::LoopCallback, this, _1));

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
            pmlLog(pml::LOG_WARN) << "Could not load SDP '" << m_config.Get(jsonConsts::source, jsonConsts::sdp,"") << "'";
        }
    }
    m_sSdp = ssSdp.str();

    auto pSource = std::make_shared<pml::aoip::AoIPSource>(1, m_config.Get(jsonConsts::source, jsonConsts::name, "test"), m_config.Get(jsonConsts::source, jsonConsts::rtsp, ""), m_sSdp);

    OutputStreamJson(false);

    m_pClient->Run();   //does not return until we exit
    m_pClient->StreamFromSource(pSource);

    while(true)
    {
         std::this_thread::sleep_for(std::chrono::seconds(1));
    };
}


void LoggerApp::QoSCallback(std::shared_ptr<pml::aoip::AoIPSource>, std::shared_ptr<pml::aoip::qosData> pData)
{
    //@todo make sure we are getting audio packages and no data loss
    pmlLog(pml::LOG_DEBUG) << "QoS"
             << "\tbitrate=" << pData->dkbits_per_second_Now << "kbit/s"
             << "\tloss=" << pData->dPacket_loss_fraction_av*100.0 << "%"
             << "\tgap=" << pData->dInter_packet_gap_ms_av << "ms"
             << "\tpackets lost=" << pData->nTotNumPacketsLost
             << "\tjitter=" << pData->dJitter << "ms"
             << "\tTS-DF=" << pData->dTSDF << "ms"
             << "\tTimestamp Errors=" << pData->nTimestampErrors;

    OutputQoSJson(pData);
}

void LoggerApp::OutputQoSJson(std::shared_ptr<pml::aoip::qosData> pData)
{
    m_jsStatus[jsonConsts::id] = m_sName;
    m_jsStatus[jsonConsts::qos][jsonConsts::bitrate] = pData->dkbits_per_second_Now;
    m_jsStatus[jsonConsts::qos][jsonConsts::packets][jsonConsts::received] = pData->nTotNumPacketsReceived;
    m_jsStatus[jsonConsts::qos][jsonConsts::packets][jsonConsts::lost] = pData->nTotNumPacketsLost;
    m_jsStatus[jsonConsts::qos][jsonConsts::timestamp_errors] = pData->nTimestampErrors;
    m_jsStatus[jsonConsts::qos][jsonConsts::packet_gap] = pData->dInter_packet_gap_ms_av;
    m_jsStatus[jsonConsts::qos][jsonConsts::jitter] = pData->dJitter;
    m_jsStatus[jsonConsts::qos][jsonConsts::tsdf] = pData->dTSDF;
    m_jsStatus[jsonConsts::qos][jsonConsts::duration] = m_dFrameDuration;

    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);
}


void LoggerApp::SessionCallback(std::shared_ptr<pml::aoip::AoIPSource>,  const pml::aoip::session& theSession)
{
    m_pServer->StartTimer(std::chrono::milliseconds(500), std::bind(&LoggerApp::StreamFail, this));

    m_session = theSession;
    if(auto itSubsession = theSession.GetCurrentSubsession(); itSubsession !=  theSession.lstSubsession.end())
    {
        m_subsession = (*itSubsession);

        m_nFrameSize = m_subsession.nChannels*m_subsession.nSampleRate;
        if(m_subsession.sCodec == "L24")
        {
            m_nFrameSize*=3;
        }
        else if(m_subsession.sCodec == "L16")
        {
            m_nFrameSize*=2;
        }
        else if(m_subsession.sCodec == "F32")
        {
            m_nFrameSize*=4;
        }

        pmlLog() << "New session: channels=" << m_subsession.nChannels << "\tsampleRate=" << m_subsession.nSampleRate;
    }
    else
    {
        m_subsession = pml::aoip::subsession();
        pmlLog(pml::LOG_WARN) << "New session but not subsession!";
    }

    OutputSessionJson();
}

void LoggerApp::OutputSessionJson()
{

    m_jsStatus[jsonConsts::id] = m_sName;

    m_jsStatus[jsonConsts::session][jsonConsts::sdp] = m_session.sRawSDP;
    m_jsStatus[jsonConsts::session][jsonConsts::name] = m_session.sName;
    m_jsStatus[jsonConsts::session][jsonConsts::type] = m_session.sType;
    m_jsStatus[jsonConsts::session][jsonConsts::description] = m_session.sDescription;
    m_jsStatus[jsonConsts::session][jsonConsts::ref_clock][jsonConsts::domain] = m_session.refClock.nDomain;
    m_jsStatus[jsonConsts::session][jsonConsts::ref_clock][jsonConsts::id] = m_session.refClock.sId;
    m_jsStatus[jsonConsts::session][jsonConsts::ref_clock][jsonConsts::type] = m_session.refClock.sType;
    m_jsStatus[jsonConsts::session][jsonConsts::ref_clock][jsonConsts::version] = m_session.refClock.sVersion;
    m_jsStatus[jsonConsts::session][jsonConsts::audio] = m_bReceivingAudio;

    m_jsStatus[jsonConsts::session][jsonConsts::subsessions] = Json::Value(Json::arrayValue);
    for(const auto& theSubsession : m_session.lstSubsession)
    {
        m_jsStatus[jsonConsts::session][jsonConsts::subsessions].append(GetSubsessionJson(theSubsession));
    }
    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);
}

Json::Value LoggerApp::GetSubsessionJson(const pml::aoip::subsession& theSubSession)
{
    Json::Value jsValue;
    jsValue[jsonConsts::id] = theSubSession.sId;
    jsValue[jsonConsts::source_address] = theSubSession.sSourceAddress;
    jsValue[jsonConsts::medium] = theSubSession.sMedium;
    jsValue[jsonConsts::codec] = theSubSession.sCodec;
    jsValue[jsonConsts::protocol] = theSubSession.sProtocol;
    jsValue[jsonConsts::port] = theSubSession.nPort;
    jsValue[jsonConsts::sample_rate] = theSubSession.nSampleRate;
    jsValue[jsonConsts::channels] = theSubSession.nChannels;
    jsValue[jsonConsts::sync_timestamp] = theSubSession.nSyncTimestamp;
    jsValue[jsonConsts::ref_clock][jsonConsts::domain] = theSubSession.refClock.nDomain;
    jsValue[jsonConsts::ref_clock][jsonConsts::id] = theSubSession.refClock.sId;
    jsValue[jsonConsts::ref_clock][jsonConsts::type] = theSubSession.refClock.sType;
    jsValue[jsonConsts::ref_clock][jsonConsts::version] = theSubSession.refClock.sVersion;

    return jsValue;

}


void LoggerApp::StreamCallback(std::shared_ptr<pml::aoip::AoIPSource>, bool bStreaming)
{
    pmlLog( bStreaming ? pml::LOG_INFO : pml::LOG_WARN) << (bStreaming ? "Stream started" : "Stream stopped");
    OutputStreamJson(bStreaming);
}

void LoggerApp::OutputStreamJson(bool bStreaming)
{
    m_jsStatus[jsonConsts::id] = m_sName;
    m_jsStatus[jsonConsts::streaming][jsonConsts::name] = m_config.Get(jsonConsts::source, jsonConsts::name, "test");
    if(m_config.Get(jsonConsts::source, jsonConsts::rtsp, "").empty() == false)
    {
        m_jsStatus[jsonConsts::streaming][jsonConsts::type] = "RTSP";
        m_jsStatus[jsonConsts::streaming][jsonConsts::source] = m_config.Get(jsonConsts::source, jsonConsts::rtsp, "");
    }
    else
    {
        m_jsStatus[jsonConsts::streaming][jsonConsts::type] = "SDP";
        m_jsStatus[jsonConsts::streaming][jsonConsts::source] = m_sSdp;
    }
    m_jsStatus[jsonConsts::streaming][jsonConsts::streaming] = bStreaming;
    JsonWriter::Get().writeToSocket(m_jsStatus, m_pServer);
}

void LoggerApp::LoopCallback(std::chrono::microseconds duration)
{
    m_timeSinceLastHeartbeat += duration;
    if(std::chrono::duration_cast<std::chrono::milliseconds>(m_timeSinceLastHeartbeat) >= m_heartbeatGap)
    {
        m_timeSinceLastHeartbeat = std::chrono::microseconds(0);
        pmlLog(pml::LOG_TRACE) << "Send heartbeat... " << (m_bHeartbeat ? "ON" : "OFF");

        OutputHeartbeatJson();
    }
    m_timeSinceLastAudio += duration;
    if(std::chrono::duration_cast<std::chrono::milliseconds>(m_timeSinceLastHeartbeat) >= std::chrono::milliseconds(500))
    {
        StreamFail();
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

void LoggerApp::WriteToSoundFile(std::shared_ptr<pml::aoip::AoIPSource>, std::shared_ptr<pml::aoip::timedbuffer> pBuffer)
{
    if(m_subsession.nChannels > 0)
    {
        m_timeSinceLastAudio = std::chrono::microseconds(0);

        if(!m_bReceivingAudio)
        {
            m_bReceivingAudio = true;
            OutputSessionJson();
        }

        m_dFrameDuration = static_cast<double>(pBuffer->GetDuration())/static_cast<double>(m_nFrameSize)*1e6;


        auto filePath = m_pathWav;
        unsigned long nFileName = 0;
        if(m_bUseTransmissionTime)
        {
            nFileName = std::chrono::duration_cast<std::chrono::minutes>(pBuffer->GetTransmissionTime().time_since_epoch()).count();
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
            m_sf.OpenToWrite(filePath, (unsigned short)m_subsession.nChannels, m_subsession.nSampleRate, 24);   //bit depth =0 implies float

            OutputFileJson();
        }

        if(m_sf.IsOpen())
        {
            m_sf.WriteAudio(pBuffer);
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

void LoggerApp::StreamFail()
{
    pmlLog(pml::LOG_WARN) << "No data from stream";
    if(m_bReceivingAudio)
    {
        m_bReceivingAudio = false;
        OutputSessionJson();
    }
}
