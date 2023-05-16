#include "loggerobserver.h"
#include "inimanager.h"
#include "observer.h"
#include "jsonconsts.h"
#include "inisection.h"
#include "log.h"
#include "playbackserver.h"
    
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



std::set<std::filesystem::path> LoggerObserver::EnumFiles(const std::filesystem::path& path, const std::string& sExt) const
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
    if(auto itFiles = GetEncodedFiles().find(sType); itFiles != GetEncodedFiles().end() && itFiles->second.empty() == false)
    {
        auto itStart = theQuery.find(queryKey("start_time"));
        auto itEnd = theQuery.find(queryKey("end_time"));
        if(itStart == theQuery.end() || itEnd == theQuery.end())
        {
            return pml::restgoose::response(400, "No start time or end time sent");
        }


        try
        {       
            auto [baseStart, diffStart] = GetBaseFileName(std::min(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get())));
            auto [baseEnd, diffEnd] = GetBaseFileName(std::max(std::stoul(itStart->second.Get()), std::stoul(itEnd->second.Get())));
           
            //check we have all the necessary files
            std::vector<std::filesystem::path> vFiles;

            auto audioPath = (*itFiles->second.begin()).parent_path();

            for(auto tp  = baseStart; tp <= baseEnd; tp+=std::chrono::minutes(m_nFileLength))
            {
                auto path = audioPath;
                path /= std::to_string(tp.count());
                path.replace_extension(sType);

                if(itFiles->second.find(path) == itFiles->second.end())
                {
                    return pml::restgoose::response(500, "File "+path.stem().string()+" is missing");
                }
                vFiles.push_back(path);
            }

            //now use ffmpeg to create a single file from these files....
            return pml::restgoose::response(500, "Files found but not implemented yet!");
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