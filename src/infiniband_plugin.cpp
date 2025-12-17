// scorep_infiniband_plugin.cpp
#include <scorep/plugin/plugin.hpp>
#include <scorep/plugin/util/matcher.hpp>

#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <map>
#include <string>
#include <utility>

#include <iomanip>  // setw, setfill
#include <ios>      // ios_base

#include "infiniband_counter.hpp"
#include "infiniband_sysfs.hpp"

using scorep::plugin::logging;
using TVPair = std::pair<scorep::chrono::ticks, double>;

class InfinibandMeasurementThread
{
    struct CounterReadings
    {
        std::vector<TVPair> readings;
        double offset{0.0};
    };

public:
    explicit InfinibandMeasurementThread(std::chrono::milliseconds interval)
    : interval_(interval)
    {
    }

    void add_counter(const InfinibandCounter& c)
    {
        data_.emplace(c, CounterReadings{});
        data_[c].offset = c.read(); // monotonic: subtract baseline
    }

    void measurement()
    {
        using clock = std::chrono::steady_clock;
        
        size_t cnt = 0;
        
        auto interval = interval_;                  // duration
        auto time_begin = clock::now();           // time_point

        std::chrono::milliseconds time_in_measurement{0};
        
        while (!stop_)
        {
            auto cycle_start = clock::now();
            auto until = cycle_start + interval;   // time_point

            // --- active work ---
            {
                std::lock_guard<std::mutex> lock(read_mutex_);
                for (auto& kv : data_)
                {
                    const auto& counter = kv.first;
                    auto& state = kv.second;

                    double value = counter.read() - state.offset;
                    state.readings.emplace_back(
                        scorep::chrono::measurement_clock::now(), value
                    );
                }
            }
            auto cycle_end_actual = clock::now();
            time_in_measurement += std::chrono::duration_cast<std::chrono::milliseconds>(
                cycle_end_actual - cycle_start
            );
            cnt++;

            // --- drift correction - use next multiple of interval to collect data ---
            // Not actually needed, but lets keep it here for now.
            while (until <= cycle_end_actual)
            {
                until += interval;
            }

            std::this_thread::sleep_until(until);
        }

        auto time_end = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - time_begin
        );
        
        auto interval_ms = interval_.count();
        auto interval_measurement_avg = cnt > 0 ? time_in_measurement.count() / cnt : 0;

        std::ostringstream oss;
        oss << "Timing Info:: "
            << "Selected interval: " << interval_ms << "ms, "
            << "Actual interval (Average measurement duration): " << interval_measurement_avg << "ms";

        const auto text = oss.str();

        if (cnt == 0)
        {
            logging::warn() << "Timing Info:: No measurements collected";
        }
        else if (interval_measurement_avg > interval_ms)
        {
            logging::warn() << text;
        }
        else
        {
            logging::info() << text;
        }
    }


    std::vector<TVPair> get_values_for_counter(const InfinibandCounter& c)
    {
        std::lock_guard<std::mutex> lock(read_mutex_);
        std::vector<TVPair> ret;
        std::swap(ret, data_.at(c).readings);
        return ret;
    }

    void start()
    {
        thread_ = std::thread([this]() { this->measurement(); });
    }

    void stop()
    {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
    }

private:
    bool stop_ = false;
    std::mutex read_mutex_;
    std::chrono::milliseconds interval_;
    std::map<InfinibandCounter, CounterReadings> data_;
    std::thread thread_;
};

using namespace scorep::plugin::policy;

