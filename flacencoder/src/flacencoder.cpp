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
FlacEncoder::~FlacEncoder()=default;



bool FlacEncoder::EncodeFile(const std::filesystem::path& wavFile)
{
    SoundFile sf;
    SoundFile sfFlac;

    if(sf.OpenToRead(wavFile) == false)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not read wav file " << wavFile;

        SendError("Could not read wav file", wavFile);
        return false;
    }
    pmlLog(pml::LOG_DEBUG, "aes67") << "Opened wav fie " << wavFile;
    auto path = GetPathEncoded();
    path /= wavFile.stem();
    path.replace_extension(".flac");

    if(sfFlac.OpenToWriteFlac(path, sf.GetChannelCount(), sf.GetSampleRate(), sf.GetBitDepth()) == false)
    {
        pmlLog(pml::LOG_ERROR, "aes67") << "Could not create flac file " << path;

        SendError("Could not create flac file", path);
        return false;
    }

    std::vector<float> vBuffer(GetBufferSize());
    bool bOk = true;
    do
    {
        if((bOk = sf.ReadAudio(vBuffer)))
        {
            sfFlac.WriteAudio(vBuffer);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50)); //sleep to reduce cpu load
    } while (bOk && vBuffer.size() == GetBufferSize());
    pmlLog(pml::LOG_INFO, "aes67") << "Encoded " << path;

    FileEncoded(wavFile);
    return true;
}
