#include "web/server.hpp"
#include "core/event_log.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace magi
{
    namespace
    {
#if defined(_WIN32)
        typedef SOCKET NativeSocket;
        typedef int SocketLength;
        const NativeSocket InvalidSocket = INVALID_SOCKET;

        bool initializeSockets()
        {
            WSADATA data;
            int result = WSAStartup(MAKEWORD(2, 2), &data);
            if (result != 0)
            {
                std::cerr << "[web] WSAStartup failed: WSA error " << result << std::endl;
                return false;
            }
            return true;
        }

        void cleanupSockets()
        {
            WSACleanup();
        }

        void closeSocket(NativeSocket socket)
        {
            ::closesocket(socket);
        }

        int lastSocketError()
        {
            return WSAGetLastError();
        }

        bool socketInterrupted(int errorCode)
        {
            return errorCode == WSAEINTR;
        }

        std::string socketErrorMessage(int errorCode)
        {
            std::ostringstream message;
            message << "WSA error " << errorCode;
            return message.str();
        }
#else
        typedef int NativeSocket;
        typedef socklen_t SocketLength;
        const NativeSocket InvalidSocket = -1;

        bool initializeSockets()
        {
            return true;
        }

        void cleanupSockets()
        {
        }

        void closeSocket(NativeSocket socket)
        {
            ::close(socket);
        }

        int lastSocketError()
        {
            return errno;
        }

        bool socketInterrupted(int errorCode)
        {
            return errorCode == EINTR;
        }

        std::string socketErrorMessage(int errorCode)
        {
            return std::strerror(errorCode);
        }
#endif

        class SocketRuntime
        {
        public:
            SocketRuntime() : ready(initializeSockets()) {}
            ~SocketRuntime()
            {
                if (ready)
                    cleanupSockets();
            }

            bool isReady() const
            {
                return ready;
            }

        private:
            bool ready;
        };

        bool setReuseAddress(NativeSocket socket)
        {
            int opt = 1;
#if defined(_WIN32)
            return ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
                                reinterpret_cast<const char *>(&opt), sizeof(opt)) == 0;
#else
            return ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
#endif
        }

        int receiveSocket(NativeSocket socket, char *buffer, size_t length)
        {
#if defined(_WIN32)
            const size_t chunk = std::min(length, static_cast<size_t>(std::numeric_limits<int>::max()));
            return ::recv(socket, buffer, static_cast<int>(chunk), 0);
#else
            ssize_t received = ::recv(socket, buffer, length, 0);
            if (received > static_cast<ssize_t>(std::numeric_limits<int>::max()))
                return std::numeric_limits<int>::max();
            return static_cast<int>(received);
#endif
        }

        int sendSocket(NativeSocket socket, const char *data, size_t length)
        {
            const size_t chunk = std::min(length, static_cast<size_t>(std::numeric_limits<int>::max()));
#if defined(_WIN32)
            return ::send(socket, data, static_cast<int>(chunk), 0);
#else
            ssize_t sent = ::send(socket, data, chunk, 0);
            if (sent > static_cast<ssize_t>(std::numeric_limits<int>::max()))
                return std::numeric_limits<int>::max();
            return static_cast<int>(sent);
#endif
        }
    }

    WebServer::WebServer(CLI &cliRef, const std::string &root)
        : cli(cliRef), documentRoot(root.empty() ? "." : root)
    {
    }

    bool WebServer::serve(std::uint16_t port)
    {
        SocketRuntime runtime;
        if (!runtime.isReady())
            return false;

        NativeSocket serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == InvalidSocket)
        {
            int errorCode = lastSocketError();
            std::cerr << "[web] socket failed: " << socketErrorMessage(errorCode) << std::endl;
            return false;
        }

        setReuseAddress(serverFd);

        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (::bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
        {
            int errorCode = lastSocketError();
            std::cerr << "[web] bind failed on port " << port << ": " << socketErrorMessage(errorCode) << std::endl;
            closeSocket(serverFd);
            return false;
        }

        if (::listen(serverFd, 16) < 0)
        {
            int errorCode = lastSocketError();
            std::cerr << "[web] listen failed: " << socketErrorMessage(errorCode) << std::endl;
            closeSocket(serverFd);
            return false;
        }

        std::cout << "[web] MAGI web dashboard: http://127.0.0.1:" << port << "/gui/index.html" << std::endl;
        std::cout << "[web] API ready: GET /api/topology, POST /api/command" << std::endl;

        while (true)
        {
            sockaddr_in clientAddress;
            SocketLength clientLength = sizeof(clientAddress);
            NativeSocket clientFd = ::accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddress), &clientLength);
            if (clientFd == InvalidSocket)
            {
                int errorCode = lastSocketError();
                if (socketInterrupted(errorCode))
                    continue;
                std::cerr << "[web] accept failed: " << socketErrorMessage(errorCode) << std::endl;
                continue;
            }

            handleClient(static_cast<SocketHandle>(clientFd));
            closeSocket(clientFd);
        }

        closeSocket(serverFd);
        return true;
    }

    void WebServer::handleClient(SocketHandle clientFd)
    {
        HttpRequest request;
        std::string response;
        if (readRequest(clientFd, request))
        {
            response = buildResponse(request);
        }
        else
        {
            response = httpResponse(400, "Bad Request", "application/json",
                                    "{\"ok\":false,\"error\":\"Malformed HTTP request\"}");
        }

        const char *data = response.c_str();
        size_t remaining = response.size();
        while (remaining > 0)
        {
            int sent = sendSocket(static_cast<NativeSocket>(clientFd), data, remaining);
            if (sent <= 0)
                break;
            data += sent;
            remaining -= static_cast<size_t>(sent);
        }
    }

    bool WebServer::readRequest(SocketHandle clientFd, HttpRequest &request)
    {
        std::string raw;
        char buffer[4096];
        size_t headerEnd = std::string::npos;

        while (headerEnd == std::string::npos)
        {
            int received = receiveSocket(static_cast<NativeSocket>(clientFd), buffer, sizeof(buffer));
            if (received <= 0)
                return false;
            raw.append(buffer, static_cast<size_t>(received));
            if (raw.size() > 1024 * 1024)
                return false;
            headerEnd = raw.find("\r\n\r\n");
        }

        std::string headerText = raw.substr(0, headerEnd);
        std::istringstream headerStream(headerText);
        std::string requestLine;
        if (!std::getline(headerStream, requestLine))
            return false;
        if (!requestLine.empty() && requestLine[requestLine.size() - 1] == '\r')
            requestLine.erase(requestLine.size() - 1);

        std::istringstream requestLineStream(requestLine);
        requestLineStream >> request.method >> request.path;
        if (request.method.empty() || request.path.empty())
            return false;

        std::string line;
        while (std::getline(headerStream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            while (!value.empty() && value[0] == ' ')
                value.erase(value.begin());
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            request.headers[key] = value;
        }

        size_t contentLength = 0;
        std::map<std::string, std::string>::const_iterator lenIt = request.headers.find("content-length");
        if (lenIt != request.headers.end())
        {
            contentLength = static_cast<size_t>(std::strtoul(lenIt->second.c_str(), nullptr, 10));
            if (contentLength > 1024 * 1024)
                return false;
        }

        size_t bodyStart = headerEnd + 4;
        while (raw.size() - bodyStart < contentLength)
        {
            int received = receiveSocket(static_cast<NativeSocket>(clientFd), buffer, sizeof(buffer));
            if (received <= 0)
                return false;
            raw.append(buffer, static_cast<size_t>(received));
        }

        request.body = raw.substr(bodyStart, contentLength);
        size_t queryStart = request.path.find('?');
        if (queryStart != std::string::npos)
        {
            request.query = request.path.substr(queryStart + 1);
            request.path = request.path.substr(0, queryStart);
        }
        return true;
    }

    std::string WebServer::buildResponse(const HttpRequest &request)
    {
        if (request.path.find("/api/") == 0)
        {
            return buildApiResponse(request);
        }
        return buildStaticResponse(request);
    }

    std::string WebServer::buildApiResponse(const HttpRequest &request)
    {
        if (request.method == "OPTIONS")
        {
            return httpResponse(204, "No Content", "text/plain", "");
        }

        if (request.method == "GET" && request.path == "/api/health")
        {
            return httpResponse(200, "OK", "application/json",
                                "{\"ok\":true,\"service\":\"magi_web\",\"version\":\"1.0\"}");
        }

        if (request.method == "GET" && request.path == "/api/topology")
        {
            return httpResponse(200, "OK", "application/json", cli.exportTopologyJson());
        }

        if (request.method == "GET" && request.path == "/api/help")
        {
            std::string output = cli.executeLine("help", false);
            std::string body = "{\"ok\":true,\"output\":\"" + jsonEscape(output) + "\"}";
            return httpResponse(200, "OK", "application/json", body);
        }

        if (request.method == "GET" && request.path == "/api/events")
        {
            std::string sinceStr = extractQueryParam(request.query, "since");
            size_t sinceIndex = 0;
            if (!sinceStr.empty())
            {
                try
                {
                    sinceIndex = static_cast<size_t>(std::stoul(sinceStr));
                }
                catch (...)
                {
                    sinceIndex = 0;
                }
            }

            std::vector<magi::NetworkEvent> events = magi::getEventsSince(sinceIndex);
            size_t totalCount = magi::getEventCount();

            std::ostringstream body;
            body << "{\"ok\":true,\"totalCount\":" << totalCount << ",\"events\":[";
            for (size_t i = 0; i < events.size(); ++i)
            {
                if (i > 0)
                    body << ",";
                body << "{";
                body << "\"timestamp\":\"" << jsonEscape(events[i].timestamp) << "\",";
                body << "\"type\":\"" << jsonEscape(events[i].type) << "\",";
                body << "\"source\":\"" << jsonEscape(events[i].source) << "\",";
                body << "\"target\":\"" << jsonEscape(events[i].target) << "\",";
                body << "\"protocol\":\"" << jsonEscape(events[i].protocol) << "\",";
                body << "\"detail\":\"" << jsonEscape(events[i].detail) << "\"";
                body << "}";
            }
            body << "]}";
            return httpResponse(200, "OK", "application/json", body.str());
        }

        if (request.method == "POST" && request.path == "/api/command")
        {
            std::string command = extractJsonString(request.body, "command");
            if (command.empty())
            {
                return httpResponse(400, "Bad Request", "application/json",
                                    "{\"ok\":false,\"error\":\"Missing command\"}");
            }

            std::string output = cli.executeLine(command);
            bool ok = commandLooksSuccessful(output);
            std::ostringstream body;
            body << "{";
            body << "\"ok\":" << (ok ? "true" : "false") << ",";
            body << "\"command\":\"" << jsonEscape(command) << "\",";
            body << "\"output\":\"" << jsonEscape(output) << "\",";
            body << "\"topology\":" << cli.exportTopologyJson();
            body << "}";
            return httpResponse(200, "OK", "application/json", body.str());
        }

        if (request.method == "POST" && request.path == "/api/import-topology")
        {
            const std::string importPath = documentRoot + "/web_import_topology.json";
            std::ofstream file(importPath.c_str(), std::ios::binary);
            if (!file.is_open())
            {
                return httpResponse(500, "Internal Server Error", "application/json",
                                    "{\"ok\":false,\"error\":\"Unable to write imported topology file\"}");
            }
            file << request.body;
            file.close();

            std::string output = cli.executeLine("load web_import_topology.json", false);
            bool ok = commandLooksSuccessful(output);
            std::ostringstream body;
            body << "{";
            body << "\"ok\":" << (ok ? "true" : "false") << ",";
            body << "\"output\":\"" << jsonEscape(output) << "\",";
            body << "\"topology\":" << cli.exportTopologyJson();
            body << "}";
            return httpResponse(ok ? 200 : 400, ok ? "OK" : "Bad Request", "application/json", body.str());
        }

        return httpResponse(404, "Not Found", "application/json",
                            "{\"ok\":false,\"error\":\"Unknown API endpoint\"}");
    }

    std::string WebServer::buildStaticResponse(const HttpRequest &request)
    {
        if (request.method != "GET" && request.method != "HEAD")
        {
            return httpResponse(405, "Method Not Allowed", "text/plain", "Method Not Allowed");
        }

        std::string path = sanitizePath(request.path);
        std::string body = readFile(path);
        if (body.empty())
        {
            return httpResponse(404, "Not Found", "text/plain", "Not Found");
        }

        if (request.method == "HEAD")
        {
            body.clear();
        }
        return httpResponse(200, "OK", contentTypeFor(path), body);
    }

    std::string WebServer::readFile(const std::string &path) const
    {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.is_open())
            return "";
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string WebServer::contentTypeFor(const std::string &path) const
    {
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".html")
            return "text/html; charset=utf-8";
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")
            return "text/css; charset=utf-8";
        if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")
            return "text/javascript; charset=utf-8";
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".json")
            return "application/json; charset=utf-8";
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg")
            return "image/svg+xml";
        return "application/octet-stream";
    }

    std::string WebServer::sanitizePath(const std::string &requestPath) const
    {
        std::string path = requestPath.empty() || requestPath == "/" ? "/gui/index.html" : requestPath;
        if (path.find("..") != std::string::npos)
            return documentRoot + "/gui/index.html";
        while (!path.empty() && path[0] == '/')
            path.erase(path.begin());
        return documentRoot + "/" + path;
    }

    std::string WebServer::extractQueryParam(const std::string &query, const std::string &key) const
    {
        if (query.empty())
            return "";
        std::string pattern = key + "=";
        size_t pos = query.find(pattern);
        if (pos == std::string::npos)
            return "";
        size_t start = pos + pattern.size();
        size_t end = query.find('&', start);
        if (end == std::string::npos)
            return query.substr(start);
        return query.substr(start, end - start);
    }

    std::string WebServer::extractJsonString(const std::string &json, const std::string &key) const
    {
        std::string pattern = "\"" + key + "\"";
        size_t keyPos = json.find(pattern);
        if (keyPos == std::string::npos)
            return "";
        size_t colon = json.find(':', keyPos + pattern.size());
        if (colon == std::string::npos)
            return "";
        size_t quote = json.find('"', colon + 1);
        if (quote == std::string::npos)
            return "";

        std::string value;
        bool escaping = false;
        for (size_t i = quote + 1; i < json.size(); ++i)
        {
            char ch = json[i];
            if (escaping)
            {
                if (ch == 'n')
                    value += '\n';
                else if (ch == 'r')
                    value += '\r';
                else if (ch == 't')
                    value += '\t';
                else
                    value += ch;
                escaping = false;
                continue;
            }
            if (ch == '\\')
            {
                escaping = true;
                continue;
            }
            if (ch == '"')
                return value;
            value += ch;
        }
        return "";
    }

    std::string WebServer::jsonEscape(const std::string &value) const
    {
        std::ostringstream out;
        for (size_t i = 0; i < value.size(); ++i)
        {
            unsigned char ch = static_cast<unsigned char>(value[i]);
            switch (ch)
            {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20)
                {
                    out << "\\u00";
                    const char *hex = "0123456789abcdef";
                    out << hex[(ch >> 4) & 0x0f] << hex[ch & 0x0f];
                }
                else
                {
                    out << value[i];
                }
            }
        }
        return out.str();
    }

    bool WebServer::commandLooksSuccessful(const std::string &output) const
    {
        std::string lower = output;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });

        return lower.find("error:") == std::string::npos &&
               lower.find("perintah tidak dikenal") == std::string::npos &&
               lower.find("request timeout") == std::string::npos &&
               lower.find("destination unreachable") == std::string::npos &&
               lower.find("[tcp] gagal") == std::string::npos &&
               lower.find("[http client] gagal") == std::string::npos &&
               lower.find("gagal terhubung") == std::string::npos;
    }

    std::string WebServer::httpResponse(int statusCode,
                                        const std::string &statusText,
                                        const std::string &contentType,
                                        const std::string &body) const
    {
        std::ostringstream response;
        response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
        response << "Content-Type: " << contentType << "\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        response << "Access-Control-Allow-Headers: Content-Type\r\n";
        response << "Cache-Control: no-store\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << body;
        return response.str();
    }

} // namespace magi
