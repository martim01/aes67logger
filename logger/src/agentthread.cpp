#include "agentthread.h"
#include <agent_pp/agent++.h>
#include <agent_pp/snmp_group.h>
#include <agent_pp/system_group.h>
#include <agent_pp/snmp_target_mib.h>
#include <agent_pp/snmp_notification_mib.h>
#include <agent_pp/notification_originator.h>
#include <agent_pp/v3_mib.h>
#include <agent_pp/vacm.h>
#include <snmp_pp/oid_def.h>
#include <snmp_pp/mp_v3.h>
#include <snmp_pp/log.h>
#include <thread>
#include <sstream>
#include "log.h"
#include <iomanip>
#include "mibwritabletable.h"

using namespace Snmp_pp;
using namespace Agentpp;

static const char* loggerModuleName = "logger.static_table";

AgentThread::AgentThread(int nPort, int nPortTrap, const std::string& sBaseOid, const std::string& sCommunity) :
    m_nPortTrap(nPortTrap),
    m_sBaseOid(sBaseOid),
    m_sCommunity(sCommunity),
    m_pThread(nullptr)
{
    int nStatus;
    Snmp::socket_startup();  // Initialize socket subsystem
    m_pSnmp = new Snmpx(nStatus, nPort);

    DefaultLog::log()->set_filter(ERROR_LOG,0);
    DefaultLog::log()->set_filter(WARNING_LOG,0);
    DefaultLog::log()->set_filter(EVENT_LOG,0);
    DefaultLog::log()->set_filter(INFO_LOG,0);
    DefaultLog::log()->set_filter(DEBUG_LOG,0);

    if (nStatus == SNMP_CLASS_SUCCESS)
    {
        m_bOK = true;
        pmlLog() << "SNMP Started";
    }
    else
    {
        m_bOK = false;
        pmlLog(pml::LOG_CRITICAL) << "SNMP Failed To Start!";
        return;
    }

    m_pMib = new Mib();
    m_pReqList = new RequestList();

    //@todo community
//    m_pReqList->set_read_community(m_sCommunity.c_str());
//    m_pReqList->set_write_community(m_sCommunity.c_str());

    m_pReqList->set_address_validation(true);

    // register requestList for outgoing requests
    m_pMib->set_request_list(m_pReqList);




}

AgentThread::~AgentThread()
{
    if(m_pThread)
    {
        m_pThread->join();
    }
    delete m_pMib;
    delete m_pSnmp;
    Snmp::socket_cleanup();  // Shut down socket subsystem
}

void AgentThread::Init()
{
    if(!m_bOK) return;

    m_pMib->add(new sysGroup("compi SNMP Agent",m_sBaseOid.c_str(), 10));
    m_pMib->add(new snmpGroup());
    m_pMib->add(new snmp_target_mib());
    m_pMib->add(new snmp_notification_mib());

    m_pTable = new MibWritableTable((m_sBaseOid+".1").c_str());

    m_pMib->add(m_pTable);

    // load persitent objects from disk
    m_pMib->init();

    m_pReqList->set_snmp(m_pSnmp);

}
void AgentThread::AddOid(const std::string& sOid, const std::string& sInitial, std::function<bool(SnmpSyntax*, int)> callback)
{
    if(!m_bOK) return;
    m_pTable->add(MibWritableEntry(Oidx(sOid.c_str()), OctetStr(sInitial.c_str()), callback));
}
void AgentThread::AddOid(const std::string& sOid, int nInitial, std::function<bool(SnmpSyntax*, int)> callback)
{
    if(!m_bOK) return;
    m_pTable->add(MibWritableEntry(Oidx(sOid.c_str()), SnmpInt32(nInitial), callback));
}
void AgentThread::AddOid(const std::string& sOid, unsigned int nInitial, std::function<bool(SnmpSyntax*, int)> callback)
{
    if(!m_bOK) return;
    m_pTable->add(MibWritableEntry(Oidx(sOid.c_str()), SnmpUInt32(nInitial), callback));
}
void AgentThread::AddOid(const std::string& sOid, double dInitial, std::function<bool(SnmpSyntax*, int)> callback)
{
    if(!m_bOK) return;
    m_pTable->add(MibWritableEntry(Oidx(sOid.c_str()), OctetStr(std::to_string(dInitial).c_str()), callback));
}

void AgentThread::Run()
{
    if(!m_bOK) return;
    InitTraps();
    m_pThread = std::make_unique<std::thread>(&AgentThread::ThreadLoop, this);

}

void AgentThread::InitTraps()
{
    //send cold start OID
    Vbx* pVbs = 0;
    coldStartOid coldOid;
    NotificationOriginator no;
    for(const auto& sDestination : m_setTrapDestination)
    {
        std::stringstream ssDest;
        ssDest << sDestination << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());
    }
    no.generate(pVbs, 0, coldOid, "", "");
}

void AgentThread::ThreadLoop()
{

    Request* req;
    while (m_bRun)
    {
	    req = m_pReqList->receive(1);
	    if (req)
        {
            m_pMib->process_request(req);
        }
        else
        {
            m_pMib->cleanup();
        }
    }
    pmlLog(pml::LOG_INFO) << "AgentThread\tExiting";

}

