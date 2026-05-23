#ifndef MAGI_LAYER7_DNS_CLIENT_HPP
#define MAGI_LAYER7_DNS_CLIENT_HPP

#include <memory>
#include <string>
#include <vector>

namespace magi
{
    class Host;

    class DNSClient
    {
    public:
        static std::string resolve(std::shared_ptr<Host> sourceHost,
                                   const std::string &name,
                                   const std::vector<std::shared_ptr<Host>> &allHosts,
                                   int timeoutMs = 1000,
                                   int attempts = 2);
    };
}

#endif // MAGI_LAYER7_DNS_CLIENT_HPP