#include "loggerobserver.h"
#include "inimanager.h"
#include "observer.h"
#include "jsonconsts.h"
#include "inisection.h"
#include "log.h"
#include "playbackserver.h"
#include "threadpool.h"

using namespace std::placeholders;

LoggerObserver::LoggerObserver(PlaybackServer& server, const std::string& sName, const iniManager& config, pml::filewatch::Observer& observer) :
m_server(server),
m_sName(sName)
{
    std::filesystem::path pathAudio = config.Get(jsonConsts::path, jsonConsts::audio, ".");
    
    m_nFileLength = config.Get(jsonConsts::aoip, jsonConsts::filelength, 1L);

    auto pSection = config.GetSection(jsonConsts::keep);
    if(pSection)
    {
        for(const auto& [key, value] : pSection->GetData())
        {
            try
            {
                auto nKeep = std::stoul(value);
                if(nKeep > 0)
                {
                    auto path = pathAudio;
                    path /= key;
                    path /= m_sName;
                    
                    m_mFiles.try_emplace(key, EnumFiles(path, "."+key));
                    
                    if(auto nWatch = observer.AddWatch(path, pml::filewatch::Observer::CREATED | pml::filewatch::Observer::DELETED, false); nWatch != 0)
                    {
                        m_mWatches.try_emplace(nWatch, key);
                        observer.AddWatchHandler(nWatch, std::bind(&LoggerObserver::OnFileCreated, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CREATED);
                        observer.AddWatchHandler(nWatch, std::bind(&LoggerObserver::OnFileDeleted, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::DELETED);
                    }
                }
            }
            catch(const std::invalid_argument& e)
            {
                pmlLog(pml::LOG_DEBUG, "aes67") << m_sName << " - Could not decode keep value " << key << "=" << value;
            }
            catch(const std::out_of_range& e)
            {
                pmlLog(pml::LOG_DEBUG, "aes67") << m_sName << " - Could not decode keep value " << key << "=" << value;
            }
        }
    }
    else
    {
        pmlLog(pml::LOG_ERROR, "aes67") << m_sName << " - could not find keep section in ini file";
    }
}



std::set<std::filesystem::path> LoggerObserver::EnumFiles(const std::filesystem::path& path, const std::string& sExt) const
{
    pmlLog(pml::LOG_INFO, "aes67") << m_sName << " - enum " << path;

    std::set<std::filesystem::path> setFiles;
    try
    {
        for(const auto& entry : std::filesystem::directory_iterator(path))
        {
            if(entry.path().extension() == sExt)
            {
                setFiles.insert(entry.path());
            }
        }
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << m_sName << " - Failed to enum " << path << ": " << e.what();
        std::cerr << e.what() << '\n';
    }
    return setFiles;
}


void LoggerObserver::OnFileCreated(int nWd, const std::filesystem::path& path, uint32_t, bool)
{
    auto itType = m_mWatches.find(nWd);
    if(itType != m_mWatches.end())
    {
        auto itFiles = m_mFiles.find(itType->second);
        if(itFiles != m_mFiles.end())
        {
            itFiles->second.insert(path);
            m_server.FileCreated(m_sName, path);

            pmlLog(pml::LOG_INFO, "aes67") << m_sName << " - " << path << " created";
        }
        else
        {
            pmlLog(pml::LOG_WARN, "aes67") << m_sName << " - " << path << " created but not expected type";
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN, "aes67") << m_sName << " - " << path << " created but watch not found";
    }
}

void LoggerObserver::OnFileDeleted(int nWd, const std::filesystem::path& path, uint32_t, bool)
{
    auto itType = m_mWatches.find(nWd);
    if(itType != m_mWatches.end())
    {
        auto itFiles = m_mFiles.find(itType->second);
        if(itFiles != m_mFiles.end())
        {
            itFiles->second.erase(path);
            m_server.FileDeleted(m_sName, path);

            pmlLog(pml::LOG_INFO, "aes67") << m_sName << " - " << path << " deleted";
        }
        else
        {
            pmlLog(pml::LOG_WARN, "aes67") << m_sName << " - " << path << " deleted but not expected type";
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN, "aes67") << m_sName << " - " << path << " deleted but watch not found";
    }
}

std::pair<std::chrono::minutes, std::chrono::seconds> LoggerObserver::GetBaseFileName(unsigned long nTime) const
{
     //get the base start time
    auto nBase = nTime / 60;
    nBase -= (nBase % m_nFileLength);

    //get the difference between the base start time and asked for time
    return {std::chrono::minutes(nBase), std::chrono::seconds(nTime - (nBase*60))};
}

pml::restgoose::response LoggerObserver::CreateDownloadFile(const std::string& sType, const query& theQuery) const
{
    auto id = m_sName+"_"+GetCurrentTimeAsString(false);
    pml::restgoose::ThreadPool::Get().Submit([this, sType, theQuery, id]()
    {
        DownloadFileThread(sType, theQuery, id);
    });

    pml::restgoose::response resp;
    resp.jsonData["id"] = id;
    return resp;
}

void LoggerObserver::DownloadFileThread(const std::string& sType, const query& theQuery, const std::string& sId) const
{
    if(auto itFiles = GetEncodedFiles().find(sType); itFiles != GetEncodedFiles().end() && itFiles->second.empty() == false)
    {
        m_server.DownloadFileMessage(sId, 200, std::string("Check start and end times"));
        auto itStart = theQuery.find(queryKey("start_time"));
        auto itEnd = theQuery.find(queryKey("end_time"));
        if(itStart == theQuery.end() || itEnd == theQuery.end())
        {
            m_server.DownloadFileMessage(sId, 400, std::string("No start time or end time sent"));
            return;
        }

        try
        {       
            pmlLog(pml::LOG_INFO, "aes67") << "CreateDownloadFile from: " << std::min(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get())) << " to " << std::max(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get()));

            m_server.DownloadFileMessage(sId, 200, "Find files");

            auto [baseStart, diffStart] = GetBaseFileName(std::min(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get())));
            auto [baseEnd, diffEnd] = GetBaseFileName(std::max(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get())));
        
            //check we have all the necessary files
            std::filesystem::path pathIn("/tmp/in_"+sId);
            std::ofstream ofs;
            ofs.open(pathIn.string());
            if(ofs.is_open())
            {
                auto audioPath = (*itFiles->second.begin()).parent_path();

                for(auto tp  = baseStart; tp <= baseEnd; tp+=std::chrono::minutes(m_nFileLength))
                {
                    auto path = audioPath;
                    path /= std::to_string(tp.count());
                    path.replace_extension(sType);

                    if(itFiles->second.find(path) == itFiles->second.end())
                    {
                        m_server.DownloadFileMessage(sId, 500, "File "+path.stem().string()+" is missing");
                    }
                    ofs << "file '" << path.string() << "'\n";
                }

                ofs.close();
                ConcatFiles(sType, pathIn, sId);
            }
            else
            {
                m_server.DownloadFileMessage(sId, 500, std::string("Could not create file for ffmpeg!"));
            }       
        }
        catch(const std::invalid_argument& e)
        {
            m_server.DownloadFileMessage(sId, 404, std::string("Invalid start or end time"));
        }
        catch(const std::out_of_range& e)
        {
            m_server.DownloadFileMessage(sId, 404, std::string("Invalid start or end time"));
        }

    }
    else
    {
        m_server.DownloadFileMessage(sId, 404, "Logger has no files of that type "+sType);
    }
}

