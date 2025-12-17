// ib_scorep_dump_ports.cpp
//
// Iterates over Score-P topology PCI nodes, maps them to /sys/class/infiniband devices+ports,
// then prints a single snapshot of counters per port (no background thread).
//
// Build notes:
//   - Compile this together with the same includes/libs you use for the plugin (Score-P headers).
//   - Requires C++17 for <filesystem>.
//
// Output is intentionally "best effort": it prints what is readable on the current system.

#include <scorep/plugin/plugin.hpp>

#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "infiniband_counter.hpp"
#include "infiniband_sysfs.hpp"

static std::string pci_id_string(const SCOREP_MetricTopologyNodeIdentifier& id)
{
    char buf[64];
    // domain:xxxx bus:xx device:xx func:x
    std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%1x",
                  (unsigned)id.pci.domain,
                  (unsigned)id.pci.bus,
                  (unsigned)id.pci.device,
                  (unsigned)id.pci.function);
    return std::string(buf);
}

static std::map<uint64_t, std::vector<std::pair<std::string, uint32_t>>> build_pci_to_ib_map()
{
    std::map<uint64_t, std::vector<std::pair<std::string, uint32_t>>> out;

    for (const auto& dev : ibsys::list_infiniband_devices())
    {
        const std::string bdf = ibsys::device_pci_bdf(dev);
        if (bdf.empty()) continue;

        uint32_t domain = 0;
        uint8_t bus = 0, device = 0, function = 0;
        if (!ibsys::parse_bdf(bdf, domain, bus, device, function)) continue;

        SCOREP_MetricTopologyNodeIdentifier id;
        id.pci.domain = domain;
        id.pci.bus = bus;
        id.pci.device = device;
        id.pci.function = function;

        for (auto p : ibsys::list_ports(dev))
            out[id.id].emplace_back(dev, p);
    }

    return out;
}

int main()
{
    // Build the sysfs-based mapping first.
    const auto pci_to_ib = build_pci_to_ib_map();

    // const SCOREP_MetricTopologyNode* root = SCOREP_GetMetricTopologyTree();
    // if (!root)
    // {
    //     std::cerr << "Score-P topology root not available.\n";
    //     return 1;
    // }

    // Gather all responsible PCI nodes (domain name depends on Score-P build).
    std::vector<const SCOREP_MetricTopologyNode*> pci_nodes;
    
    SCOREP_MetricTopology_ForAllResponsiblePerDomain(
        SCOREP_METRIC_TOPOLOGY_NODE_DOMAIN_MACHINE,
        SCOREP_METRIC_TOPOLOGY_NODE_DOMAIN_NETWORKING_DEVICE,
        []( const SCOREP_MetricTopologyNode* node,
            void* cbArg)
        {
            //static_cast<std::vector<const SCOREP_MetricTopologyNode*>*>(cbArg)->emplace_back(node);
            auto* nodes = static_cast<std::vector<const SCOREP_MetricTopologyNode*>*>(cbArg);
            nodes->emplace_back(node);
        }, (void*)&pci_nodes);

    if (pci_nodes.empty())
    {
        std::cout << "No Score-P PCI/network topology nodes found.\n";
        return 0;
    }

    const auto known = InfinibandCounter::get_known_counters();

    for (const auto* node : pci_nodes)
    {
        SCOREP_MetricTopologyNodeIdentifier nid;
        nid.id = node->id;

        auto it = pci_to_ib.find(node->id);
        if (it == pci_to_ib.end()) continue;

        std::cout << "Topology node PCI: " << pci_id_string(nid) << "  (node id=" << node->id << ")\n";

        for (const auto& dp : it->second)
        {
            const auto& dev = dp.first;
            const auto port = dp.second;

            std::cout << "  IB dev: " << dev << "  port: " << port << "\n";

            // Print known counters snapshot (supported only).
            for (const auto& [type, props] : known)
            {
                if (type == InfinibandCounter::Type::INVALID) continue;

                InfinibandCounter c(dev, port, type);
                if (!c.supported()) continue;

                double v = 0.0;
                try { v = c.read(); }
                catch (...) { continue; }

                std::cout << "    " << props.name << " = " << v;
                if (!props.unit.empty()) std::cout << " " << props.unit;
                std::cout << "\n";
            }

            // Print device-specific hw_counters snapshot (direct enumeration).
            const auto hw_files = ibsys::list_hw_counters(dev, port);
            if (!hw_files.empty())
            {
                std::cout << "    hw_counters:\n";
                for (const auto& f : hw_files)
                {
                    auto hc = InfinibandCounter::hw_custom(dev, port, f);
                    double v = 0.0;
                    try { v = hc.read(); }
                    catch (...) { continue; }
                    std::cout << "      " << "hw_" << f << " = " << v << " count\n";
                }
            }
        }

        std::cout << "\n";
    }

    return 0;
}
