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
    std::scoped_lock lg(m_mutex);
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
    std::scoped_lock lg(m_mutex);
    m_pWriter->write(jsValue, &ss);
}

void JsonWriter::writeToSocket(const Json::Value& jsValue, std::shared_ptr<AsioServer> pSocket)
{
    std::scoped_lock lg(m_mutex);
    std::stringstream ss;
    writeToSStream(jsValue, ss);
    ss << std::endl;
    pSocket->write(ss.str());
}
