// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Common {

// This is diagnostic data, not a scheduling policy. In particular, an OS efficiency class does
// not uniquely identify a physical P, E, or low-power E core on every hybrid processor.
struct CpuSetTopology {
    std::uint32_t id{};
    std::uint16_t group{};
    std::uint8_t logical_index{};
    std::uint8_t core_index{};
    std::uint8_t last_level_cache_index{};
    std::uint8_t numa_node_index{};
    std::uint8_t efficiency_class{};
    bool parked{};
    bool allocated{};
    bool allocated_to_process{};
};

struct CpuTopology {
    unsigned hardware_concurrency{};
    bool cpu_sets_available{};
    std::uint32_t cpu_sets_error{};
    bool affinity_available{};
    std::uint32_t affinity_error{};
    std::uint64_t process_affinity_mask{};
    std::uint64_t system_affinity_mask{};
    bool process_groups_available{};
    std::vector<std::uint16_t> process_groups;
    std::vector<CpuSetTopology> cpu_sets;
    std::string parse_error;
};

enum class CpuAffinityAccess { Unknown, Allowed, Excluded };

// GetProcessAffinityMask describes one group only. Never apply that mask to another group or
// claim an all-groups count when a process spans more than one group.
inline CpuAffinityAccess CpuSetAffinityAccess(const CpuTopology& topology,
                                              const CpuSetTopology& cpu) {
    if (!topology.affinity_available || !topology.process_groups_available ||
        topology.process_groups.size() != 1 || cpu.group != topology.process_groups.front() ||
        cpu.logical_index >= 64) {
        return CpuAffinityAccess::Unknown;
    }
    return (topology.process_affinity_mask & (std::uint64_t{1} << cpu.logical_index)) != 0
               ? CpuAffinityAccess::Allowed
               : CpuAffinityAccess::Excluded;
}

#ifdef _WIN32
namespace CpuTopologyDetail {

// Walk the documented variable-sized records. Copy only the bytes present in each record so
// an older/smaller record and an unknown future record type cannot cause an out-of-bounds read.
inline bool ParseCpuSets(std::span<const std::byte> bytes, std::vector<CpuSetTopology>& result,
                         std::string& error) {
    std::vector<CpuSetTopology> parsed;
    std::size_t offset = 0;
    constexpr std::size_t header_size = offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet);
    using WindowsCpuSet = decltype(SYSTEM_CPU_SET_INFORMATION{}.CpuSet);
    constexpr std::size_t required_size =
        header_size + offsetof(WindowsCpuSet, AllFlags) + sizeof(BYTE);
    while (offset < bytes.size()) {
        if (bytes.size() - offset < header_size) {
            error = "truncated CPU set record header";
            return false;
        }
        SYSTEM_CPU_SET_INFORMATION record{};
        std::memcpy(&record, bytes.data() + offset, header_size);
        if (record.Size < header_size || record.Size > bytes.size() - offset) {
            error = "invalid CPU set record size";
            return false;
        }
        if (record.Type == CpuSetInformation) {
            if (record.Size < required_size) {
                error = "truncated CPU set record fields";
                return false;
            }
            std::memcpy(&record, bytes.data() + offset,
                        std::min<std::size_t>(record.Size, sizeof(record)));
            const auto& cpu = record.CpuSet;
            parsed.push_back({cpu.Id, cpu.Group, cpu.LogicalProcessorIndex, cpu.CoreIndex,
                              cpu.LastLevelCacheIndex, cpu.NumaNodeIndex, cpu.EfficiencyClass,
                              cpu.Parked != 0, cpu.Allocated != 0,
                              cpu.AllocatedToTargetProcess != 0});
        }
        offset += record.Size;
    }
    result = std::move(parsed);
    error.clear();
    return true;
}

} // namespace CpuTopologyDetail
#endif

