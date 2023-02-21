#include "session.h"
#include "guid.h"
#include <iomanip>
#include <sstream>
#include "log.h"

const std::chrono::seconds SessionCookie::TTL = std::chrono::seconds(30);

static uuid_t NameSpace_OID = { /* 6ba7b812-9dad-11d1-80b4-00c04fd430c8 */
       0x6ba7b812,
       0x9dad,
       0x11d1,
       0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
   };

std::string CreateGuid(const std::string& sName)
{
    uuid_t guid;
    uuid_create_md5_from_name(&guid, NameSpace_OID, sName.c_str(), sName.length());

    std::stringstream ss;
    ss << std::hex
       << std::setw(8) << std::setfill('0') << guid.time_low
       << std::setw(4) << std::setfill('0') << guid.time_mid
       << std::setw(4) << std::setfill('0') << guid.time_hi_and_version
       << std::setw(2) << std::setfill('0') << (int)guid.clock_seq_hi_and_reserved
       << std::setw(2) << std::setfill('0') << (int)guid.clock_seq_low
       << std::setw(2) << std::setfill('0') << (int)guid.node[0]
       << std::setw(2) << std::setfill('0') << (int)guid.node[1]
       << std::setw(2) << std::setfill('0') << (int)guid.node[2]
       << std::setw(2) << std::setfill('0') << (int)guid.node[3]
       << std::setw(2) << std::setfill('0') << (int)guid.node[4]
       << std::setw(2) << std::setfill('0') << (int)guid.node[5];

    return ss.str();
    /*std::array<char,40> output;
    snprintf(output.data(), output.size(), "%08x-%04hx-%04hx-%02x%02x-%02x%02x%02x%02x%02x%02x",
             guid.time_low, guid.time_mid, guid.time_hi_and_version, guid.clock_seq_hi_and_reserved, guid.clock_seq_low,
             guid.node[0], guid.node[1], guid.node[2], guid.node[3], guid.node[4], guid.node[5]);
    return std::string(output.data());*/
}



SessionCookie::SessionCookie(const userName& user, const ipAddress& peer) : m_user(user)
{
    m_tpCreated = std::chrono::system_clock::now();
    m_id = CreateGuid(GetCurrentTimeAsString(true)+peer.Get()+user.Get());
    pmlLog() << "SessionCookie created";
}

void SessionCookie::Accessed()
{
    m_tpUsed = std::chrono::system_clock::now();
}

SessionCookie::~SessionCookie()
{

}

headerValue SessionCookie::GetHeaderValue()
{
    std::stringstream ss;
    ss << "access_token=" << std::hex << m_id;
    return headerValue(ss.str());
}

///* Cleans up sessions that have been idle for too long. */
//void check_sessions() {
//  double threshold = mg_time() - SESSION_TTL;
//  for (int i = 0; i < NUM_SESSIONS; i++) {
//    struct session *s = &s_sessions[i];
//    if (s->id != 0 && s->last_used < threshold) {
//      fprintf(stderr, "Session %" INT64_X_FMT " (%s) closed due to idleness.\n",
//              s->id, s->user);
//      destroy_session(s);
//    }
//  }
//}

