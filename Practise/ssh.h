#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <fstream>
#include <openssl/sha.h>

class SHA256Hasher {
public:
    SHA256Hasher() { reset(); }
    
    void reset() {
        SHA256_Init(&context);
    }
    
    void update(const std::vector<uint8_t>& data) {
        SHA256_Update(&context, data.data(), data.size());
    }
    
    void update(const std::string& data) {
        SHA256_Update(&context, data.c_str(), data.size());
    }
    
    void update(const void* data, size_t length) {
        SHA256_Update(&context, data, length);
    }
    
    std::vector<uint8_t> digest() {
        std::vector<uint8_t> result(SHA256_DIGEST_LENGTH);
        SHA256_CTX temp_context = context;
        SHA256_Final(result.data(), &temp_context);
        return result;
    }
    
    std::string digest_hex() {
        auto digest_bytes = digest();
        std::stringstream ss;
        for(uint8_t byte : digest_bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
        }
        return ss.str();
    }
    
    static std::string hash_string(const std::string& input) {
        SHA256Hasher hasher;
        hasher.update(input);
        return hasher.digest_hex();
    }
    
    static std::string hash_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        SHA256Hasher hasher;
        std::vector<char> buffer(4096);
        
        while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
            hasher.update(buffer.data(), file.gcount());
        }
        
        return hasher.digest_hex();
    }

private:
    SHA256_CTX context;
};


int main() {
    // Hash a string
    std::string text = "Hello, World!";
    std::string hash1 = SHA256Hasher::hash_string(text);
    std::cout << "Text hash: " << hash1 << std::endl;
    
    // Hash a file
    try {
        std::string file_hash = SHA256Hasher::hash_file("document.txt");
        std::cout << "File hash: " << file_hash << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    };
    
    // Streaming data (useful for large files)
    SHA256Hasher hasher;
    hasher.update("Part 1");
    hasher.update("Part 2");
    hasher.update("Part 3");
    std::string stream_hash = hasher.digest_hex();
    std::cout << "Stream hash: " << stream_hash << std::endl;
    
    return 0;
}