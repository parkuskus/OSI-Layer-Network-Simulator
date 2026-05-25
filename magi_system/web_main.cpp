#include "cli.hpp"
#include "web/server.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

int main(int argc, char **argv)
{
    std::uint16_t port = 8080;
    if (argc >= 2)
    {
        int parsed = std::atoi(argv[1]);
        if (parsed > 0 && parsed <= 65535)
        {
            port = static_cast<std::uint16_t>(parsed);
        }
    }

    try
    {
        magi::CLI cli;
        std::string loadOutput = cli.executeLine("load topology.json", false);
        if (!loadOutput.empty())
        {
            std::cout << loadOutput;
        }

        magi::WebServer server(cli, ".");
        return server.serve(port) ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
