#include "loggerobserver.h"
#include "inimanager.h"
#include "observer.h"
#include "jsonconsts.h"
#include "inisection.h"
#include "log.h"
#include "playbackserver.h"
    
using namespace std::placeholders;

LoggerObserver::LoggerObserver(PlaybackServer& server, const std::string& sName, iniManager& config, pml::filewatch::Observer& observer) :
m_server(server),
m_sName(sName)
{
    std::filesystem::path pathAudio = config.Get(jsonConsts::path, jsonConsts::audio, ".");
    

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
                    m_mFiles.insert({key, EnumFiles(path, "."+key)});

                    auto nWatch = observer.AddWatch(path, pml::filewatch::Observer::CREATED | pml::filewatch::Observer::DELETED, false);
                    if(nWatch != 0)
                    {
                        m_mWatches.insert({nWatch, key});
                        observer.AddWatchHandler(nWatch, std::bind(&LoggerObserver::OnFileCreated, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::CREATED);
                        observer.AddWatchHandler(nWatch, std::bind(&LoggerObserver::OnFileDeleted, this, _1, _2, _3, _4), pml::filewatch::Observer::enumWatch::DELETED);
                    }
                }
            }
            catch(const std::exception& e)
            {
                pmlLog(pml::LOG_DEBUG) << m_sName << " - Could not decode keep value " << key << "=" << value;
            }
        }
    }
    else
    {
        pmlLog(pml::LOG_ERROR) << m_sName << " - could not find keep section in ini file";
    }
}



std::set<std::filesystem::path> LoggerObserver::EnumFiles(const std::filesystem::path& path, const std::string& sExt)
{
    pmlLog(pml::LOG_INFO) << m_sName << " - enum " << path;

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
    catch(const std::exception& e)
    {
        pmlLog(pml::LOG_ERROR) << m_sName << " - Failed to enum " << path << ": " << e.what();
        std::cerr << e.what() << '\n';
    }
    return setFiles;
}


void LoggerObserver::OnFileCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    auto itType = m_mWatches.find(nWd);
    if(itType != m_mWatches.end())
    {
        auto itFiles = m_mFiles.find(itType->second);
        if(itFiles != m_mFiles.end())
        {
            itFiles->second.insert(path);
            m_server.FileCreated(m_sName, path);

            pmlLog(pml::LOG_INFO) << m_sName << " - " << path << " created";
        }
        else
        {
            pmlLog(pml::LOG_WARN) << m_sName << " - " << path << " created but not expected type";
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN) << m_sName << " - " << path << " created but watch not found";
    }
}

void LoggerObserver::OnFileDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory)
{
    auto itType = m_mWatches.find(nWd);
    if(itType != m_mWatches.end())
    {
        auto itFiles = m_mFiles.find(itType->second);
        if(itFiles != m_mFiles.end())
        {
            itFiles->second.erase(path);
            m_server.FileDeleted(m_sName, path);

            pmlLog(pml::LOG_INFO) << m_sName << " - " << path << " deleted";
        }
        else
        {
            pmlLog(pml::LOG_WARN) << m_sName << " - " << path << " deleted but not expected type";
        }
    }
    else
    {
        pmlLog(pml::LOG_WARN) << m_sName << " - " << path << " deleted but watch not found";
    }
}


pml::restgoose::response LoggerObserver::CreateDownloadFile(const std::string& sType, const query& theQuery)
{
    auto itFiles = GetEncodedFiles().find(sType);
    if(itFiles != GetEncodedFiles().end())
    {
        auto itStart = theQuery.find(queryKey("start_time"));
        auto itEnd = theQuery.find(queryKey("end_time"));
        if(itStart == theQuery.end() || itEnd == theQuery.end())
        {
            return pml::restgoose::response(400, "No start time or end time sent");
        }

        try
        {
            auto nStart = std::stoul(itStart->second.Get());
            auto nEnd = std::stoul(itEnd->second.Get());
            auto tpStart = std::chrono::system_clock::from_time_t(time_t(std::min(nStart, nEnd)));
            auto tpEnd = std::chrono::system_clock::from_time_t(time_t(std::max(nStart, nEnd)));

            //check we have all the necessary files
            std::vector<std::string> vFiles;

            for(auto tp  = tpStart; tp <= tpEnd; tp+=std::chrono::minutes(1))
            {
                auto sFile = ConvertTimeToString(tp, "%Y-%m-%DT%H-%M");
                if(itFiles->second.find(sFile) == itFiles->second.end())
                {
                    return pml::restgoose::response(500, "File "+sFile+" is missing");
                }
                vFiles.push_back(sFile);
            }

            //now use ffmpeg to create a single file from these files....

        }
        catch(const std::exception& e)
        {
            return pml::restgoose::response(404, "Invalid start or end time");
        }
        

    }
    else
    {
        return pml::restgoose::response(404, "Logger has no files of that type "+sType);
    }
}