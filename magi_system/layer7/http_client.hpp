#ifndef MAGI_LAYER7_HTTP_CLIENT_HPP
#define MAGI_LAYER7_HTTP_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>

namespace magi
{
    class Host;

    class HTTPClient
    {
    public:
        static void get(std::shared_ptr<Host> sourceHost, const std::string &url, const std::vector<std::shared_ptr<Host>> &allHosts);
    };
}

#endif // MAGI_LAYER7_HTTP_CLIENT_HPP
