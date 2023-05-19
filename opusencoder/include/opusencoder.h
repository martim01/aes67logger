#pragma once
#include "encoder.h"
#include <opusenc.h>


class OpusEncoder : public Encoder
{
    public:
        OpusEncoder();
        virtual ~OpusEncoder();


    private:
        bool InitEncoder() final;
        bool EncodeFile(const std::filesystem::path& wavFile) final;


        OggOpusComments* m_pComments = nullptr;
        OggOpusEnc* m_pEncoder = nullptr;
};
