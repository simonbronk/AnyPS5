#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
int APS5_VABI MD4_Init_nid_postfix(void*);
int APS5_VABI MD4_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI MD4_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI MD5_Init_nid_postfix(void*);
int APS5_VABI MD5_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI MD5_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA1_Init_nid_postfix(void*);
int APS5_VABI SHA1_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA1_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA224_Init_nid_postfix(void*);
int APS5_VABI SHA224_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA224_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA256_Init_nid_postfix(void*);
int APS5_VABI SHA256_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA256_Final_nid_postfix(unsigned char*, void*);
unsigned char* APS5_VABI SHA256_nid_postfix(const unsigned char*, std::size_t, unsigned char*);
int APS5_VABI SHA384_Init_nid_postfix(void*);
int APS5_VABI SHA384_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA384_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA512_Init_nid_postfix(void*);
int APS5_VABI SHA512_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA512_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI AES_set_encrypt_key_nid_postfix(const unsigned char*, int, void*);
int APS5_VABI AES_set_decrypt_key_nid_postfix(const unsigned char*, int, void*);
void APS5_VABI AES_encrypt_nid_postfix(const unsigned char*, unsigned char*, const void*);
void APS5_VABI AES_decrypt_nid_postfix(const unsigned char*, unsigned char*, const void*);
void APS5_VABI AES_ecb_encrypt_nid_postfix(const unsigned char*, unsigned char*, const void*, int);
void APS5_VABI AES_cbc_encrypt_nid_postfix(const unsigned char*, unsigned char*, std::size_t, const void*, unsigned char*, int);
void APS5_VABI AES_cfb128_encrypt_nid_postfix(const unsigned char*, unsigned char*, std::size_t, const void*, unsigned char*, int*, int);
void APS5_VABI AES_ctr128_encrypt_nid_postfix(const unsigned char*, unsigned char*, std::size_t, const void*, unsigned char*, unsigned char*, unsigned int*);
int APS5_VABI RAND_bytes_nid_postfix(unsigned char*, int);
int APS5_VABI RAND_status_nid_postfix(void);
void APS5_VABI RAND_add_nid_postfix(const void*, int, double);
}

namespace {

constexpr std::size_t Md4ContextSize = 92;
constexpr std::size_t Md5ContextSize = 92;
constexpr std::size_t Sha1ContextSize = 96;
constexpr std::size_t Sha256ContextSize = 112;
constexpr std::size_t Sha512ContextSize = 216;
constexpr std::size_t AesKeySize = 244;
constexpr unsigned char Guard = 0xA5;

void Require(bool value) { if (!value) std::abort(); }

std::string Hex(const unsigned char* bytes, std::size_t length) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < length; ++i) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    return text;
}

std::string Bytes(const char* hex) {
    std::string bytes;
    for (std::size_t i = 0; hex[i] && hex[i + 1]; i += 2) bytes += static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16));
    return bytes;
}

const unsigned char* Data(const std::string& bytes) { return reinterpret_cast<const unsigned char*>(bytes.data()); }

template<std::size_t ContextSize, typename Init, typename Update, typename Final>
void RequireDigest(Init init, Update update, Final final, std::size_t digestLength, const std::string& message, std::size_t split, const char* expected) {
    alignas(8) unsigned char context[ContextSize + 8];
    std::memset(context, Guard, sizeof(context));
    unsigned char digest[64] = {};
    Require(init(context) == 1);
    Require(update(context, message.data(), split) == 1);
    Require(update(context, message.data() + split, message.size() - split) == 1);
    Require(final(digest, context) == 1);
    Require(Hex(digest, digestLength) == expected);
    for (std::size_t i = ContextSize; i < sizeof(context); ++i) Require(context[i] == Guard);
}

