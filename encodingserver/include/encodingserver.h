#pragma once
#include "launchmanager.h"
#include "server.h"


class EncodingServer : public Server
{
    public:
        EncodingServer();

        pml::restgoose::response GetApi(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;

        pml::restgoose::response GetEncoders(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetEncoder(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;

        pml::restgoose::response GetStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const final;

        pml::restgoose::response GetEncoderConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response GetEncodeStatus(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;
        pml::restgoose::response PutEncoderPower(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser);

        pml::restgoose::response GetConfig(const query& theQuery, const postData& vData, const endpoint& theEndpoint, const userName& theUser) const;


        void EncoderCreated(const std::string& sEncoder);
        void EncoderDeleted(const std::string& sEncoder);

    private:

        void Init() final;
        void AddCustomEndpoints() final;
        void DeleteCustomEndpoints() final;
        Json::Value GetVersion() const final;
        Json::Value GetCustomStatusSummary() const final;

        LaunchManager m_launcher;
        
        void AddEncoderEndpoints();
        void AddEncoderEndpoints(const std::string& sEncoder);
        void RemoveEncoderEndpoints(const std::string& sEncoder);


        void EncoderCallback(const std::string& sEncoderId, const std::string& sType, bool bAdded);
        void StatusCallback(const std::string& sEncoderId, const Json::Value& jsStatus);
        void ExitCallback(const std::string& sEncoderId, int nExit, bool bRemove);
        
        static const endpoint EP_ENCODERS;
        static const endpoint EP_WS_ENCODERS;
        
        static const std::string ENCODERS;

};
