#pragma once
#include <string>
#include <memory>

namespace pml
{
    namespace aoip
    {
        class timedbuffer;
    }
}

class SndfileHandle;

class SoundFile
{
public:
	SoundFile(void);
	~SoundFile(void);

	bool Close(void);
	unsigned int GetLength() const;


	bool OpenToWrite(const std::string& sFileName, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength);
	bool WriteAudio(std::shared_ptr<pml::aoip::timedbuffer> pBuffer);

	bool IsOpen() const { return (m_pHandle!=nullptr);}

	const std::string& GetFilename() const { return m_sFilename;}
private:
    std::string m_sFilename;
    SndfileHandle* m_pHandle = nullptr;
   unsigned int m_nWritten = 0;
};