// All Windows calls in this function are queries. Leave Windows/Thread Director free to place
// guest and emulator workers; no affinity, CPU set, ideal processor, or priority is written.
inline CpuTopology CaptureCpuTopology() {
    CpuTopology topology;
    topology.hardware_concurrency = std::thread::hardware_concurrency();
#ifdef _WIN32
    const auto process = GetCurrentProcess();
    DWORD_PTR process_mask{}, system_mask{};
    topology.affinity_available = GetProcessAffinityMask(process, &process_mask, &system_mask) != 0;
    if (topology.affinity_available) {
        topology.process_affinity_mask = static_cast<std::uint64_t>(process_mask);
        topology.system_affinity_mask = static_cast<std::uint64_t>(system_mask);
    } else {
        topology.affinity_error = GetLastError();
    }

    // One entry is enough on the handheld; retry with the required size for a multi-group host.
    USHORT group_count = 1;
    std::vector<USHORT> groups(group_count);
    if (GetProcessGroupAffinity(process, &group_count, groups.data())) {
        topology.process_groups_available = true;
    } else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && group_count != 0) {
        groups.resize(group_count);
        topology.process_groups_available =
            GetProcessGroupAffinity(process, &group_count, groups.data()) != 0;
    }
    if (topology.process_groups_available) {
        groups.resize(group_count);
        topology.process_groups.assign(groups.begin(), groups.end());
    }

    ULONG required{};
    if (!GetSystemCpuSetInformation(nullptr, 0, &required, process, 0)) {
        topology.cpu_sets_error = GetLastError();
        if (topology.cpu_sets_error != ERROR_INSUFFICIENT_BUFFER) {
            return topology;
        }
    }
    // Topology can change between size and data queries. Bound retries without trusting a
    // partially filled buffer. The successful call's returned byte count is authoritative.
    for (unsigned attempt = 0; attempt < 3 && required != 0; ++attempt) {
        std::vector<std::byte> bytes(required);
        ULONG returned{};
        if (GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(bytes.data()),
                                       static_cast<ULONG>(bytes.size()), &returned, process, 0)) {
            if (returned > bytes.size()) {
                topology.parse_error = "CPU set query returned an oversized buffer length";
                return topology;
            }
            topology.cpu_sets_available =
                CpuTopologyDetail::ParseCpuSets(std::span<const std::byte>(bytes.data(), returned),
                                                topology.cpu_sets, topology.parse_error);
            topology.cpu_sets_error = 0;
            return topology;
        }
        topology.cpu_sets_error = GetLastError();
        if (topology.cpu_sets_error != ERROR_INSUFFICIENT_BUFFER || returned <= bytes.size()) {
            return topology;
        }
        required = returned;
    }
#endif
    return topology;
}

inline std::vector<std::string> DescribeCpuTopology(const CpuTopology& topology) {
    std::vector<std::string> lines;
    std::ostringstream summary;
    summary << "[CPU-TOPOLOGY] read-only; scheduler=OS default; hardware_concurrency="
            << topology.hardware_concurrency << "; cpu_sets=" << topology.cpu_sets.size()
            << "; query=" << (topology.cpu_sets_available ? "ok" : "unavailable")
            << "; error=" << topology.cpu_sets_error;
    if (!topology.parse_error.empty()) {
        summary << "; parse_error=" << topology.parse_error;
    }
    lines.push_back(summary.str());

    std::ostringstream affinity;
    affinity << "[CPU-TOPOLOGY] process_affinity="
             << (topology.affinity_available ? "ok" : "unavailable") << "; mask=0x" << std::hex
             << topology.process_affinity_mask << "; system_mask=0x"
             << topology.system_affinity_mask << std::dec << "; error=" << topology.affinity_error
             << "; process_groups=";
    if (!topology.process_groups_available) {
        affinity << "unknown";
    } else {
        for (std::size_t i = 0; i < topology.process_groups.size(); ++i) {
            affinity << (i ? "," : "") << topology.process_groups[i];
        }
    }
    affinity << "; mask_scope="
             << (topology.process_groups_available && topology.process_groups.size() == 1
                     ? "single listed group"
                     : "group scope not determined; no all-groups eligibility inferred");
    lines.push_back(affinity.str());

    struct ClassCounts {
        unsigned logical{}, affinity_allowed{}, affinity_excluded{}, affinity_unknown{};
        unsigned parked{}, reserved_for_other{};
        std::set<std::pair<std::uint16_t, std::uint8_t>> cores;
    };
    std::map<unsigned, ClassCounts> classes;
    for (const auto& cpu : topology.cpu_sets) {
        auto& counts = classes[cpu.efficiency_class];
        ++counts.logical;
        counts.cores.emplace(cpu.group, cpu.core_index);
        counts.parked += cpu.parked;
        counts.reserved_for_other += cpu.allocated && !cpu.allocated_to_process;
        const auto access = CpuSetAffinityAccess(topology, cpu);
        counts.affinity_allowed += access == CpuAffinityAccess::Allowed;
        counts.affinity_excluded += access == CpuAffinityAccess::Excluded;
        counts.affinity_unknown += access == CpuAffinityAccess::Unknown;
        std::ostringstream record;
        record << "[CPU-TOPOLOGY] id=" << cpu.id << " group=" << cpu.group
               << " logical=" << unsigned(cpu.logical_index) << " core=" << unsigned(cpu.core_index)
               << " llc=" << unsigned(cpu.last_level_cache_index)
               << " numa=" << unsigned(cpu.numa_node_index)
               << " efficiency_class=" << unsigned(cpu.efficiency_class) << " affinity="
               << (access == CpuAffinityAccess::Allowed    ? "allowed"
                   : access == CpuAffinityAccess::Excluded ? "excluded"
                                                           : "unknown")
               << " parked=" << cpu.parked << " allocated=" << cpu.allocated
               << " allocated_to_process=" << cpu.allocated_to_process;
        lines.push_back(record.str());
    }
    for (const auto& [value, counts] : classes) {
        std::ostringstream line;
        line << "[CPU-TOPOLOGY] efficiency_class=" << value << " logical=" << counts.logical
             << " cores=" << counts.cores.size() << " affinity_allowed=" << counts.affinity_allowed
             << " affinity_excluded=" << counts.affinity_excluded
             << " affinity_unknown=" << counts.affinity_unknown << " parked=" << counts.parked
             << " reserved_for_other=" << counts.reserved_for_other;
        lines.push_back(line.str());
    }
    lines.emplace_back("[CPU-TOPOLOGY] Higher EfficiencyClass means higher OS performance class; "
                       "physical P/E/LP core types are not inferred. Parked is a transient state, "
                       "not an affinity exclusion. No scheduling settings changed.");
    return lines;
}