bool LoggerObserver::ConcatFiles(const std::string& sType, const std::filesystem::path& pathIn, const std::string& sId) const
{
    m_server.DownloadFileMessage(sId, 200, "Join files together");

    std::filesystem::path pathOut("/tmp/out_"+sId+"."+sType);
    
    //now use ffmpeg to create a single file from these files....
    std::string sCommand("ffmpeg -f concat -safe 0 -i "+pathIn.string()+ " -c copy "+pathOut.string()+" -nostats -progress - -v panic 2>&1");
    auto pipe = popen(sCommand.c_str(), "r");
    if(!pipe)
    {
        m_server.DownloadFileMessage(sId, 500, "Could not launch FFMPEG: "+sCommand);
        return false;
    }

    std::array<char, 128> buffer;
    std::string sResult;
    Json::Value jsProgress;
    jsProgress["id"] = sId;
    jsProgress["action"] = "progress";
    while(fgets(buffer.data(), 128, pipe) != nullptr)
    {
        sResult += buffer.data();
        //find the last '\n'
        auto nLast = sResult.rfind('\n');
        if(nLast != std::string::npos)
        {
            auto vSplit = SplitString(sResult.substr(0, nLast+1), '\n');
            for(const auto& sLine : vSplit)
            {
                auto nPos = sLine.find('=');
                if(nPos != std::string::npos)
                {
                    auto sKey = sLine.substr(0, nPos);
                    auto sValue = sLine.substr(nPos+1);
                    jsProgress[sKey] = sValue;
                    if(sKey == "progress")
                    {
                        m_server.DownloadFileProgress(jsProgress);
                        jsProgress.clear();
                        jsProgress["id"] = sId;
                        jsProgress["action"] = "progress";
                    }
                }
            }

            if(nLast < sResult.length()-2)
            {
                sResult = sResult.substr(nLast+1);
            }
            else
            {
                sResult.clear();
            }
        }

    }
    auto nCode = pclose(pipe);

    m_server.DownloadFileDone(sId, pathOut.string());
    return true;
}
