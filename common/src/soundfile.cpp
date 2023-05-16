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
        pmlLog() << "Closed sound file '" << m_file.string() << "'";
        m_file.clear();
        m_pHandle = nullptr;
    }
    return true;
}


bool SoundFile::OpenToWrite(const std::filesystem::path& path, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength)
{
    pmlLog() << "Attempt to open Sound File '" << path.string() << "' channels=" << nChannels << " sampleRate=" << nSampleRate;

    m_file = path;

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

    m_pHandle = std::make_unique<SndfileHandle>(m_file.string().c_str(), SFM_WRITE, nFormat, nChannels, nSampleRate);
    m_nWritten = 0;
    if(m_pHandle == nullptr)
    {
        pmlLog(pml::LOG_WARN) << "Could not open the sound file for writing";
    }

    return (m_pHandle != nullptr);
}

bool SoundFile::OpenToWriteFlac(const std::filesystem::path& path, unsigned short nChannels, unsigned long nSampleRate, unsigned short nBitLength)
{
    pmlLog() << "Attempt to open Sound File '" << path.string() << "' channels=" << nChannels << " sampleRate=" << nSampleRate;

    m_file = path;

    int nFormat = SF_FORMAT_FLAC;
    switch(nBitLength)
    {
        case 16:
            nFormat |= SF_FORMAT_PCM_16;
            break;
        case 32:
            nFormat |= SF_FORMAT_PCM_32;
            break;
        default:
            nFormat |= SF_FORMAT_PCM_24;
            break;

    }

    m_pHandle = std::make_unique<SndfileHandle>(m_file.string().c_str(), SFM_WRITE, nFormat, nChannels, nSampleRate);
    m_nWritten = 0;
    if(m_pHandle == nullptr)
    {
        pmlLog(pml::LOG_WARN) << "Could not open the sound file for writing";
    }

    return (m_pHandle != nullptr);
}

bool SoundFile::OpenToRead(const std::filesystem::path& path)
{
    pmlLog() << "Attempt to open Sound File '" << path.string() << "' to read";
    m_file = path;

    m_pHandle = std::make_unique<SndfileHandle>(m_file.string().c_str());
    if(m_pHandle == nullptr)
    {
        pmlLog(pml::LOG_WARN) << "Could not open the sound file for reading";
    }
    return (m_pHandle != nullptr);
}

bool SoundFile::WriteAudio(std::shared_ptr<pml::aoip::timedbuffer> pBuffer)
{
    return WriteAudio(pBuffer->GetBuffer());
}

bool SoundFile::WriteAudio(const std::vector<float>& vBuffer)
{
    if(m_pHandle)
    {
        m_nWritten += m_pHandle->write(vBuffer.data(), vBuffer.size());
        return true;
    }
    return false;
}

unsigned int SoundFile::GetLengthWritten() const
{
    return m_pHandle ? m_nWritten : 0;
}


bool SoundFile::ReadAudio(std::vector<float>& vBuffer)
{
    if(m_pHandle)
    {
        auto nRead = m_pHandle->read(vBuffer.data(), vBuffer.size());
        vBuffer.resize(nRead);
        return true;
    }
    else
    {
        vBuffer.resize(0);
        return false;
    }
}

int SoundFile::GetFormat() const
{
    return m_pHandle ? m_pHandle->format() : 0;
}
int SoundFile::GetSampleRate() const
{
    return m_pHandle ? m_pHandle->samplerate() : 0;
}

int SoundFile::GetChannelCount() const
{
    return m_pHandle ? m_pHandle->channels() : 0;
}

int SoundFile::GetBitDepth() const
{
    if(m_pHandle)
    {
        switch(m_pHandle->format() & 0xFFFF)
        {
            case SF_FORMAT_PCM_16:
                return 16;
            case SF_FORMAT_PCM_24:
                return 24;
            case SF_FORMAT_PCM_32:
                return 32;

        }
        return 0;
    }
    return -1;
}