// CPU-only CLI check. This neither creates a graphics device nor starts an emulator/guest.
inline int RunCpuTopologySelfTest() {
    unsigned failures{};
    const auto require = [&failures](bool valid, const char* name) {
        if (!valid) {
            ++failures;
            std::cerr << "[CPU-TOPOLOGY-TEST] FAIL " << name << '\n';
        }
    };
    CpuTopology synthetic;
    synthetic.affinity_available = synthetic.process_groups_available = true;
    synthetic.process_groups = {0};
    synthetic.process_affinity_mask = 5;
    CpuSetTopology cpu;
    require(CpuSetAffinityAccess(synthetic, cpu) == CpuAffinityAccess::Allowed, "allowed bit");
    cpu.logical_index = 1;
    require(CpuSetAffinityAccess(synthetic, cpu) == CpuAffinityAccess::Excluded, "excluded bit");
    cpu.group = 1;
    require(CpuSetAffinityAccess(synthetic, cpu) == CpuAffinityAccess::Unknown, "another group");
    synthetic.process_groups = {0, 1};
    cpu.group = 0;
    require(CpuSetAffinityAccess(synthetic, cpu) == CpuAffinityAccess::Unknown, "multiple groups");
    synthetic.process_groups = {0};
    cpu.logical_index = 64;
    require(CpuSetAffinityAccess(synthetic, cpu) == CpuAffinityAccess::Unknown, "mask shift bound");
#ifdef _WIN32
    std::vector<CpuSetTopology> parsed;
    std::string error;
    SYSTEM_CPU_SET_INFORMATION record{};
    record.Size = sizeof(record);
    record.Type = CpuSetInformation;
    record.CpuSet.Id = 42;
    record.CpuSet.Group = 1;
    record.CpuSet.LogicalProcessorIndex = 2;
    record.CpuSet.CoreIndex = 3;
    record.CpuSet.LastLevelCacheIndex = 4;
    record.CpuSet.NumaNodeIndex = 5;
    record.CpuSet.EfficiencyClass = 6;
    record.CpuSet.Parked = 1;
    record.CpuSet.Allocated = 1;
    const auto bytes = std::as_bytes(std::span{&record, 1});
    require(CpuTopologyDetail::ParseCpuSets(bytes, parsed, error) && parsed.size() == 1 &&
                parsed[0].id == 42 && parsed[0].group == 1 && parsed[0].logical_index == 2 &&
                parsed[0].core_index == 3 && parsed[0].last_level_cache_index == 4 &&
                parsed[0].numa_node_index == 5 && parsed[0].efficiency_class == 6 &&
                parsed[0].parked && parsed[0].allocated && !parsed[0].allocated_to_process,
            "CPU set record fields");
    require(!CpuTopologyDetail::ParseCpuSets(bytes.first(bytes.size() - 1), parsed, error),
            "truncated record rejected");
    record.Size = 0;
    require(!CpuTopologyDetail::ParseCpuSets(bytes, parsed, error), "zero-sized record rejected");
    record.Size = sizeof(record);
    record.Type = static_cast<CPU_SET_INFORMATION_TYPE>(99);
    require(CpuTopologyDetail::ParseCpuSets(bytes, parsed, error) && parsed.empty(),
            "unknown record type skipped");
#endif
    const auto actual = CaptureCpuTopology();
    for (const auto& line : DescribeCpuTopology(actual)) {
        std::cout << line << '\n';
    }
#ifdef _WIN32
    require(actual.cpu_sets_available && !actual.cpu_sets.empty(), "actual OS CPU set query");
    require(actual.affinity_available, "actual process affinity query");
    std::set<std::uint32_t> ids;
    for (const auto& actual_cpu : actual.cpu_sets) {
        require(ids.insert(actual_cpu.id).second, "unique actual CPU set ID");
    }
#endif
    std::cout << "[CPU-TOPOLOGY-TEST] failures=" << failures
              << "; only synthetic parsing and read-only OS queries; no affinity writes\n";
    return failures == 0 ? 0 : 1;
}

} // namespace Common
