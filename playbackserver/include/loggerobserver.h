#pragma once
#include <set>
#include <string>
#include <filesystem>
#include <map>

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
        LoggerObserver(PlaybackServer& server, const std::string& sName, iniManager& config, pml::filewatch::Observer& observer);

        const encodedFiles& GetEncodedFiles() const { return m_mFiles;}

    private:

        void Init(iniManager& config);
        std::set<std::filesystem::path> EnumFiles(const std::filesystem::path& path, const std::string& sExt);
        void OnFileCreated(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);
        void OnFileDeleted(int nWd, const std::filesystem::path& path, uint32_t mask, bool bDirectory);

        PlaybackServer& m_server;
        std::string m_sName;
        encodedFiles m_mFiles;
        std::map<int, std::string> m_mWatches;        
        
};