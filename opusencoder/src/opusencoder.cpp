#include "opusencoder.h"
#include "log.h"
#include "jsonwriter.h"
#include "observer.h"
#include "jsonconsts.h"
#include "soundfile.h"
#include "logtofile.h"

using namespace std::placeholders;

OpusEncoder::~OpusEncoder()
{
    if(m_pEncoder)
    {
        ope_encoder_drain( m_pEncoder );
	    ope_encoder_destroy(m_pEncoder);
        m_pEncoder = nullptr;
    }
    if(m_pComments)
	{
        ope_comments_destroy(m_pComments);
        m_pComments = nullptr;
    }

    m_pServer = nullptr;
    std::filesystem::remove(m_pathSockets);
}


bool OpusEncoder::Init(const std::filesystem::path& config)
{
    if(!LoadConfig(config))
    {
        std::cout << "Could not load config file: " << config << std::endl;
        return false;
    }

    m_pathSockets /= m_sName+"_"+jsonConsts::opus;
    m_pathSockets.replace_extension(std::to_string(getpid()));

    //make sure the file does not exist
    std::filesystem::remove(m_pathSockets);

    m_pServer = std::make_shared<AsioServer>(m_pathSockets);
    m_pServer->Run();

    return true;
}



bool OpusEncoder::LoadConfig(const std::filesystem::path& config)
{
    if(m_config.Read(config))
    {
        m_sName = m_config.Get(jsonConsts::general, jsonConsts::name, "");
        CreateLogging();

        m_pathWav.assign(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/audio"));
        m_pathWav /= "wav";
        m_pathWav /= m_sName;

        m_pathOpus.assign(m_config.Get(jsonConsts::path, jsonConsts::audio, "/var/loggers/audio"));
        m_pathOpus /= "opus";
        m_pathOpus /= m_sName;

        m_pathSockets = std::filesystem::path(m_config.Get(jsonConsts::path, jsonConsts::sockets, "/var/loggers/sockets"));

        try
        {
            std::filesystem::create_directories(m_pathOpus);
            std::filesystem::create_directories(m_pathSockets);
        }
        catch(std::filesystem::filesystem_error& e)
        {
            pmlLog(pml::LOG_ERROR) << "Could not create opus file directory " << m_pathOpus;
        }

        return true;
    }
    else
    {
        return false;
    }
}


void OpusEncoder::CreateLogging()
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
            pathLog /= "opusencoder";
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

std::filesystem::path OpusEncoder::GetNextFile()
{
    std::scoped_lock<std::mutex> lg(m_mutex);
    std::filesystem::path path;
            
    
    
    if(m_qToEncode.empty() == false)
    {
        path = m_qToEncode.front();
        m_qToEncode.pop();
	    pmlLog(pml::LOG_DEBUG) << "Queue=" << m_qToEncode.size();
    }

    Json::Value jsStatus;
    jsStatus["action"] = "status";
    jsStatus["queue"] = m_qToEncode.size();
    jsStatus["encoded"] = 0.0;
    jsStatus["encoding"] = path.string();
    
    pmlLog(pml::LOG_DEBUG) << jsStatus;
    JsonWriter::Get().writeToSocket(jsStatus, m_pServer);

    return path;
}

int OpusEncoder::Run()
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
                m_condition.wait(lk, [this]{return m_qToEncode.empty() == false;});
            }
        }
	pmlLog(pml::LOG_DEBUG) << "Finished OK";
        return 0;
    }
    pmlLog(pml::LOG_DEBUG) << "Finished ERROR";

    return -1;
}

bool OpusEncoder::InitEncoder()
{
    m_pComments = ope_comments_create();
	if( !m_pComments )
	{
        pmlLog(pml::LOG_CRITICAL) << "Could not allocate opus comments";
        return false;
	}
    return true;
}

