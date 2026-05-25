#ifndef MAGI_WEB_SERVER_HPP
#define MAGI_WEB_SERVER_HPP

#include "cli.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace magi
{

    class WebServer
    {
    public:
        WebServer(CLI &cli, const std::string &documentRoot);
        bool serve(std::uint16_t port);

    private:
        typedef std::uintptr_t SocketHandle;

        struct HttpRequest
        {
            std::string method;
            std::string path;
            std::string body;
            std::map<std::string, std::string> headers;
        };

        CLI &cli;
        std::string documentRoot;

        void handleClient(SocketHandle clientFd);
        bool readRequest(SocketHandle clientFd, HttpRequest &request);
        std::string buildResponse(const HttpRequest &request);
        std::string buildApiResponse(const HttpRequest &request);
        std::string buildStaticResponse(const HttpRequest &request);

        std::string readFile(const std::string &path) const;
        std::string contentTypeFor(const std::string &path) const;
        std::string sanitizePath(const std::string &requestPath) const;
        std::string extractJsonString(const std::string &json, const std::string &key) const;
        std::string jsonEscape(const std::string &value) const;
        bool commandLooksSuccessful(const std::string &output) const;
        std::string httpResponse(int statusCode,
                                 const std::string &statusText,
                                 const std::string &contentType,
                                 const std::string &body) const;
    };

} // namespace magi

#endif