void TestDigests() {
    const std::string abc = "abc";
    const std::string twoBlocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    RequireDigest<Md4ContextSize>(MD4_Init_nid_postfix, MD4_Update_nid_postfix, MD4_Final_nid_postfix, 16, abc, 1, "a448017aaf21d8525fc10ae87aa6729d");
    RequireDigest<Md5ContextSize>(MD5_Init_nid_postfix, MD5_Update_nid_postfix, MD5_Final_nid_postfix, 16, abc, 2, "900150983cd24fb0d6963f7d28e17f72");
    RequireDigest<Sha1ContextSize>(SHA1_Init_nid_postfix, SHA1_Update_nid_postfix, SHA1_Final_nid_postfix, 20, abc, 0, "a9993e364706816aba3e25717850c26c9cd0d89d");
    RequireDigest<Sha1ContextSize>(SHA1_Init_nid_postfix, SHA1_Update_nid_postfix, SHA1_Final_nid_postfix, 20, twoBlocks, 13, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    RequireDigest<Sha256ContextSize>(SHA224_Init_nid_postfix, SHA224_Update_nid_postfix, SHA224_Final_nid_postfix, 28, abc, 1, "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
    RequireDigest<Sha256ContextSize>(SHA256_Init_nid_postfix, SHA256_Update_nid_postfix, SHA256_Final_nid_postfix, 32, abc, 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    RequireDigest<Sha256ContextSize>(SHA256_Init_nid_postfix, SHA256_Update_nid_postfix, SHA256_Final_nid_postfix, 32, twoBlocks, 55, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    RequireDigest<Sha512ContextSize>(SHA384_Init_nid_postfix, SHA384_Update_nid_postfix, SHA384_Final_nid_postfix, 48, abc, 1, "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
    RequireDigest<Sha512ContextSize>(SHA512_Init_nid_postfix, SHA512_Update_nid_postfix, SHA512_Final_nid_postfix, 64, abc, 2, "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

    unsigned char digest[32] = {};
    Require(SHA256_nid_postfix(Data(abc), abc.size(), digest) == digest);
    Require(Hex(digest, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const unsigned char* shared = SHA256_nid_postfix(Data(abc), abc.size(), nullptr);
    Require(shared != nullptr && Hex(shared, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    Require(SHA256_nid_postfix(Data(twoBlocks), twoBlocks.size(), nullptr) == shared);
}

void RequireBlock(const char* keyHex, int bits, const char* plainHex, const char* cipherHex) {
    const std::string key = Bytes(keyHex);
    const std::string plain = Bytes(plainHex);
    alignas(8) unsigned char encryptKey[AesKeySize + 8];
    alignas(8) unsigned char decryptKey[AesKeySize + 8];
    std::memset(encryptKey, Guard, sizeof(encryptKey));
    std::memset(decryptKey, Guard, sizeof(decryptKey));
    Require(AES_set_encrypt_key_nid_postfix(Data(key), bits, encryptKey) == 0);
    Require(AES_set_decrypt_key_nid_postfix(Data(key), bits, decryptKey) == 0);
    for (std::size_t i = AesKeySize; i < sizeof(encryptKey); ++i) Require(encryptKey[i] == Guard && decryptKey[i] == Guard);
    unsigned char cipher[16] = {};
    unsigned char back[16] = {};
    AES_encrypt_nid_postfix(Data(plain), cipher, encryptKey);
    Require(Hex(cipher, 16) == cipherHex);
    AES_decrypt_nid_postfix(cipher, back, decryptKey);
    Require(std::memcmp(back, plain.data(), 16) == 0);
    std::memset(cipher, 0, sizeof(cipher));
    AES_ecb_encrypt_nid_postfix(Data(plain), cipher, encryptKey, 1);
    Require(Hex(cipher, 16) == cipherHex);
    AES_ecb_encrypt_nid_postfix(cipher, back, decryptKey, 0);
    Require(std::memcmp(back, plain.data(), 16) == 0);
}

void TestAes() {
    RequireBlock("000102030405060708090a0b0c0d0e0f", 128, "00112233445566778899aabbccddeeff", "69c4e0d86a7b0430d8cdb78070b4c55a");
    RequireBlock("000102030405060708090a0b0c0d0e0f1011121314151617", 192, "00112233445566778899aabbccddeeff", "dda97ca4864cdfe06eaf70a0ec0d7191");
    RequireBlock("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", 256, "00112233445566778899aabbccddeeff", "8ea2b7ca516745bfeafc49904b496089");
    RequireBlock("2b7e151628aed2a6abf7158809cf4f3c", 128, "6bc1bee22e409f96e93d7e117393172a", "3ad77bb40d7a3660a89ecaf32466ef97");

    const std::string key = Bytes("2b7e151628aed2a6abf7158809cf4f3c");
    const std::string plain = Bytes("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    alignas(8) unsigned char encryptKey[AesKeySize];
    alignas(8) unsigned char decryptKey[AesKeySize];
    Require(AES_set_encrypt_key_nid_postfix(Data(key), 128, encryptKey) == 0);
    Require(AES_set_decrypt_key_nid_postfix(Data(key), 128, decryptKey) == 0);
    unsigned char out[32] = {};
    unsigned char back[32] = {};

    std::string iv = Bytes("000102030405060708090a0b0c0d0e0f");
    AES_cbc_encrypt_nid_postfix(Data(plain), out, 32, encryptKey, reinterpret_cast<unsigned char*>(iv.data()), 1);
    Require(Hex(out, 32) == "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2");
    Require(Hex(Data(iv), 16) == "5086cb9b507219ee95db113a917678b2");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    AES_cbc_encrypt_nid_postfix(out, back, 32, decryptKey, reinterpret_cast<unsigned char*>(iv.data()), 0);
    Require(std::memcmp(back, plain.data(), 32) == 0);

    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    int num = 0;
    AES_cfb128_encrypt_nid_postfix(Data(plain), out, 5, encryptKey, reinterpret_cast<unsigned char*>(iv.data()), &num, 1);
    Require(num == 5);
    AES_cfb128_encrypt_nid_postfix(Data(plain) + 5, out + 5, 27, encryptKey, reinterpret_cast<unsigned char*>(iv.data()), &num, 1);
    Require(num == 0);
    Require(Hex(out, 32) == "3b3fd92eb72dad20333449f8e83cfb4ac8a64537a0b3a93fcde3cdad9f1ce58b");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    num = 0;
    AES_cfb128_encrypt_nid_postfix(out, back, 32, encryptKey, reinterpret_cast<unsigned char*>(iv.data()), &num, 0);
    Require(std::memcmp(back, plain.data(), 32) == 0);

    std::string counter = Bytes("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    unsigned char keystream[16] = {};
    unsigned int position = 0;
    AES_ctr128_encrypt_nid_postfix(Data(plain), out, 7, encryptKey, reinterpret_cast<unsigned char*>(counter.data()), keystream, &position);
    Require(position == 7);
    AES_ctr128_encrypt_nid_postfix(Data(plain) + 7, out + 7, 25, encryptKey, reinterpret_cast<unsigned char*>(counter.data()), keystream, &position);
    Require(position == 0);
    Require(Hex(out, 32) == "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff");
    Require(Hex(Data(counter), 16) == "f0f1f2f3f4f5f6f7f8f9fafbfcfdff01");

    alignas(8) unsigned char untouched[AesKeySize];
    std::memset(untouched, Guard, sizeof(untouched));
    Require(AES_set_encrypt_key_nid_postfix(nullptr, 128, untouched) == -1);
    Require(AES_set_encrypt_key_nid_postfix(Data(key), 128, nullptr) == -1);
    Require(AES_set_decrypt_key_nid_postfix(nullptr, 128, untouched) == -1);
    Require(AES_set_encrypt_key_nid_postfix(Data(key), 100, untouched) == -2);
    Require(AES_set_decrypt_key_nid_postfix(Data(key), 0, untouched) == -2);
    for (unsigned char byte : untouched) Require(byte == Guard);
}

void TestRand() {
    Require(RAND_status_nid_postfix() == 1);
    RAND_add_nid_postfix("seed", 4, 4.0);
    unsigned char first[64] = {};
    unsigned char second[64] = {};
    Require(RAND_bytes_nid_postfix(first, sizeof(first)) == 1);
    Require(RAND_bytes_nid_postfix(second, sizeof(second)) == 1);
    Require(std::memcmp(first, second, sizeof(first)) != 0);
    unsigned char zero[64] = {};
    Require(std::memcmp(first, zero, sizeof(first)) != 0);
    unsigned char guard = Guard;
    Require(RAND_bytes_nid_postfix(&guard, 0) == 1 && guard == Guard);
}

}

int main() {
    TestDigests();
    TestAes();
    TestRand();
}