bool OpusEncoder::EncodeFile(const std::filesystem::path& wavFile)
{
    SoundFile sf;
    if(sf.OpenToRead(wavFile) == false)
    {
        pmlLog(pml::LOG_ERROR) << "Could not read wav file " << wavFile;

        SendError("Could not read wav file", wavFile);
        return false;
    }
    pmlLog(pml::LOG_DEBUG) << "Opened wav fie " << wavFile;
    auto path = m_pathOpus;
    path /= wavFile.stem();
    path.replace_extension(".opus");

    m_nFileEncoded = 0;

    if(m_pEncoder == nullptr)
    {
        m_pEncoder = ope_encoder_create_file(path.string().c_str(), m_pComments, sf.GetSampleRate(), sf.GetChannelCount(), 0, nullptr);
        if(m_pEncoder == nullptr)
        {
            pmlLog(pml::LOG_ERROR) << "Failed to open encoder";

            SendError("Could not create encoder", wavFile);
            return false;
        }
        pmlLog(pml::LOG_DEBUG) << "Opened encoder";
    }
    else if(ope_encoder_continue_new_file(m_pEncoder, path.string().c_str(), m_pComments ) != OPE_OK)
    {
        pmlLog(pml::LOG_ERROR) << "Failed to open encoder for new file";
        SendError("Could not reopen encoder", wavFile);
        return false;
    }
    else
    {
	    pmlLog(pml::LOG_DEBUG) << "Continue new file " << path;
    }

    auto tpStart = std::chrono::system_clock::now();

    std::vector<float> vBuffer(m_nBufferSize);
    bool bOk = true;
    do
    {
        if((bOk = sf.ReadAudio(vBuffer)))
        {
            m_nFileEncoded += vBuffer.size();

            if(ope_encoder_ctl(m_pEncoder, OPUS_SET_LSB_DEPTH(24)) != OPE_OK)
            {
                pmlLog(pml::LOG_WARN) << "Failed to set LSB_DEPTH, continuing anyway...";
                SendError("Failed to set LSB_DEPTH", wavFile);
            }
            if(ope_encoder_write_float(m_pEncoder, vBuffer.data(), vBuffer.size()/2) != OPE_OK)
            {
                pmlLog(pml::LOG_ERROR) << "Failed to encode file " << wavFile.string();
                SendError("Failed to encode file", wavFile);
                bOk = false;
            }
        }
        auto elapsed = std::chrono::system_clock::now()-tpStart;
        if(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 2)
        {
            OutputEncodedStats(wavFile, static_cast<double>(m_nFileEncoded)/static_cast<double>(sf.GetFileLength()));
            tpStart = std::chrono::system_clock::now();
        }

    } while (bOk && vBuffer.size() == m_nBufferSize);
    pmlLog() << "Encoded " << path;
    return true;
}

void OpusEncoder::CreateInitialFileQueue()
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
        for(const auto& entry : std::filesystem::directory_iterator(m_pathOpus))
        {
            if(entry.path().extension() == ".opus")
            {
                mWavFiles.erase(entry.path().stem());
            }
        }

        pmlLog(pml::LOG_INFO) << "Need to encode " << mWavFiles.size();

        //and add all the files to the queue
        for(const auto& [name, path] : mWavFiles)
        {
	   pmlLog(pml::LOG_DEBUG) << "Add " << path << " to queue";
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

void OpusEncoder::StartObserver()
{
    auto nWatch = m_observer.AddWatch(m_pathWav, pml::filewatch::Observer::CLOSED_WRITE, false);
    if(nWatch != -1)
    {
        m_observer.AddWatchHandler(nWatch, std::bind(&OpusEncoder::OnWavWritten, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CLOSED);
    }
    else
    {
        pmlLog(pml::LOG_ERROR) << "Failed to attach watch to " << m_pathWav;
    }
    m_observer.Run();
}

void OpusEncoder::OnWavWritten(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    std::scoped_lock<std::mutex> lg(m_mutex);
   
     pmlLog(pml::LOG_DEBUG) << "Add " << path << " to queue now its written";

    m_qToEncode.push(path);
    m_condition.notify_one();
}

void OpusEncoder::SendError(const std::string& sMessage, const std::filesystem::path& path)
{
    Json::Value jsStatus;
    jsStatus["action"] = "error";
    jsStatus["message"] = sMessage;
    jsStatus["path"] = path.string();
    JsonWriter::Get().writeToSocket(jsStatus, m_pServer);
    pmlLog(pml::LOG_DEBUG) << jsStatus;
}

void OpusEncoder::OutputEncodedStats(const std::filesystem::path& wavFile, double dDone)
{
    Json::Value jsStatus;
    jsStatus["action"] = "status";
    jsStatus["encoding"] = wavFile.string();
    jsStatus["queue"] = m_qToEncode.size();
    jsStatus["encoded"] = dDone;
    JsonWriter::Get().writeToSocket(jsStatus, m_pServer);
}