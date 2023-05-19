#include "flacencoder.h"
#include "log.h"
#include "jsonwriter.h"
#include "observer.h"
#include "jsonconsts.h"
#include "soundfile.h"
#include "logtofile.h"

using namespace std::placeholders;

FlacEncoder::FlacEncoder() : Encoder(jsonConsts::flac)
{

}
FlacEncoder::~FlacEncoder()
{

}





bool FlacEncoder::EncodeFile(const std::filesystem::path& wavFile)
{
    SoundFile sf;
    SoundFile sfFlac;

    if(sf.OpenToRead(wavFile) == false)
    {
        pmlLog(pml::LOG_ERROR) << "Could not read wav file " << wavFile;

        SendError("Could not read wav file", wavFile);
        return false;
    }
    pmlLog(pml::LOG_DEBUG) << "Opened wav fie " << wavFile;
    auto path = m_pathEncoded;
    path /= wavFile.stem();
    path.replace_extension(".flac");

    if(sfFlac.OpenToWriteFlac(path, sf.GetChannelCount(), sf.GetSampleRate(), sf.GetBitDepth()) == false)
    {
        pmlLog(pml::LOG_ERROR) << "Could not create flac file " << path;

        SendError("Could not create flac file", path);
        return false;
    }

    std::vector<float> vBuffer(m_nBufferSize);
    bool bOk = true;
    do
    {
        if((bOk = sf.ReadAudio(vBuffer)))
        {
            sfFlac.WriteAudio(vBuffer);
        }

    } while (bOk && vBuffer.size() == m_nBufferSize);
    pmlLog() << "Encoded " << path;
    return true;
}
