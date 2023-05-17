#pragma once
#include <string>
#include <memory>
#include "sndfile.hh"
#include <filesystem>
#include <vector>

namespace pml::aoip
{
	class timedbuffer;
};

class SoundFile
{
public:
	SoundFile(void);
	~SoundFile(void);

	bool Close(void);
	unsigned int GetLengthWritten() const;

	bool OpenToRead(const std::filesystem::path& path);

	bool OpenToWrite(const std::filesystem::path& path, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength);
	bool WriteAudio(std::shared_ptr<pml::aoip::timedbuffer> pBuffer);

	bool IsOpen() const { return (m_pHandle!=nullptr);}

	const std::filesystem::path& GetFile() const { return m_file;}

	int GetFormat() const;
	int GetSampleRate() const;
	int GetChannelCount() const;

	bool ReadAudio(std::vector<float>& vBuffer);

private:
    std::filesystem::path m_file;
    std::unique_ptr<SndfileHandle> m_pHandle = nullptr;
    unsigned int m_nWritten = 0;
};