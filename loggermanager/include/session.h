#include <string>
#include <chrono>
#include "response.h"
#include "aoiputils.h"

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

