#include <string>
#include <chrono>
#include "response.h"
#include "aes67utils.h"

/* Session information structure. */
class SessionCookie
{
    public:
        SessionCookie(const userName& user, const ipAddress& peer);
        ~SessionCookie();

        headerValue GetHeaderValue();
        const std::string& GetId() const { return m_id;}

        void Accessed();
    private:
        //uint64_t m_id;
        std::string m_id;
        std::chrono::time_point<std::chrono::system_clock> m_tpCreated;
        std::chrono::time_point<std::chrono::system_clock> m_tpUsed;
        userName m_user;

        static const std::chrono::seconds TTL;
};


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
