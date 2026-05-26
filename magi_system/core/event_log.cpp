#include "event_log.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace magi {

    namespace
    {
        std::vector<NetworkEvent> gEvents;

        std::string currentTimestamp()
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time), "%H:%M:%S");
            return ss.str();
        }
    } 

    void logEvent(const std::string &type,
                  const std::string &source,
                  const std::string &target,
                  const std::string &protocol,
                  const std::string &detail)
    {
        NetworkEvent ev;
        ev.timestamp = currentTimestamp();
        ev.type = type;
        ev.source = source;
        ev.target = target;
        ev.protocol = protocol;
        ev.detail = detail;
        gEvents.push_back(ev);
        if (gEvents.size() > 2000)
        {
            gEvents.erase(gEvents.begin());
        }
    }

    std::vector<NetworkEvent> getEventsSince(size_t sinceIndex)
    {
        if (sinceIndex >= gEvents.size())
        {
            return {};
        }
        return std::vector<NetworkEvent>(gEvents.begin() + static_cast<long>(sinceIndex), gEvents.end());
    }

    size_t getEventCount()
    {
        return gEvents.size();
    }

    void clearEvents()
    {
        gEvents.clear();
    }

} 
