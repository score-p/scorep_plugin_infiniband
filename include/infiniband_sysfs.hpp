#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ibsys
{
namespace fs = std::filesystem;

inline std::string
canonical_str(const fs::path& p)
{
    std::error_code ec;
    auto c = fs::canonical(p, ec);
    if (ec) return {};
    return c.string();
}

inline std::vector<std::string>
list_infiniband_devices( void )
{
    std::vector<std::string> out;
    std::error_code ec;
    fs::path root = "/sys/class/infiniband";
    if (!fs::exists(root, ec) || ec) return out;

    for (auto const& it : fs::directory_iterator(root, ec))
    {
        if (ec) break;
        if (!it.is_directory(ec) || ec) continue;
        out.push_back(it.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline std::vector<uint32_t>
list_ports(const std::string& ib_dev)
{
    std::vector<uint32_t> ports;
    std::error_code ec;
    fs::path ports_dir = fs::path("/sys/class/infiniband") / ib_dev / "ports";
    if (!fs::exists(ports_dir, ec) || ec) return ports;

    for (auto const& it : fs::directory_iterator(ports_dir, ec))
    {
        if (ec) break;
        if (!it.is_directory(ec) || ec) continue;

        const std::string name = it.path().filename().string();
        try {
            unsigned long v = std::stoul(name);
            ports.push_back(static_cast<uint32_t>(v));
        } catch (...) {
        }
    }
    std::sort(ports.begin(), ports.end());
    return ports;
}

inline std::string
device_pci_bdf(const std::string& ib_dev)
{
    // /sys/class/infiniband/<dev>/device -> .../pci.../<BDF>
    const fs::path link = fs::path("/sys/class/infiniband") / ib_dev / "device";
    const std::string canon = canonical_str(link);
    if (canon.empty()) return {};
    return fs::path(canon).filename().string(); // "0000:bb:dd.f"
}

inline std::vector<std::string>
list_counters_in_directory( const std::string& ib_dev,
                            uint32_t port,
                            const std::string& directory )
{
    std::vector<std::string> out;
    std::error_code ec;
    fs::path hw_dir = fs::path("/sys/class/infiniband") / ib_dev / "ports" / std::to_string(port) / directory;
    if (!fs::exists(hw_dir, ec) || ec) return out;

    for (auto const& it : fs::directory_iterator(hw_dir, ec))
    {
        if (ec) break;
        if (!it.is_regular_file(ec) || ec) continue;
        out.push_back(it.path().filename().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline std::vector<std::string>
list_counters( const std::string& ib_dev,
                  uint32_t port )
{
    return list_counters_in_directory(ib_dev, port, "counters");
}

inline std::vector<std::string>
list_hw_counters( const std::string& ib_dev,
                  uint32_t port )
{
    return list_counters_in_directory(ib_dev, port, "hw_counters");;
}

// Parse "0000:3b:00.0" into {domain,bus,device,function}. Returns false on failure.
inline bool
parse_bdf( const std::string& bdf,
           uint32_t&          domain,
           uint8_t&           bus,
           uint8_t&           dev,
           uint8_t&           func )
{
    // Minimal, strict parser.
    // domain: 4 hex, bus: 2 hex, dev: 2 hex, func: 1 hex
    if (bdf.size() < 12) return false; // "0000:00:00.0"
    auto hex = [](char c)->int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    auto parse_n = [&](size_t pos, size_t n, uint32_t& out)->bool {
        uint32_t v = 0;
        for (size_t i = 0; i < n; ++i)
        {
            int h = hex(bdf[pos + i]);
            if (h < 0) return false;
            v = (v << 4) | static_cast<uint32_t>(h);
        }
        out = v;
        return true;
    };

    uint32_t dom=0, busv=0, devv=0, funcv=0;
    if (!parse_n(0, 4, dom))    return false;
    if (bdf[4] != ':')          return false;
    if (!parse_n(5, 2, busv))   return false;
    if (bdf[7] != ':')          return false;
    if (!parse_n(8, 2, devv))   return false;
    if (bdf[10] != '.')         return false;
    if (!parse_n(11, 1, funcv)) return false;

    domain = dom;
    bus    = static_cast<uint8_t>(busv);
    dev    = static_cast<uint8_t>(devv);
    func   = static_cast<uint8_t>(funcv);
    return true;
}
} // namespace ibsys
