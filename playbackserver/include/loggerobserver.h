#pragma once
#include <set>
#include <string>
#include <filesystem>
#include <map>
#include "response.h"

class iniManager;
class PlaybackServer;
namespace pml::filewatch
{
    class Observer;
} // namespace pml::filewatch


using encodedFiles = std::map<std::string, std::set<std::filesystem::path>>;
class LoggerObserver
{
    public:
        LoggerObserver(PlaybackServer& server, const std::string& sName, const iniManager& config, pml::filewatch::Observer& observer);

        const encodedFiles& GetEncodedFiles() const { return m_mFiles;}

        pml::restgoose::response CreateDownloadFile(const std::string& sType, const query& theQuery) const;

    private:

        std::set<std::filesystem::path> EnumFiles(const std::filesystem::path& path, const std::string& sExt) const;
        void OnFileCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);
        void OnFileDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        pml::restgoose::response ConcatFiles(const std::string& sType, const std::filesystem::path& pathIn) const;
        std::pair<std::chrono::minutes, std::chrono::seconds> GetBaseFileName(unsigned long nTime) const;

        PlaybackServer& m_server;
        std::string m_sName;
        encodedFiles m_mFiles;
        std::map<int, std::string> m_mWatches;  
        unsigned long m_nFileLength = 0;      
        
};