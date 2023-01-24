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

using namespace std::placeholders;

LoggerApp::LoggerApp() :
    m_tpStart(std::chrono::system_clock::now())
{

}

LoggerApp::~LoggerApp() = default;

bool LoggerApp::Init(const std::filesystem::path& config)
{
    if(!LoadConfig(config))
    {
        std::cout << "Could not load config file: " << config << std::endl;
        return false;
    }

    return true;

}

int LoggerApp::Run()
{
    StartRecording();

    pmlLog() << "Application finished";
    return 0;
}


void LoggerApp::Exit()
{
    pmlLog() << "Exit";
    m_pClient->Stop();
}


bool LoggerApp::LoadConfig(const std::filesystem::path& config)
{
    if(m_config.Read(config))
    {
        m_sName = m_config.Get(jsonConsts::general, jsonConsts::name, "");
        CreateLogging();

        m_pathWav.assign(m_config.Get(jsonConsts::audio, jsonConsts::path, "/var/loggers/wav"));
        m_pathWav /= m_sName;
        try
        {
            std::filesystem::create_directories(m_pathWav);
        }
        catch(std::filesystem::filesystem_error& e)
        {
            pmlLog(pml::LOG_ERROR) << "Could not create wav file directory " << m_pathWav;
        }

        m_heartbeatGap = std::chrono::milliseconds(m_config.Get(jsonConsts::heartbeat, jsonConsts::gap, 10000));
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
    auto nConsole = m_config.Get(jsonConsts::logs, jsonConsts::console, -1);
    auto nFile = m_config.Get(jsonConsts::logs, jsonConsts::file, 2);
    if(nConsole > -1)
    {
        if(m_nLogOutputConsole == -1)
        {
            m_nLogOutputConsole = pml::LogStream::AddOutput(std::make_unique<JsonLog>(m_sName));
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
            std::filesystem::path pathLog = m_config.Get(jsonConsts::logs, jsonConsts::path, "/var/loggers/log");
            pathLog.append(m_sName);
            m_nLogOutputFile = pml::LogStream::AddOutput(std::make_unique<pml::LogToFile>(pathLog));
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
    m_pClient = std::make_unique<pml::aoip::AoipClient>(m_config.Get(jsonConsts::aoip, jsonConsts::interface, "eth0"), m_config.Get(jsonConsts::general, jsonConsts::name, "test"), m_config.Get(jsonConsts::aoip, jsonConsts::buffer, 4096));

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
    auto pSource = std::make_shared<pml::aoip::AoIPSource>(1, m_config.Get(jsonConsts::source, jsonConsts::name, "test"), m_config.Get(jsonConsts::source, jsonConsts::rtsp, ""), ssSdp.str());

    m_pClient->StreamFromSource(pSource);
    m_pClient->Run();   //does not return until we exit

}


void LoggerApp::QoSCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource, std::shared_ptr<pml::aoip::qosData> pData)
{
    //@todo make sure we are getting audio packages and no data loss
    pmlLog() << "QoS"
             << "\tbitrate=" << pData->dkbits_per_second_Now << "kbit/s"
             << "\tloss=" << pData->dPacket_loss_fraction_av*100.0 << "%"
             << "\tgap=" << pData->dInter_packet_gap_ms_Now << "ms"
             << "\tpackets lost=" << pData->nTotNumPacketsLost
             << "\tjitter=" << pData->dJitter << "ms"
             << "\tTS-DF=" << pData->dTSDF << "ms"
             << "\tTimestamp Errors=" << pData->nTimestampErrors;

    OutputQoSJson(pData);
}

void LoggerApp::OutputQoSJson(std::shared_ptr<pml::aoip::qosData> pData)
{
    Json::Value jsValue;
    jsValue[jsonConsts::id] = m_sName;
    jsValue[jsonConsts::event] = jsonConsts::qos;
    jsValue[jsonConsts::data][jsonConsts::bitrate] = pData->dkbits_per_second_Now;
    jsValue[jsonConsts::data][jsonConsts::packets][jsonConsts::received] = pData->nTotNumPacketsReceived;
    jsValue[jsonConsts::data][jsonConsts::packets][jsonConsts::lost] = pData->nTotNumPacketsLost;
    jsValue[jsonConsts::data][jsonConsts::timestamp_errors] = pData->nTimestampErrors;
    jsValue[jsonConsts::data][jsonConsts::packet_gap] = pData->dInter_packet_gap_ms_Now;
    jsValue[jsonConsts::data][jsonConsts::jitter] = pData->dJitter;
    jsValue[jsonConsts::data][jsonConsts::tsdf] = pData->dTSDF;

    JsonWriter::Get().writeToStdOut(jsValue);
}


void LoggerApp::SessionCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource,  const pml::aoip::session& theSession)
{
    auto itSubsession = theSession.GetCurrentSubsession();
    if(itSubsession != theSession.lstSubsession.end())
    {
        m_subsession = (*itSubsession);
        pmlLog() << "New session: channels=" << m_subsession.nChannels << "\tsampleRate=" << m_subsession.nSampleRate;
    }
    else
    {
        m_subsession = pml::aoip::subsession();
        pmlLog(pml::LOG_WARN) << "New session but not subsession!";
    }

    OutputSessionJson(theSession);
}

void LoggerApp::OutputSessionJson(const pml::aoip::session& theSession)
{
    Json::Value jsValue;
    jsValue[jsonConsts::id] = m_sName;
    jsValue[jsonConsts::event] = jsonConsts::session;

    jsValue[jsonConsts::data][jsonConsts::sdp] = theSession.sRawSDP;
    jsValue[jsonConsts::data][jsonConsts::name] = theSession.sName;
    jsValue[jsonConsts::data][jsonConsts::type] = theSession.sType;
    jsValue[jsonConsts::data][jsonConsts::description] = theSession.sDescription;
    jsValue[jsonConsts::data][jsonConsts::groups] = theSession.sGroups;
    jsValue[jsonConsts::data][jsonConsts::ref_clock][jsonConsts::domain] = theSession.refClock.nDomain;
    jsValue[jsonConsts::data][jsonConsts::ref_clock][jsonConsts::id] = theSession.refClock.sId;
    jsValue[jsonConsts::data][jsonConsts::ref_clock][jsonConsts::type] = theSession.refClock.sType;
    jsValue[jsonConsts::data][jsonConsts::ref_clock][jsonConsts::version] = theSession.refClock.sVersion;

    jsValue[jsonConsts::data][jsonConsts::subsessions] = Json::Value(Json::arrayValue);
    for(const auto& theSubsession : theSession.lstSubsession)
    {
        jsValue[jsonConsts::data][jsonConsts::subsessions].append(GetSubsessionJson(theSubsession));
    }
    JsonWriter::Get().writeToStdOut(jsValue);
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


void LoggerApp::StreamCallback(std::shared_ptr<pml::aoip::AoIPSource> pSource, bool bStreaming)
{
    pmlLog( bStreaming ? pml::LOG_INFO : pml::LOG_WARN) << (bStreaming ? "Stream started" : "Stream stopped");
    OutputStreamJson(bStreaming);
}

void LoggerApp::OutputStreamJson(bool bStreaming)
{
    Json::Value jsValue;
    jsValue[jsonConsts::id] = m_sName;
    jsValue[jsonConsts::event] = jsonConsts::streaming;
    jsValue[jsonConsts::data][jsonConsts::streaming] = bStreaming;
    JsonWriter::Get().writeToStdOut(jsValue);
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
}

void LoggerApp::OutputHeartbeatJson()
{
    Json::Value jsValue;
    jsValue[jsonConsts::id] = m_sName;
    jsValue[jsonConsts::event] = jsonConsts::heartbeat;
    jsValue[jsonConsts::data][jsonConsts::heartbeat] = m_bHeartbeat;
    jsValue[jsonConsts::data][jsonConsts::timestamp] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    jsValue[jsonConsts::data][jsonConsts::start_time] = std::chrono::duration_cast<std::chrono::seconds>(m_tpStart.time_since_epoch()).count();
    jsValue[jsonConsts::data][jsonConsts::up_time] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-m_tpStart).count();
    JsonWriter::Get().writeToStdOut(jsValue);
}

void LoggerApp::WriteToSoundFile(std::shared_ptr<pml::aoip::AoIPSource> pSource, std::shared_ptr<pml::aoip::timedbuffer> pBuffer)
{
    if(m_subsession.nChannels > 0)
    {
        auto filePath = m_pathWav;
        filePath /= (ConvertTimeToString(pBuffer->GetTransmissionTime(), "%Y-%m-%dT%H-%M")+".wav");


        if(m_sf.GetFilename() != filePath)
        {
            m_sf.Close();
            m_sf.OpenToWrite(filePath.string(), m_subsession.nChannels, m_subsession.nSampleRate, 0);   //bit depth =0 implies float

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
    Json::Value jsValue;
    jsValue[jsonConsts::id] = m_sName;
    jsValue[jsonConsts::event] = jsonConsts::file;
    jsValue[jsonConsts::data][jsonConsts::filename] = m_sf.GetFilename();
    jsValue[jsonConsts::data][jsonConsts::open] = m_sf.IsOpen();
    JsonWriter::Get().writeToStdOut(jsValue);

}

