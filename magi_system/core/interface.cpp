#include "interface.hpp"
#include "link.hpp"
#include "node.hpp"
#include <sstream>
#include <iomanip>
#include <random>

namespace magi {

// Static counter untuk memastikan MAC address unik
static uint64_t macCounter = 0;

std::string Interface::generateMacAddress() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Generate 6 bytes untuk MAC address
    for (int i = 0; i < 6; ++i) {
        if (i > 0) ss << ":";
        // Gunakan counter untuk memastikan unik + random untuk variasi
        int byte = (dis(gen) + macCounter++) % 256;
        ss << std::setw(2) << byte;
    }
    
    return ss.str();
}

Interface::Interface(Node* node, uint32_t portNumber, const std::string& macAddress)
    : node(node), portNumber(portNumber), macAddress(macAddress), link(nullptr) {
}

void Interface::send(const std::vector<uint8_t>& rawBytes) {
    if (link) {
        link->transmit(this, rawBytes);
    }
    // Jika tidak terhubung, data akan di-drop (sesuai perilaku nyata)
}

void Interface::receive(const std::vector<uint8_t>& rawBytes) {
    // Diteruskan ke Node induk untuk di-unpack dari Layer 1 ke Layer 2
    if (node) {
        node->handleReceive(this, rawBytes);
    }
}

} // namespace magi
