#include "opusencoder.h"
#include "log.h"
#include "jsonwriter.h"
#include "observer.h"
#include "jsonconsts.h"
#include "soundfile.h"
#include "logtofile.h"

using namespace std::placeholders;

OpusEncoder::OpusEncoder() : Encoder(jsonConsts::opus)
{

}

OpusEncoder::~OpusEncoder()
{
    if(m_pEncoder)
    {
        ope_encoder_drain( m_pEncoder );
	    ope_encoder_destroy(m_pEncoder);
        m_pEncoder = nullptr;
    }
    if(m_pComments)
	{
        ope_comments_destroy(m_pComments);
        m_pComments = nullptr;
    }
}





bool OpusEncoder::InitEncoder()
{
    m_pComments = ope_comments_create();
	if( !m_pComments )
	{
        pmlLog(pml::LOG_CRITICAL) << "Could not allocate opus comments";
        return false;
	}
    return true;
}

bool OpusEncoder::EncodeFile(const std::filesystem::path& wavFile)
{
    SoundFile sf;
    if(sf.OpenToRead(wavFile) == false)
    {
        pmlLog(pml::LOG_ERROR) << "Could not read wav file " << wavFile;

        SendError("Could not read wav file", wavFile);
        return false;
    }
    pmlLog(pml::LOG_DEBUG) << "Opened wav fie " << wavFile;
    auto path = m_pathEncoded;
    path /= wavFile.stem();
    path.replace_extension(".opus");

    m_nFileEncoded = 0;

    if(m_pEncoder == nullptr)
    {
        m_pEncoder = ope_encoder_create_file(path.string().c_str(), m_pComments, sf.GetSampleRate(), sf.GetChannelCount(), 0, nullptr);
        if(m_pEncoder == nullptr)
        {
            pmlLog(pml::LOG_ERROR) << "Failed to open encoder";

            SendError("Could not create encoder", wavFile);
            return false;
        }
        pmlLog(pml::LOG_DEBUG) << "Opened encoder";
    }
    else if(ope_encoder_continue_new_file(m_pEncoder, path.string().c_str(), m_pComments ) != OPE_OK)
    {
        pmlLog(pml::LOG_ERROR) << "Failed to open encoder for new file";
        SendError("Could not reopen encoder", wavFile);
        return false;
    }
    else
    {
	    pmlLog(pml::LOG_DEBUG) << "Continue new file " << path;
    }

    auto tpStart = std::chrono::system_clock::now();

    std::vector<float> vBuffer(m_nBufferSize);
    bool bOk = true;
    do
    {
        if((bOk = sf.ReadAudio(vBuffer)))
        {
            m_nFileEncoded += vBuffer.size();

            if(ope_encoder_ctl(m_pEncoder, OPUS_SET_LSB_DEPTH(24)) != OPE_OK)
            {
                pmlLog(pml::LOG_WARN) << "Failed to set LSB_DEPTH, continuing anyway...";
                SendError("Failed to set LSB_DEPTH", wavFile);
            }
            if(ope_encoder_write_float(m_pEncoder, vBuffer.data(), vBuffer.size()/2) != OPE_OK)
            {
                pmlLog(pml::LOG_ERROR) << "Failed to encode file " << wavFile.string();
                SendError("Failed to encode file", wavFile);
                bOk = false;
            }
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now()-tpStart);
        if(elapsed.count() > 2)
        {
            OutputEncodedStats(wavFile, static_cast<double>(m_nFileEncoded)/static_cast<double>(sf.GetFileLength()));
            tpStart = std::chrono::system_clock::now();
        }

    } while (bOk && vBuffer.size() == m_nBufferSize);
    pmlLog() << "Encoded " << path;

    m_nFilesEncoded++;
    return true;
}
