#include <array>
#include <cstdio>
#include <stdexcept>

#include "video_core/amdgpu/pm4_cmds.h"

int main() {
    using namespace AmdGpu;
    std::array<u32, 0xD000> registers{};
    registers.fill(0xccccccccu);

    PM4Type0Header header{};
    header.base.Assign(483);
    header.count.Assign(2); // Three consecutive register payload words.
    const std::array<u32, 5> packet{header.raw, 0x12345678u, 0x89abcdefu, 0x0badf00du,
                                     0xc0001000u};
    const u32 consumed = ApplyType0RegisterWrite(registers, packet);
    if (consumed != 4 || registers[482] != 0xccccccccu ||
        registers[483] != packet[1] || registers[484] != packet[2] ||
        registers[485] != packet[3] || registers[486] != 0xccccccccu) {
        throw std::runtime_error("PM4 type-0 register write did not preserve its exact range");
    }
    if (packet[consumed] != 0xc0001000u) {
        throw std::runtime_error("PM4 type-0 packet consumed the following packet header");
    }

    PM4Type0Header single{};
    single.base.Assign(19);
    single.count.Assign(0);
    const std::array<u32, 2> single_packet{single.raw, 0xfeedfaceu};
    if (ApplyType0RegisterWrite(registers, single_packet) != 2 ||
        registers[19] != 0xfeedfaceu || registers[18] != 0xccccccccu ||
        registers[20] != 0xccccccccu) {
        throw std::runtime_error("Single-word PM4 type-0 register write failed");
    }

    std::puts("PASS: PM4 type-0 writes exact consecutive registers and preserves packet framing");
    return 0;
}
