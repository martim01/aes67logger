#include "jsonwriter.h"
#include <iostream>
#include "asiosession.h"


JsonWriter&  JsonWriter::Get()
{
    static JsonWriter json;
    return json;
}

JsonWriter::~JsonWriter()=default;


void JsonWriter::writeToStdOut(const Json::Value& jsValue)
{
    m_pWriter->write(jsValue, &std::cout);
    std::cout <<  std::endl;
}

JsonWriter::JsonWriter()
{
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    m_pWriter = std::unique_ptr<Json::StreamWriter>(builder.newStreamWriter());
}


void JsonWriter::writeToSStream(const Json::Value& jsValue, std::stringstream& ss)
{
    m_pWriter->write(jsValue, &ss);
}

void JsonWriter::writeToSocket(Json::Value jsValue, std::shared_ptr<AsioServer> pSocket)
{
    std::stringstream ss;
    writeToSStream(jsValue, ss);
    ss << std::endl;
    pSocket->write(ss.str());
}