bool AgentThread::UpdateOid(const std::string& sOid, const std::string& sValue)
{
    if(!m_bOK) return false;


    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(sOid.c_str(), true);
    if(pEntry)
    {
        char buffer[1024];
        auto nResult = pEntry->get_value(buffer);
        if(nResult != SNMP_CLASS_SUCCESS)
        {
            pmlLog(pml::LOG_WARN) << "AgentThreat\tOid " << sOid << " is not an octet string";
            return false;
        }

        if(std::string(buffer) != sValue)
        {
  	        pmlLog(pml::LOG_TRACE) << "AgentThread\tOid " << sOid << " changed to '" << sValue << "'";
            pEntry->set_value(sValue.c_str());
            SendTrap(sValue, sOid);
        }
        return true;
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tAudioChanged:  OID Not Found!";
        return false;
    }
}

bool AgentThread::UpdateOid(const std::string& sOid, int nValue)
{
    if(!m_bOK) return false;

    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(sOid.c_str(), true);
    if(pEntry)
    {
        int nCurrent;
        auto nResult = pEntry->get_value(nCurrent);
        if(nResult != SNMP_CLASS_SUCCESS)
        {
            pmlLog(pml::LOG_WARN) << "AgentThreat\tOid " << sOid << " is not an integer";
            return false;
        }

        if(nCurrent != nValue)
        {
  	        pmlLog(pml::LOG_TRACE) << "AgentThread\tOid " << sOid << " changed to '" << nValue << "'";
            pEntry->set_value(nValue);
            SendTrap(nValue, sOid);
        }
        return true;
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tAudioChanged:  OID Not Found!";
        return false;
    }
}

bool AgentThread::UpdateOid(const std::string& sOid, unsigned int nValue)
{
    if(!m_bOK) return false;

    std::lock_guard<std::mutex> lg(m_mutex);

    MibWritableEntry* pEntry = m_pTable->get(sOid.c_str(), true);
    if(pEntry)
    {
        unsigned int nCurrent;
        auto nResult = pEntry->get_value(nCurrent);
        if(nResult != SNMP_CLASS_SUCCESS)
        {
            pmlLog(pml::LOG_WARN) << "AgentThreat\tOid " << sOid << " is not an unsigned integer";
            return false;
        }

        if(nCurrent != nValue)
        {
  	        pmlLog(pml::LOG_TRACE) << "AgentThread\tOid " << sOid << " changed to '" << nValue << "'";
            pEntry->set_value(nValue);
            SendTrap(nValue, sOid);
        }
        return true;
    }
    else
    {
        pmlLog(pml::LOG_WARN)  << "AgentThread\tAudioChanged:  OID Not Found!";
        return false;
    }
}

bool AgentThread::UpdateOid(const std::string& sOid, double dValue)
{
    if(!m_bOK) return false;
    return UpdateOid(sOid, std::to_string(dValue));
}


void AgentThread::AddTrapDestination(const std::string& sIpAddress)
{
    if(!m_bOK) return;

    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.insert(sIpAddress);
    pmlLog(pml::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " added";
}

void AgentThread::RemoveTrapDestination(const std::string& sIpAddress)
{
    if(!m_bOK) return;

    std::lock_guard<std::mutex> lg(m_mutex);
    m_setTrapDestination.erase(sIpAddress);
    pmlLog(pml::LOG_INFO)  << "AgentThread\tTrap destination " << sIpAddress << " removed";
}


void AgentThread::SendTrap(int nValue, const std::string& sOid)
{

    Vbx* pVbs = new Vbx[1];
    Oidx rdsOid((m_sBaseOid+".2."+sOid).c_str());

    pVbs[0].set_oid((m_sBaseOid+".1."+sOid).c_str());
    pVbs[0].set_value(SnmpInt32(nValue));

    NotificationOriginator no;

    for(const auto& sDestination : m_setTrapDestination)
    {
        std::stringstream ssDest;
        ssDest << sDestination << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());

        pmlLog(pml::LOG_TRACE)  << "AgentThread\tTrap " << sOid << " sent to " << ssDest.str();
    }
    no.generate(pVbs, 1, rdsOid, "", "");

    delete[] pVbs;
}


void AgentThread::SendTrap(const std::string& sValue, const std::string& sOid)
{
    Vbx* pVbs = new Vbx[1];
    Oidx rdsOid((m_sBaseOid+".2."+sOid).c_str());

    pVbs[0].set_oid((m_sBaseOid+".1."+sOid).c_str());
    pVbs[0].set_value(sValue.c_str());

    NotificationOriginator no;


    for(const auto& sDestination : m_setTrapDestination)
    {
        std::stringstream ssDest;
        ssDest << sDestination << "/" << m_nPortTrap;

        UdpAddress dest(ssDest.str().c_str());
        no.add_v2_trap_destination(dest, "start", "start", m_sCommunity.c_str());

        pmlLog(pml::LOG_TRACE)  << "AgentThread\tTrap " << sOid << " sent to " << ssDest.str();
    }
    no.generate(pVbs, 1, rdsOid, "", "");

    delete[] pVbs;
}


