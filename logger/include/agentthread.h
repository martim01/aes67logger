#pragma once
#include <mutex>
#include <set>
#include <thread>
#include <atomic>
#include <functional>


namespace Agentpp
{
    class Mib;
    class RequestList;
    class Snmpx;
    class MibWritableTable;
};

namespace Snmp_pp
{
    class SnmpSyntax;
};

extern bool g_bRun;

class AgentThread
{
    public:
        AgentThread(int nPort, int nPortTrap,const std::string& sBaseOid, const std::string& sCommunity="public");
        ~AgentThread();

        bool IsOK() const { return m_bOK;}
        void Init();
        void AddOid(const std::string& sOid, const std::string& sInitial, std::function<bool(Snmp_pp::SnmpSyntax*, int)> callback=nullptr);
        void AddOid(const std::string& sOid, int nInitial, std::function<bool(Snmp_pp::SnmpSyntax*, int)> callback=nullptr);
        void AddOid(const std::string& sOid, unsigned int nInitial, std::function<bool(Snmp_pp::SnmpSyntax*, int)> callback=nullptr);
        void AddOid(const std::string& sOid, double dInitial, std::function<bool(Snmp_pp::SnmpSyntax*, int)> callback=nullptr);

        bool UpdateOid(const std::string& sOid, const std::string& sValue);
        bool UpdateOid(const std::string& sOid, int nValue);
        bool UpdateOid(const std::string& sOid, unsigned int nValue);
        bool UpdateOid(const std::string& sOid, double dValue);

        void Run();
        void Stop() { m_bRun = false;}


        void AddTrapDestination(const std::string& sIpAddress);
        void RemoveTrapDestination(const std::string& sIpAddress);

    private:
        void InitTraps();
        void ThreadLoop();

        void SendTrap(int nValue, const std::string& sOid);
        void SendTrap(const std::string& sValue, const std::string& sOid);


        Agentpp::Snmpx* m_pSnmp;
        Agentpp::Mib* m_pMib;
        Agentpp::RequestList* m_pReqList;
        Agentpp::MibWritableTable* m_pTable;

        std::mutex m_mutex;
        bool m_bOK;

        int m_nPortTrap;
        bool m_bRun = true;

        std::string m_sBaseOid;
        std::string m_sCommunity;

        std::set<std::string> m_setTrapDestination;

        std::unique_ptr<std::thread> m_pThread;

};

