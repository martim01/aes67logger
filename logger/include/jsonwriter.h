#pragma once
#include "json/json.h"
#include <memory>

class JsonWriter
{
    public:
        static JsonWriter&  Get();
        void writeToStdOut(const Json::Value& jsValue);
        void writeToSStream(const Json::Value& jsValue, std::stringstream& ss);

    private:
        JsonWriter();
        std::unique_ptr<Json::StreamWriter> m_pWriter;
};

