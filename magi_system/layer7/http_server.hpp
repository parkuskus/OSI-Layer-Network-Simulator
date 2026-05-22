#ifndef MAGI_LAYER7_HTTP_SERVER_HPP
#define MAGI_LAYER7_HTTP_SERVER_HPP

#include <string>
#include <memory>
#include <vector>

namespace magi
{
    class Host;
    class MagiSocket;

    class HTTPServer
    {
    private:
        Host *host;
        std::string filename;
        std::shared_ptr<MagiSocket> serverSocket;
        bool running;

        std::string getFileContent(const std::string &path);
        std::string generateResponse(const std::string &requestStr);

    public:
        HTTPServer(Host *host, const std::string &filename = "index.html");
        ~HTTPServer();

        bool start();
        void stop();
        void tick();

        bool isRunning() const { return running; }
        std::string getFilename() const { return filename; }
    };
}

#endif // MAGI_LAYER7_HTTP_SERVER_HPP
