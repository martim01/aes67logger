#include "soundfile.h"
#include "timedbuffer.h"
#include "log.h"

SoundFile::SoundFile()=default;

SoundFile::~SoundFile()
{
    Close();
}

bool SoundFile::Close()
{
    if(m_pHandle)
    {
        pmlLog() << "Closed sound file '" << m_sFilename << "'";
        m_sFilename.clear();
        m_pHandle = nullptr;
    }
    return true;
}


bool SoundFile::OpenToWrite(const std::string& sFileName, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength)
{
    pmlLog() << "Attempt to open Sound File '" << sFileName << "' channels=" << nChannels << " sampleRate=" << nSampleRate;

    m_sFilename = sFileName;

    int nFormat = SF_FORMAT_WAVEX;
    switch(nBitLength)
    {
        case 16:
            nFormat |= SF_FORMAT_PCM_16;
            break;
        case 24:
            nFormat |= SF_FORMAT_PCM_24;
            break;
        case 32:
            nFormat |= SF_FORMAT_PCM_32;
            break;
        default:
            nFormat |= SF_FORMAT_FLOAT;
    }

    m_pHandle = std::make_unique<SndfileHandle>(sFileName.c_str(), SFM_WRITE, nFormat, nChannels, nSampleRate);
    m_nWritten = 0;
    if(m_pHandle == nullptr)
    {
        pmlLog(pml::LOG_WARN) << "Could not open the sound file for writing";
    }

    return (m_pHandle != nullptr);
}

bool SoundFile::WriteAudio(std::shared_ptr<pml::aoip::timedbuffer> pBuffer)
{
    if(m_pHandle)
    {
        m_nWritten += m_pHandle->write(pBuffer->GetBuffer().data(), pBuffer->GetBufferSize());
        return true;
    }
    return false;
}


unsigned int SoundFile::GetLength() const
{
    if(m_pHandle)
    {
        return  m_nWritten;
    }
    return 0;
}


