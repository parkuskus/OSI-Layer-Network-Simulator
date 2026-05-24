#include "web/server.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace magi
{

    WebServer::WebServer(CLI &cliRef, const std::string &root)
        : cli(cliRef), documentRoot(root.empty() ? "." : root)
    {
    }

    bool WebServer::serve(uint16_t port)
    {
        int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0)
        {
            std::cerr << "[web] socket failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        int opt = 1;
        ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(port);

        if (::bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
        {
            std::cerr << "[web] bind failed on port " << port << ": " << std::strerror(errno) << std::endl;
            ::close(serverFd);
            return false;
        }

        if (::listen(serverFd, 16) < 0)
        {
            std::cerr << "[web] listen failed: " << std::strerror(errno) << std::endl;
            ::close(serverFd);
            return false;
        }

        std::cout << "[web] MAGI web dashboard: http://127.0.0.1:" << port << "/gui/index.html" << std::endl;
        std::cout << "[web] API ready: GET /api/topology, POST /api/command" << std::endl;

        while (true)
        {
            sockaddr_in clientAddress;
            socklen_t clientLength = sizeof(clientAddress);
            int clientFd = ::accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddress), &clientLength);
            if (clientFd < 0)
            {
                if (errno == EINTR)
                    continue;
                std::cerr << "[web] accept failed: " << std::strerror(errno) << std::endl;
                continue;
            }

            handleClient(clientFd);
            ::close(clientFd);
        }

        ::close(serverFd);
        return true;
    }

    void WebServer::handleClient(int clientFd)
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
            ssize_t sent = ::send(clientFd, data, remaining, 0);
            if (sent <= 0)
                break;
            data += sent;
            remaining -= static_cast<size_t>(sent);
        }
    }

    bool WebServer::readRequest(int clientFd, HttpRequest &request)
    {
        std::string raw;
        char buffer[4096];
        size_t headerEnd = std::string::npos;

        while (headerEnd == std::string::npos)
        {
            ssize_t received = ::recv(clientFd, buffer, sizeof(buffer), 0);
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
            ssize_t received = ::recv(clientFd, buffer, sizeof(buffer), 0);
            if (received <= 0)
                return false;
            raw.append(buffer, static_cast<size_t>(received));
        }

        request.body = raw.substr(bodyStart, contentLength);
        size_t queryStart = request.path.find('?');
        if (queryStart != std::string::npos)
        {
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
