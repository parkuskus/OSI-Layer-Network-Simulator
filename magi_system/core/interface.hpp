#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace magi {

// Forward declarations
class Node;
class Link;

// Interface merepresentasikan port/kartu jaringan pada sebuah Node
class Interface : public std::enable_shared_from_this<Interface> {
private:
    Node* node;                    // Pointer ke Node pemilik
    uint32_t portNumber;           // Nomor port pada node
    std::string macAddress;        // MAC Address (format: AA:BB:CC:DD:EE:FF)
    std::shared_ptr<Link> link;    // Koneksi ke Link (kabel)

public:
    // Constructor
    Interface(Node* node, uint32_t portNumber, const std::string& macAddress);
    
    // Getters
    Node* getNode() const { return node; }
    uint32_t getPortNumber() const { return portNumber; }
    std::string getMacAddress() const { return macAddress; }
    std::shared_ptr<Link> getLink() const { return link; }
    
    // Setters
    void setLink(std::shared_ptr<Link> newLink) { link = newLink; }
    void setMacAddress(const std::string& mac) { macAddress = mac; }
    
    // Mengirim data melalui link yang terhubung
    void send(const std::vector<uint8_t>& rawBytes);
    
    // Menerima data dari link (akan diteruskan ke Node)
    void receive(const std::vector<uint8_t>& rawBytes);
    
    // Cek apakah interface terhubung ke link
    bool isConnected() const { return link != nullptr; }
    
    // Generate MAC Address unik secara otomatis
    static std::string generateMacAddress();
};

} // namespace magi

#endif // INTERFACE_HPP
