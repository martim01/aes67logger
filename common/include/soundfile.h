#pragma once
#include <string>
#include <memory>
#include "sndfile.hh"
#include <filesystem>
#include <vector>


class SoundFile
{
public:
	SoundFile(void);
	~SoundFile(void);

	bool Close(void);
	unsigned int GetLengthWritten() const;
	unsigned int GetFileLength() const;

	bool OpenToRead(const std::filesystem::path& path);

	bool OpenToWrite(const std::filesystem::path& path, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength);
	bool WriteAudio(const std::vector<float>& vBuffer);

	bool OpenToWriteFlac(const std::filesystem::path& path, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength);

	bool IsOpen() const { return (m_pHandle!=nullptr);}

	const std::filesystem::path& GetFile() const { return m_file;}

	int GetFormat() const;
	int GetSampleRate() const;
	int GetChannelCount() const;
	int GetBitDepth() const;

	bool ReadAudio(std::vector<float>& vBuffer);

private:
    std::filesystem::path m_file;
    std::unique_ptr<SndfileHandle> m_pHandle = nullptr;
    unsigned int m_nWritten = 0;
};
