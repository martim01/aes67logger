#pragma once
#include "json/json.h"
#include <memory>
#include <mutex>

class AsioServer;
class JsonWriter
{
    public:
        static JsonWriter&  Get();
        void writeToStdOut(const Json::Value& jsValue);
        void writeToSStream(const Json::Value& jsValue, std::stringstream& ss);
        void writeToSocket(Json::Value jsValue, std::shared_ptr<AsioServer> pSocket);

    private:
        JsonWriter();
        ~JsonWriter();
        std::unique_ptr<Json::StreamWriter> m_pWriter;

        std::mutex m_mutex;

};

