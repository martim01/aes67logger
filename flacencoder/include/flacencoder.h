#pragma once
#include "encoder.h"
class FlacEncoder : public Encoder
{
    public:
        FlacEncoder();
        virtual ~FlacEncoder();
    private:

        
        bool InitEncoder() final { return true;}
        bool EncodeFile(const std::filesystem::path& wavFile) final;

};
