#ifndef MAGI_EVENT_LOG_HPP
#define MAGI_EVENT_LOG_HPP

#include <string>
#include <vector>

namespace magi {

struct NetworkEvent {
    std::string timestamp;
    std::string type;      
    std::string source;    
    std::string target;  
    std::string protocol; 
    std::string detail;
};

void logEvent(const std::string& type,
              const std::string& source,
              const std::string& target,
              const std::string& protocol,
              const std::string& detail);

std::vector<NetworkEvent> getEventsSince(size_t sinceIndex);
size_t getEventCount();
void clearEvents();

} // namespace magi

#endif
