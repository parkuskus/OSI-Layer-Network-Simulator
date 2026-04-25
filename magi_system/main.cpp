#include "cli.hpp"
#include <iostream>

int main() {
    try {
        magi::CLI cli;
        cli.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
