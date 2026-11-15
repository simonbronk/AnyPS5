#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libSceJxr/Factory.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JXR file stream check failed at line %u\n", location.line());
    std::abort();
}

constexpr const char* Path = "guest_jxr_file_stream.bin";
constexpr std::array<std::uint8_t, 10> Data {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
constexpr std::int64_t FileError = -102;

Jxr::Stream Open(const char* mode) {
    Jxr::Stream stream {};
    auto* file = fopen_nid_postfix(Path, mode);
    Require(file != nullptr);
    stream.state[0] = reinterpret_cast<std::uintptr_t>(file);
    return stream;
}

FileStream* File(const Jxr::Stream& stream) { return reinterpret_cast<FileStream*>(stream.state[0]); }

void Close(Jxr::Stream& stream) { Require(fclose_nid_postfix(File(stream)) == 0); }

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void FactoryStream() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-jxr-stream-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    AddPathAlias_nid_no_patch("/jxr-file-stream", root.string().c_str());
    constexpr const char* GuestPath = "/jxr-file-stream/photo.jxr";

    Jxr::Factory* factory = nullptr;
    Require(PKCreateFactory_nid_postfix(reinterpret_cast<void**>(&factory), 0x0101) == 0 && factory != nullptr);

    Jxr::Stream* stream = nullptr;
    Require(factory->createStreamFromFilename(&stream, GuestPath, "w+b") == 0 && stream != nullptr);
    Require(stream->close != nullptr && stream->end != nullptr && stream->read != nullptr && stream->write != nullptr);
    Require(stream->setPosition != nullptr && stream->getPosition != nullptr);
    Require(stream->write(stream, Data.data(), Data.size()) == 0);
    Require(stream->close(&stream) == 0 && stream == nullptr);
    Require(std::filesystem::file_size(root / "photo.jxr") == Data.size());

    Require(factory->createStreamFromFilename(&stream, GuestPath, "rb") == 0 && stream != nullptr);
    std::size_t position = 99;
    Require(stream->getPosition(stream, &position) == 0 && position == 0);
    std::array<std::uint8_t, 10> read {};
    Require(stream->end(stream) == 0);
    Require(stream->read(stream, read.data(), read.size()) == 0 && read == Data);
    std::uint8_t extra = 0;
    Require(stream->read(stream, &extra, 1) == FileError && stream->end(stream) != 0);
    Require(stream->setPosition(stream, 0) == 0 && stream->end(stream) == 0);
    Require(stream->close(&stream) == 0 && stream == nullptr);

    Require(factory->createStreamFromFilename(&stream, "/jxr-file-stream/missing.jxr", "rb") == FileError && stream == nullptr);

    RequireInvalidArgument([&] { factory->createStreamFromFilename(nullptr, GuestPath, "rb"); });
    RequireInvalidArgument([&] { factory->createStreamFromFilename(&stream, nullptr, "rb"); });
    RequireInvalidArgument([&] { factory->createStreamFromFilename(&stream, GuestPath, nullptr); });

    Require(factory->release(&factory) == 0 && factory == nullptr);
    static_cast<void>(std::filesystem::remove_all(root));
    Require(!std::filesystem::exists(root));
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    {
        auto stream = Open("w+b");
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), Data.size()) == 0);
        Require(WriteWS_File_nid_postfix(&stream, nullptr, 0) == 0);
        Require(ftell_nid_postfix(File(stream)) == 10);
        Close(stream);
        stream = Open("rb");
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), 1) == -102);
        Require(WriteWS_File_nid_postfix(&stream, Data.data(), 0) == 0);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::array<std::uint8_t, 10> read {};
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 4) == 0 && std::memcmp(read.data(), Data.data(), 4) == 0);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 0) == -102);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 7) == -102);
        Close(stream);
        stream = Open("rb");
        Require(ReadWS_File_nid_postfix(&stream, read.data(), 10) == 0 && read == Data);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::uint8_t byte = 0;
        Require(SetPosWS_File_nid_postfix(&stream, 6) == 0 && ftell_nid_postfix(File(stream)) == 6);
        Require(ReadWS_File_nid_postfix(&stream, &byte, 1) == 0 && byte == 7);
        Require(SetPosWS_File_nid_postfix(&stream, 0) == 0 && ReadWS_File_nid_postfix(&stream, &byte, 1) == 0 && byte == 1);
        Require(SetPosWS_File_nid_postfix(&stream, SIZE_MAX) == -102);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::size_t position = 99;
        Require(GetPosWS_File_nid_postfix(&stream, &position) == 0 && position == 0);
        Require(SetPosWS_File_nid_postfix(&stream, 9) == 0 && GetPosWS_File_nid_postfix(&stream, &position) == 0 && position == 9);
        bool threw = false;
        try {
            GetPosWS_File_nid_postfix(&stream, reinterpret_cast<std::size_t*>(0x10));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Close(stream);
    }
    {
        auto stream = Open("rb");
        std::array<std::uint8_t, 11> read {};
        Require(EOSWS_File_nid_postfix(&stream) == 0);
        Require(ReadWS_File_nid_postfix(&stream, read.data(), read.size()) == -102 && EOSWS_File_nid_postfix(&stream) != 0);
        Require(SetPosWS_File_nid_postfix(&stream, 0) == 0 && EOSWS_File_nid_postfix(&stream) == 0);
        Close(stream);
        Require(std::remove(Path) == 0);
    }
    FactoryStream();
}
