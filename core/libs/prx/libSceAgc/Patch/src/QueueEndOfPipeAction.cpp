#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(std::uint32_t* cmd, const volatile Label* address) {
    const auto packet = reinterpret_cast<std::uintptr_t>(cmd);
    std::uint8_t opcode = 0;
    if (!GuestCopyIn_nid_no_patch(&opcode, reinterpret_cast<const void*>(packet + 1), sizeof(opcode))) NotImplemented_nid_no_patch(__func__);
    if (opcode != 0x49u) return static_cast<int>(0x8a6c000cu);
    std::uint64_t control = 0;
    if (!GuestCopyIn_nid_no_patch(&control, reinterpret_cast<const void*>(packet + 4), sizeof(control))) NotImplemented_nid_no_patch(__func__);
    if (((control >> 56u) & 7u) == 4u) return 0;
    std::uint32_t lowAddress = 0;
    auto* lowAddressField = reinterpret_cast<void*>(packet + 12);
    if (!GuestCopyIn_nid_no_patch(&lowAddress, lowAddressField, sizeof(lowAddress))) NotImplemented_nid_no_patch(__func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    lowAddress = (lowAddress & 3u) | (static_cast<std::uint32_t>(guestAddress) & ~3u);
    const auto highAddress = static_cast<std::uint32_t>(guestAddress >> 32u);
    if (!GuestCopyOut_nid_no_patch(lowAddressField, &lowAddress, sizeof(lowAddress))) NotImplemented_nid_no_patch(__func__);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(packet + 16), &highAddress, sizeof(highAddress))) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint64_t data) {
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    const auto dataSelect = cmd[2] >> 29u;
    if (((cmd[2] >> 24u) & 7u) == 4u) {
        Agc::Command::Require(data == 0 && (cmd[3] | cmd[4]) == 0, __func__, "interrupt-only release cannot write data");
    } else if (dataSelect == 5u) {
        Agc::Command::Require(data == 0, __func__, "GDS release cannot use immediate data");
        return 0;
    }
    if (dataSelect == 1u) Agc::Command::CheckBits(data, 0xffffffffu, __func__);
    cmd[5] = static_cast<std::uint32_t>(data);
    cmd[6] = static_cast<std::uint32_t>(data >> 32u);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchGcrCntl(std::uint32_t* cmd, std::uint16_t gcrControl) {
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    Agc::Command::CheckBits(gcrControl, 0xfffu, __func__);
    std::uint32_t control = gcrControl;
    if ((control & 0x300u) == 0x100u) {
        control |= 0x200u;
    }
    cmd[1] = (cmd[1] & ~(0xfffu << 12u)) | (control << 12u);
    return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchType(std::uint32_t* cmd, std::uint8_t action) {
    Agc::Command::ValidatePacket(cmd, 0x49u, 8, __func__);
    Agc::Command::CheckBits(action, 0x3fu, __func__);
    const std::uint32_t eventIndex = action >= 0x2fu ? 6u : 5u;
    cmd[1] = (cmd[1] & ~0xf3fu) | action | (eventIndex << 8u);
    return 0;
}

}
