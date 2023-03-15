#include "ntpstatus.h"
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <error.h>
#include <unistd.h>
#include <algorithm>
#include "aes67utils.h"
#include "log.h"

const std::string NtpStatus::clksrcname[10] = {  /* Refer RFC-1305, Appendix B, Section 2.2.1 */
    "unspecified",    /* 0 */
    "atomic clock",   /* 1 */
    "VLF radio",      /* 2 */
    "HF radio",       /* 3 */
    "UHF radio",      /* 4 */
    "local net",      /* 5 */
    "NTP server",     /* 6 */
    "UDP/TIME",       /* 7 */
    "wristwatch",     /* 8 */
    "modem"};         /* 9 */


NtpStatus::NtpStatus() = default;

NtpStatus::~NtpStatus()
{
    Stop();
}


void NtpStatus::Start()
{
    Stop();
    m_pThread = std::make_unique<std::thread>(&NtpStatus::Thread, this);
}

void NtpStatus::Stop()
{
    if(m_pThread)
    {
        m_bRun = false;
        m_pThread->join();
        m_pThread = nullptr;
    }
}

const Json::Value& NtpStatus::GetStatus() const
{
    std::scoped_lock<std::mutex> lg(m_mutex);
    return m_jsStatus;
}

void NtpStatus::Thread()
{
    while(m_bRun)
    {
        if(Connect())
        {
            while(m_bRun)
            {
                if(Send() == false)
                {
                    break;
                }
                if(Receive() == false)
                {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            close(m_sd);
        }
        std::this_thread::sleep_for(std::chrono::seconds(4));
    }
}

bool NtpStatus::Connect()
{
    sockaddr_in sock;
    in_addr address;

    inet_aton("127.0.0.1", &address);
    sock.sin_family = AF_INET;
    sock.sin_addr = address;
    sock.sin_port = htons(NTP_PORT);


    if ((m_sd = socket (PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] = "unable to open socket";
        return false;
    }

    if (connect(m_sd, (struct sockaddr *)&sock, sizeof(sock)) < 0)
    {
        close(m_sd);
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] = "unable to connect to socket";
        return false;
    }
    return true;

}

bool NtpStatus::Send()
{
    ntp ntpmsg;
    memset ( &ntpmsg, 0, sizeof(ntpmsg));
    ntpmsg.byte1 = B1VAL;
    ntpmsg.byte2 = B2VAL;  ntpmsg.sequence=htons(1);


    if (send(m_sd, &ntpmsg, sizeof(ntpmsg), 0) < 0)
    {
        close(m_sd);
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] =  "unable to send command to NTP port";
        return false;
    }
    return true;

}

bool NtpStatus::Receive()
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_sd, &fds);

    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;


    ssize_t n = select (m_sd+1, &fds, nullptr, nullptr , &tv);

    if (n == 0)
    {
        close(m_sd);
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] = "timeout from ntp";
        return false;
    }

    if (n == -1)
    {
        close(m_sd);
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] = "error on select";
        return false;
    }

    ntp ntpmsg;
    memset ( &ntpmsg, 0, sizeof(ntpmsg));

    if (recv(m_sd, &ntpmsg, sizeof(ntpmsg), 0) < 0)
    {
        std::scoped_lock<std::mutex> lg(m_mutex);
        m_jsStatus["error"] = "Unable to talk to NTP daemon. Is it running?";
        close(m_sd);
        return false;
    }

    InterpretMessage(ntpmsg);
    return true;
}

void NtpStatus::InterpretMessage(const ntp& ntpmsg)
{
    std::scoped_lock<std::mutex> lg(m_mutex);
    /*----------------------------------------------------------------------*/
    /* Interpret the received NTP control message */
    /* For the reply message to be valid, the first byte should be as sent,
     and the second byte should be the same, with the response bit set */
    unsigned char byte1ok = ((ntpmsg.byte1&0x3F) == B1VAL);
    unsigned char byte2ok = (char(ntpmsg.byte2 & ~MMASK) == char(B2VAL|RMASK));
    if (!(byte1ok && byte2ok))
    {
        m_jsStatus["error"] = "return data appears to be invalid based on status word";
        return;
    }

    if (!(ntpmsg.byte2 | EMASK))
    {
        m_jsStatus["error"] = "error bit is set in reply";
        return;
    }

    if (!(ntpmsg.byte2 | MMASK))
    {
        m_jsStatus["error"] = "More bit unexpected in reply";
    }

    /* if the leap indicator (LI), which is the two most significant bits
     in status byte1, are both one, then the clock is not synchronised. */
    if ((ntpmsg.status1 >> 6) == 3)
    {
        m_jsStatus["synchronised"] = false;

        /* look at the system event code and see if indicates system restart */
        if ((ntpmsg.status2 & 0x0F) == 1)
        {
            m_jsStatus["restarting"] = true;
        }
        else
        {
            m_jsStatus["restarting"] = false;
        }
    }
    else
    {
        m_jsStatus["synchronised"] = true;

        unsigned int clksrc = (ntpmsg.status1 & 0x3F);
        if (clksrc < 10)
        {
            m_jsStatus["source"] = clksrcname[clksrc];
        }
        else
        {
            m_jsStatus["source"] = "Unknown";
        }

    }
    std::string sMessage = ntpmsg.payload;
    std::replace(sMessage.begin(), sMessage.end(), '\n', ' ');

    std::vector<std::string> vSplit = SplitString(sMessage, ',');

    for(size_t i = 0; i < vSplit.size(); i++)
    {

        std::vector<std::string> vKeyValue = SplitString(trim(vSplit[i]), '=');
        if(vKeyValue.size() == 2)
        {
            trim(vKeyValue[0]);
            trim(vKeyValue[1]);
            
            if(auto llValue = ConvertToLongLong(vKeyValue[1]); llValue)
            {
                m_jsStatus[vKeyValue[0]] = Json::Int64(*llValue);
            }
            else if(auto dValue = ConvertToDouble(vKeyValue[1]); dValue)
            {
                m_jsStatus[vKeyValue[0]] = *dValue;
            }
            else
            {
                m_jsStatus[vKeyValue[0]] = vKeyValue[1];
            }
        }
    }
}


std::optional<double> NtpStatus::ConvertToDouble(const std::string& sValue) const
{
    try
    {
        auto d = std::stod(sValue);
        return d;
    }
    catch(std::invalid_argument&)
    {
        return {};
    }
}

std::optional<long long> NtpStatus::ConvertToLongLong(const std::string& sValue) const
{
    try
    {
        auto n = std::stoll(sValue);
        return true;
    }
    catch(std::invalid_argument& )
    {
        return {};
    }
}


