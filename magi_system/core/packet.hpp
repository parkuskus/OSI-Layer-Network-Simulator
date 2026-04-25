#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <vector>
#include <string>

namespace magi {

// Kelas abstrak dasar untuk semua jenis paket
// Mendukung konversi ke/dari byte-array (serialization/deserialization)
class Packet {
public:
    virtual ~Packet() = default;
    
    // Serialize objek menjadi byte mentah (header + payload)
    virtual std::vector<uint8_t> toBytes() const = 0;
    
    // Deserialize byte mentah menjadi instance objek paket
    virtual void fromBytes(const std::vector<uint8_t>& rawBytes) = 0;
    
    // Mendapatkan tipe paket untuk identifikasi
    virtual std::string getType() const = 0;
    
    // Utility function: convert bytes to hex string for debugging
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
    
    // Utility function: convert hex string to bytes
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
};

} // namespace magi

#endif // PACKET_HPP
