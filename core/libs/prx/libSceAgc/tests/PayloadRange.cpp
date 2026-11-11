#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testRanges() {
    const std::array<std::tuple<std::uint32_t, int, std::ptrdiff_t, std::uint64_t>, 10> rows{{
        {0xc0047600u, 1, 2, 16}, {0xc0001000u, 1, 2, 0}, {0xffff7600u, 1, 2, 0xfffc}, {0xc0047600u, -1, 2, 16},
        {0xc0047600u, 7, 2, 16}, {0xc0021000u, 0, 1, 12}, {0xc0001000u, 0, 1, 4}, {0xfffe1000u, 0, 1, 0xfffc},
        {0xffff1000u, 0, -1, 0}, {0xffff7600u, 0, -1, 0}}};
    for (const auto& [header, type, offset, size] : rows) {
        std::array<std::uint32_t, 2> words{header, 0x12345678u};
        SceAgcMemoryRange range{reinterpret_cast<void*>(std::uintptr_t{0x1000}), 0xdeadu};
        check(sceAgcGetDataPacketPayloadRange(&range, words.data(), type) == 0, "payload range failed");
        void* expected = offset < 0 ? nullptr : static_cast<void*>(words.data() + offset);
        check(range.base == expected, "payload range base mismatch");
        check(range.size == size, "payload range size mismatch");
        check(words[0] == header && words[1] == 0x12345678u, "payload range query modified the packet");
        std::uint32_t* address = nullptr;
        check(sceAgcGetDataPacketPayloadAddress_0090(&address, words.data(), type) == 0 && address == range.base,
              "payload range base differs from the payload address");
        std::uint32_t* unversionedAddress = nullptr;
        check(sceAgcGetDataPacketPayloadAddress(&unversionedAddress, words.data(), type) == 0 && unversionedAddress == address,
              "unversioned payload address differs from versioned payload address");
    }
}

void testAddresses() {
    for (auto query : {sceAgcGetDataPacketPayloadAddress_0090, sceAgcGetDataPacketPayloadAddress}) {
        const std::array<std::tuple<std::uintptr_t, int, std::uintptr_t>, 4> unreadable{{
            {0, 1, 8}, {1, -1, 9}, {std::numeric_limits<std::uintptr_t>::max() - 3, std::numeric_limits<int>::min(), 4},
            {std::numeric_limits<std::uintptr_t>::max(), std::numeric_limits<int>::max(), 7}}};
        for (const auto& [input, type, expected] : unreadable) {
            std::uint32_t* address = nullptr;
            check(query(&address, reinterpret_cast<std::uint32_t*>(input), type) == 0, "nonzero payload type read the packet");
            check(reinterpret_cast<std::uintptr_t>(address) == expected, "nonzero payload type address mismatch");
        }

        const std::array<std::tuple<std::uint32_t, std::size_t, std::size_t, int>, 6> rows{{
            {0, 8, 32, 0}, {0x3ffe0000u, 9, 33, 0}, {0x3fff0000u, 9, 33, 0},
            {0xffff1000u, 8, 8, 0}, {0x12345678u, 9, 10, 0}, {0x3fff0000u, 9, 33, -1}}};
        for (const auto& [header, inputOffset, outputOffset, type] : rows) {
            alignas(8) std::array<unsigned char, 48> storage{};
            storage.fill(0xa5);
            std::memcpy(storage.data() + inputOffset, &header, sizeof(header));
            auto expected = storage;
            auto* input = reinterpret_cast<std::uint32_t*>(storage.data() + inputOffset);
            auto* output = reinterpret_cast<std::uint32_t**>(storage.data() + outputOffset);
            std::uint32_t* address = nullptr;
            if (type != 0 || (header & 0x3fff0000u) != 0x3fff0000u) {
                address = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(input) + (type != 0 ? 8u : 4u));
            }
            std::memcpy(expected.data() + outputOffset, &address, sizeof(address));
            check(query(output, input, type) == 0, "unaligned or overlapping payload address query failed");
            check(storage == expected, "payload address query read or wrote overlapping storage incorrectly");
        }

        std::array<std::uint32_t, 2> words{0xc0047600u, 0x12345678u};
        const auto before = words;
        for (auto invalid : {std::uintptr_t{0}, std::uintptr_t{1}, std::numeric_limits<std::uintptr_t>::max()}) {
            auto* address = words.data();
            expectFailure([&] { query(&address, reinterpret_cast<std::uint32_t*>(invalid), 0); });
            check(address == words.data(), "unreadable packet modified output address");
            expectFailure([&] { query(reinterpret_cast<std::uint32_t**>(invalid), words.data(), 1); });
            check(words == before, "unwritable payload output modified the packet");
        }
    }
}

void testRejections() {
    std::array<std::uint32_t, 2> words{0xc0047600u, 0u};
    SceAgcMemoryRange range{};
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(nullptr, words.data(), 1); });
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, nullptr, 1); });
    auto* misaligned = reinterpret_cast<std::uint32_t*>(reinterpret_cast<unsigned char*>(words.data()) + 1);
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(&range, misaligned, 0); });
    std::array<unsigned char, sizeof(SceAgcMemoryRange) + 8> storage{};
    auto* misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 4);
    if (reinterpret_cast<std::uintptr_t>(misalignedRange) % alignof(SceAgcMemoryRange) == 0) {
        misalignedRange = reinterpret_cast<SceAgcMemoryRange*>(storage.data() + 2);
    }
    expectFailure([&] { sceAgcGetDataPacketPayloadRange(misalignedRange, words.data(), 1); });
}

}

int main() {
    try {
        testRanges();
        testAddresses();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC payload range tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