class infiniband_plugin
: public scorep::plugin::base< infiniband_plugin,
                               config_vars,
                               async,
                               per_topology,
                               scorep_clock >
{
public:
    infiniband_plugin(std::map<std::string, std::string> configVars)
    : measurement_interval_(
        std::chrono::milliseconds(stoi(configVars.at("interval")))),
      measurement_thread_(
        measurement_interval_)
    {
        build_pci_to_ib_map_();
        if (include_hw_)
        {
            build_hw_metric_name_set_();
        }

        logging::debug() << "Infiniband plugin init: "
                         << "interval="   << configVars.at("interval") << "ms ";
    }

    ~infiniband_plugin() = default;

    static std::map<std::string, std::string> declare_config_vars()
    {
        return {
            { "interval", "50" },
        };
    }

    std::vector<scorep::plugin::metric_property>
    get_metric_properties(const std::string& metricPattern)
    {
        std::vector<scorep::plugin::metric_property> result;

        // Known (portable-ish) counters.
        for (const auto& [type, props] : InfinibandCounter::get_known_counters())
        {
            if (type == InfinibandCounter::Type::INVALID) continue;
            if (!scorep::plugin::util::matcher(metricPattern)(props.name)) continue;

            auto property = scorep::plugin::metric_property(
                                props.name, props.description, props.unit)
                                .value_double()
                                .accumulated_point(); // monotonic with baseline subtraction
            result.emplace_back(property);

            logging::debug() << "Declared IB metric " << props.name;
        }

        // Device-specific hw counters (union across detected devices/ports).
        if (include_hw_)
        {
            for (const auto& n : hw_metric_names_)
            {
                if (!scorep::plugin::util::matcher(metricPattern)(n)) continue;

                auto property = scorep::plugin::metric_property(
                                    n,
                                    "Infiniband hw_counters/<file> (device/driver-specific)",
                                    "count")
                                    .value_double()
                                    .accumulated_point();
                result.emplace_back(property);

                logging::debug() << "Declared IB hw metric " << n;
            }
        }

        return result;
    }

    std::vector<scorep::plugin::measurement_point>
    add_topology_metrics( const SCOREP_MetricTopologyNode* topologyRoot,
                          const std::string&               metricName )
    {
        std::string hw_file;
        InfinibandCounter::Type type = InfinibandCounter::Type::INVALID;

        for (const auto& [t, props] : InfinibandCounter::get_known_counters())
        {
            if (metricName == props.name)
            {
                type = t;
                break;
            }
        }

        if (type == InfinibandCounter::Type::INVALID)
        {
            logging::warn() << "Invalid/disabled metric: " << metricName << ", ignored";
            throw std::exception();
        }

        // Collect all responsible PCI-device nodes (fallback if different Score-P build defines names).
        std::vector<const SCOREP_MetricTopologyNode*> nodes;

        SCOREP_MetricTopology_ForAllResponsiblePerDomain(
            topologyRoot, SCOREP_METRIC_TOPOLOGY_NODE_DOMAIN_NETWORKING_DEVICE,
            []( const SCOREP_MetricTopologyNode* node,
                void* cbArg)
            {
                //static_cast<std::vector<const SCOREP_MetricTopologyNode*>*>(cbArg)->emplace_back(node);
                auto* nodes = static_cast<std::vector<const SCOREP_MetricTopologyNode*>*>(cbArg);
                nodes->emplace_back(node);
            }, (void*)&nodes);

        std::vector<scorep::plugin::measurement_point> results;

        for (const auto* node : nodes)
        {
            auto it = ib_by_pci_.find(node->id);
            if (it == ib_by_pci_.end()) continue;

            for (const auto& dev_port : it->second)
            {
                const auto& dev = dev_port.first;
                const auto port = dev_port.second;

                InfinibandCounter counter = InfinibandCounter(dev, port, type);

                if (!counter.supported())
                {
                    logging::warn() << "Metric " << metricName << " not supported on " << dev << " port " << port;
                    continue;
                }

                const auto metric_id = static_cast<int32_t>(counters_.size());
                counters_.emplace_back(counter);
                results.emplace_back(metric_id, node);

                logging::debug() << "Added IB metric " << metricName
                                 << " for " << dev << " port " << port
                                 << " on topology node id=" << node->id;
            }
        }

        return results;
    }

    template <typename C>
    void get_all_values(int32_t& id, C& cursor)
    {
        auto values = measurement_thread_.get_values_for_counter(counters_.at(id));
        for (auto& v : values)
            cursor.write(v.first, v.second);
    }

    void start()
    {
        for (const auto& c : counters_)
        {
            measurement_thread_.add_counter(c);
        }
        measurement_thread_.start();
    }

    void stop()
    {
        measurement_thread_.stop();
    }

    void synchronize(bool, SCOREP_MetricSynchronizationMode)
    {
    }

private:
    void build_pci_to_ib_map_()
    {
        // Map Score-P PCI node id -> list of (ib_dev, port)
        for (const auto& dev : ibsys::list_infiniband_devices())
        {
            const std::string bdf = ibsys::device_pci_bdf(dev);
            if (bdf.empty())
            {
                continue;
            }

            uint32_t domain = 0;
            uint8_t bus = 0, device = 0, function = 0;
            if (!ibsys::parse_bdf(bdf, domain, bus, device, function))
            {
                continue;
            }

            SCOREP_MetricTopologyNodeIdentifier id;
            id.pci.domain   = domain;
            id.pci.bus      = bus;
            id.pci.device   = device;
            id.pci.function = function;

            const auto ports = ibsys::list_ports(dev);
            for (auto p : ports)
            {
                ib_by_pci_[id.id].emplace_back(dev, p);
                logging::debug()
                    << "Found IB device " << dev << " port " << p
                    << " on PCI "
                    << std::hex << std::setfill('0')
                    << std::setw(4) << static_cast<unsigned>(domain) << ":"
                    << std::setw(2) << static_cast<unsigned>(bus) << ":"
                    << std::setw(2) << static_cast<unsigned>(device) << "."
                    << std::setw(1) << static_cast<unsigned>(function)
                    << std::dec; // restore
            }
        }
    }

    void build_hw_metric_name_set_()
    {
        // Union across all detected devices/ports.
        std::set<std::string> names;

        for (const auto& dev : ibsys::list_infiniband_devices())
        {
            for (auto p : ibsys::list_ports(dev))
            {
                for (const auto& f : ibsys::list_hw_counters(dev, p))
                    names.insert("hw_" + f);
            }
        }

        hw_metric_names_.assign(names.begin(), names.end());
    }

private:
    std::chrono::milliseconds measurement_interval_;
    bool include_hw_{false};

    InfinibandMeasurementThread measurement_thread_;
    std::vector<InfinibandCounter> counters_;

    // PCI node id -> [(ib_dev, port), ...]
    std::map<uint64_t, std::vector<std::pair<std::string, uint32_t>>> ib_by_pci_;

    // Optional: "hw_<file>" metric names
    std::vector<std::string> hw_metric_names_;
};

SCOREP_METRIC_PLUGIN_CLASS(infiniband_plugin, "infiniband");
