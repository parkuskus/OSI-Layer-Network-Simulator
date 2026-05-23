#include "layer7/http_server.hpp"
#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"
#include "layer4/tcp_socket.hpp"
#include "layer3/ip_utils.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace magi
{
    HTTPServer::HTTPServer(Host *host, const std::string &filename)
        : host(host), filename(filename), running(false)
    {
    }

    HTTPServer::~HTTPServer()
    {
        if (running)
        {
            stop();
        }
    }

    bool HTTPServer::start()
    {
        if (running || !host)
        {
            return false;
        }

        serverSocket = std::make_shared<MagiSocket>(host, MagiSocket::AF_INET, MagiSocket::SOCK_STREAM);
        if (!serverSocket->bind(host->getIpAddress(), 80))
        {
            std::cout << "[HTTP Server] Gagal melakukan bind ke " << host->getIpAddress() << ":80" << std::endl;
            return false;
        }

        if (!serverSocket->listen(5))
        {
            std::cout << "[HTTP Server] Gagal melakukan listen di port 80" << std::endl;
            return false;
        }

        running = true;
        std::cout << "[HTTP Server] Server berjalan di " << host->getIpAddress() << ":80 (menyajikan: " << filename << ")" << std::endl;
        return true;
    }

    void HTTPServer::stop()
    {
        if (!running)
        {
            return;
        }

        if (serverSocket)
        {
            serverSocket->close();
            serverSocket.reset();
        }

        running = false;
        std::cout << "[HTTP Server] Server dihentikan." << std::endl;
    }

    std::string HTTPServer::getFileContent(const std::string &path)
    {
        // Clean path to extract filename
        std::string target = path;
        if (target == "/" || target.empty())
        {
            target = filename;
        }
        else if (target[0] == '/')
        {
            target = target.substr(1);
        }

        // Try opening the actual file in the filesystem
        std::ifstream file(target);
        if (file.is_open())
        {
            std::stringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }

        // If file not found and the requested path matches our configured filename (or is root), return a premium default page
        if (target == filename || target == "index.html")
        {
            std::stringstream ss;
            ss << "<!DOCTYPE html>\n"
               << "<html>\n"
               << "<head>\n"
               << "  <meta charset=\"UTF-8\">\n"
               << "  <title>MAGI System v4.0 - Caspar-3 Operational</title>\n"
               << "  <style>\n"
               << "    body { background: #0c0703; color: #ff5500; font-family: 'Outfit', 'Courier New', monospace; margin: 0; padding: 40px; text-align: center; }\n"
               << "    .container { max-width: 650px; margin: 40px auto; padding: 30px; border: 2px solid #ff5500; border-radius: 8px; background: rgba(255, 85, 0, 0.05); box-shadow: 0 0 25px rgba(255, 85, 0, 0.15); }\n"
               << "    h1 { font-size: 2.2em; border-bottom: 2px solid #ff5500; padding-bottom: 15px; margin-top: 0; letter-spacing: 2px; text-shadow: 0 0 10px rgba(255, 85, 0, 0.5); }\n"
               << "    .status { color: #00ff66; font-weight: bold; text-shadow: 0 0 8px rgba(0, 255, 102, 0.4); }\n"
               << "    .highlight { color: #ffaa00; }\n"
               << "    p { font-size: 1.1em; line-height: 1.6; color: #ffcc99; }\n"
               << "    .footer { margin-top: 30px; font-size: 0.9em; color: #663311; border-top: 1px solid #331a00; padding-top: 15px; }\n"
               << "  </style>\n"
               << "</head>\n"
               << "<body>\n"
               << "  <div class=\"container\">\n"
               << "    <h1>MAGI-3 SYSTEM DETECTED</h1>\n"
               << "    <p>Simulated Node: <span class=\"highlight\">" << host->getName() << "</span></p>\n"
               << "    <p>Server IP: <span class=\"highlight\">" << host->getIpAddress() << "</span></p>\n"
               << "    <p>Status: <span class=\"status\">OPERATIONAL (milestone_4)</span></p>\n"
               << "    <p>File Served: <code>" << filename << "</code> (Simulated Content)</p>\n"
               << "    <hr style=\"border-color: #552200; margin: 20px 0;\">\n"
               << "    <p><i>\"God's in his heaven, all's right with the world.\"</i></p>\n"
               << "    <div class=\"footer\">NERV Geofront Communications Network Simulator</div>\n"
               << "  </div>\n"
               << "</body>\n"
               << "</html>\n";
            return ss.str();
        }

        return "";
    }

    std::string HTTPServer::generateResponse(const std::string &requestStr)
    {
        std::istringstream iss(requestStr);
        std::string method, path, version;
        iss >> method >> path >> version;

        std::transform(method.begin(), method.end(), method.begin(), ::toupper);

        std::stringstream resp;

        if (method != "GET")
        {
            std::string body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
            resp << "HTTP/1.1 405 Method Not Allowed\r\n"
                 << "Content-Type: text/html\r\n"
                 << "Content-Length: " << body.length() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
            return resp.str();
        }

        std::string content = getFileContent(path);
        if (content.empty())
        {
            std::string body = "<html><body style=\"background-color:#140800; color:#ff3300; font-family:monospace; text-align:center; padding:50px;\">"
                               "<h1>404 Not Found</h1><p>Magi File Server could not locate the requested resource.</p></body></html>";
            resp << "HTTP/1.1 404 Not Found\r\n"
                 << "Content-Type: text/html\r\n"
                 << "Content-Length: " << body.length() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
        }
        else
        {
            resp << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/html\r\n"
                 << "Content-Length: " << content.length() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << content;
        }

        return resp.str();
    }

    void HTTPServer::tick(const std::string &remoteIp, uint16_t remotePort)
    {
        if (!running || !serverSocket)
        {
            return;
        }

        std::shared_ptr<MagiSocket> conn = serverSocket->accept();
        if (!conn)
        {
            return;
        }

        std::cout << "[HTTP Server] Menerima koneksi dari " << remoteIp << ":" << remotePort << std::endl;

        // Receive request data
        std::vector<uint8_t> reqData = conn->recv(65535);
        if (!reqData.empty())
        {
            std::string requestStr(reqData.begin(), reqData.end());
            std::cout << "[HTTP Server] Menerima HTTP Request:\n"
                      << requestStr << std::endl;

            // Generate and send response
            std::string responseStr = generateResponse(requestStr);
            std::vector<uint8_t> respData(responseStr.begin(), responseStr.end());
            conn->send(respData);

            std::cout << "[HTTP Server] Mengirimkan HTTP Response (size: " << responseStr.size() << " bytes)" << std::endl;
        }

        // Complete teardown
        conn->close();

        // Re-listen immediately to restore the server socket back to LISTEN state for subsequent queries
        serverSocket->listen(5);
    }
}
