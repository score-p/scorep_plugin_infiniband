#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#else
  #error "This header requires <filesystem> (C++17)."
#endif

class InfinibandCounter
{
public:
    enum class Type
    {
        INVALID,
        /* counters */
        PORT_XMIT_CONSTRAINT_ERRORS,
        PORT_XMIT_DATA,
        PORT_XMIT_DISCARDS,
        PORT_XMIT_PACKETS,
        PORT_XMIT_WAIT,
        PORT_RCV_CONSTRAINT_ERRORS,
        PORT_RCV_DATA,
        PORT_RCV_ERRORS,
        PORT_RCV_PACKETS,
        PORT_RCV_REMOTE_PHYSICAL_ERRORS,
        PORT_RCV_SWITCH_RELAY_ERRORS,
        MULTICAST_XMIT_PACKETS,
        MULTICAST_RCV_PACKETS,
        UNICAST_XMIT_PACKETS,
        UNICAST_RCV_PACKETS,
        SYMBOL_ERROR,
        LINK_ERROR_RECOVERY,
        LINK_DOWNED,
        LOCAL_LINK_INTEGRITY_ERRORS,
        EXCESSIVE_BUFFER_OVERRUN_ERRORS,
        VL15_DROPPED,
        TX_BYTES,
        RX_BYTES,
        TX_PACKETS,
        RX_PACKETS,

        /* hw_counters */
        DUPLICATE_REQUEST,
        IMPLIED_NAK_SEQ_ERR,
        LIFESPAN,
        LOCAL_ACK_TIMEOUT_ERR,
        OUT_OF_BUFFER,
        OUT_OF_SEQUENCE,
        PACKET_SEQ_ERR,
        REQ_CQE_ERROR,
        REQ_CQE_FLUSH_ERROR,
        REQ_REMOTE_ACCESS_ERRORS,
        REQ_REMOTE_INVALID_REQUEST,
        REQ_RNR_RETRIES_EXCEEDED,
        REQ_TRANSPORT_RETRIES_EXCEEDED,
        RESP_CQE_ERROR,
        RESP_CQE_FLUSH_ERROR,
        RESP_LOCAL_LENGTH_ERROR,
        RESP_REMOTE_ACCESS_ERRORS,
        RNR_NAK_RETRY_ERR,
        ROCE_ADP_RETRANS,
        ROCE_ADP_RETRANS_TO,
        ROCE_SLOW_RESTART,
        ROCE_SLOW_RESTART_CNPS,
        ROCE_SLOW_RESTART_TRANS,
        RX_ATOMIC_REQUESTS,
        RX_DCT_CONNECT,
        RX_ICRC_ENCAPSULATED,
        RX_READ_REQUESTS,
        RX_WRITE_REQUESTS,
    };

    enum class Group{
        Informative,
        Error
    };

    struct Properties
    {
        std::string name;
        std::string description;
        std::string unit;
        Group       group;
        bool        accumulated{false};
    };

public:
    InfinibandCounter(std::string device_name, uint32_t port, Type type)
    : device_name_(std::move(device_name)), port_(port), type_(type)
    {
    }

    static std::map<Type, Properties> get_known_counters()
    {
        return {
            /* counters */
            { Type::PORT_XMIT_CONSTRAINT_ERRORS,     { "port_xmit_constraint_errors",    "Transmit Constraint Errors",            "#",   Group::Error,       true } },
            { Type::PORT_XMIT_DATA,                  { "port_xmit_data",                 "Port Transmit Data (IB: 32-bit words)", "B",   Group::Informative, true } },
            { Type::PORT_XMIT_DISCARDS,              { "port_xmit_discards",             "Transmit Discards Counter",             "#",   Group::Error,       true } },
            { Type::PORT_XMIT_PACKETS,               { "port_xmit_packets",              "Port Transmit Packets",                 "pkt", Group::Informative, true } },
            { Type::PORT_XMIT_WAIT,                  { "port_xmit_wait",                 "Port Transmit Wait / Stall Counter",    "#",   Group::Informative, true } },
            { Type::PORT_RCV_CONSTRAINT_ERRORS,      { "port_rcv_constraint_errors",     "Receive Constraint Errors",             "#",   Group::Error,       true } },
            { Type::PORT_RCV_DATA,                   { "port_rcv_data",                  "Port Receive Data (IB: 32-bit words)",  "B",   Group::Informative, true } },
            { Type::PORT_RCV_ERRORS,                 { "port_rcv_errors",                "Receive Errors Counter",                "#",   Group::Informative, true } },
            { Type::PORT_RCV_PACKETS,                { "port_rcv_packets",               "Port Receive Packets",                  "pkt", Group::Informative, true } },
            { Type::PORT_RCV_REMOTE_PHYSICAL_ERRORS, { "port_rcv_remote_physical_errors","Receive Remote Physical Errors",        "#",   Group::Error,       true } },
            { Type::PORT_RCV_SWITCH_RELAY_ERRORS,    { "port_rcv_switch_relay_errors",   "Receive Switch Relay Errors",           "#",   Group::Error,       true } },
            { Type::MULTICAST_XMIT_PACKETS,          { "multicast_xmit_packets",         "Multicast Transmit Packets",            "pkt", Group::Informative, true } },
            { Type::MULTICAST_RCV_PACKETS,           { "multicast_rcv_packets",          "Multicast Receive Packets",             "pkt", Group::Informative, true } },
            { Type::UNICAST_XMIT_PACKETS,            { "unicast_xmit_packets",           "Unicast Transmit Packets",              "pkt", Group::Informative, true } },
            { Type::UNICAST_RCV_PACKETS,             { "unicast_rcv_packets",            "Unicast Receive Packets",               "pkt", Group::Informative, true } },
            { Type::SYMBOL_ERROR,                    { "symbol_error",                   "Symbol Error Counter",                  "#",   Group::Error,       true } },
            { Type::LINK_ERROR_RECOVERY,             { "link_error_recovery",            "Link Error Recovery Counter",           "#",   Group::Error,       true } },
            { Type::LINK_DOWNED,                     { "link_downed",                    "Link Downed Counter",                   "#",   Group::Error,       true } },
            { Type::LOCAL_LINK_INTEGRITY_ERRORS,     { "local_link_integrity_errors",    "Local Link Integrity Errors",           "#",   Group::Error,       true } },
            { Type::EXCESSIVE_BUFFER_OVERRUN_ERRORS, { "excessive_buffer_overrun_errors","Excessive Buffer Overrun Errors",       "#",   Group::Error,       true } },
            { Type::VL15_DROPPED,                    { "vl15_dropped",                   "VL15 Dropped Counter",                  "#",   Group::Error,       true } },

            /* hw_counters */
            { Type::DUPLICATE_REQUEST,              { "duplicate_request",              "Number of received packets. A duplicate request is a request that had been previously executed.",                                              "pkt", Group::Error,       true } },
            { Type::IMPLIED_NAK_SEQ_ERR,            { "implied_nak_seq_err",            "Number of time the requested decided an ACK. with a PSN larger than the expected PSN for an RDMA read or response.",                           "#",   Group::Error,       true } },
            { Type::LIFESPAN,                       { "lifespan",                       "The maximum period in ms which defines the aging of the counter reads. Two consecutive reads within this period might return the same values", "ms",  Group::Informative, true } },
            { Type::LOCAL_ACK_TIMEOUT_ERR,          { "local_ack_timeout_err",          "The number of times QP's ack timer expired for RC, XRC, DCT QPs at the sender side.",                                                          "#",   Group::Error,       true } },
            { Type::OUT_OF_BUFFER,                  { "out_of_buffer",                  "The number of drops occurred due to lack of WQE for the associated QPs.",                                                                      "#",   Group::Error,       true } },
            { Type::OUT_OF_SEQUENCE,                { "out_of_sequence",                "The number of out of sequence packets received.",                                                                                              "pkt", Group::Error,       true } },
            { Type::PACKET_SEQ_ERR,                 { "packet_seq_err",                 "The number of received NAK sequence error packets. The QP retry limit was not exceeded.",                                                      "pkt", Group::Error,       true } },
            { Type::REQ_CQE_ERROR,                  { "req_cqe_error",                  "The number of times requester detected CQEs completed with errors.",                                                                           "#",   Group::Error,       true } },
            { Type::REQ_CQE_FLUSH_ERROR,            { "req_cqe_flush_error",            "The number of times requester detected CQEs completed with flushed errors.",                                                                   "#",   Group::Error,       true } },
            { Type::REQ_REMOTE_ACCESS_ERRORS,       { "req_remote_access_errors",       "The number of times requester detected remote access errors.",                                                                                 "#",   Group::Error,       true } },
            { Type::REQ_REMOTE_INVALID_REQUEST,     { "req_remote_invalid_request",     "The number of times requester detected remote invalid request errors.",                                                                        "#",   Group::Error,       true } },
            { Type::REQ_RNR_RETRIES_EXCEEDED,       { "req_rnr_retries_exceeded",       "Unknown?",                                                                                                                                     "#",   Group::Error,       true } },
            { Type::REQ_TRANSPORT_RETRIES_EXCEEDED, { "req_transport_retries_exceeded", "Unknown?",                                                                                                                                     "#",   Group::Error,       true } },
            { Type::RESP_CQE_ERROR,                 { "resp_cqe_error",                 "The number of times responder detected CQEs completed with errors.",                                                                           "#",   Group::Error,       true } },
            { Type::RESP_CQE_FLUSH_ERROR,           { "resp_cqe_flush_error",           "The number of times responder detected CQEs completed with flushed errors.",                                                                   "#",   Group::Error,       true } },
            { Type::RESP_LOCAL_LENGTH_ERROR,        { "resp_local_length_error",        "The number of times responder detected local length errors.",                                                                                  "#",   Group::Error,       true } },
            { Type::RESP_REMOTE_ACCESS_ERRORS,      { "resp_remote_access_errors",      "The number of times responder detected remote access errors.",                                                                                 "#",   Group::Error,       true } },
            { Type::RNR_NAK_RETRY_ERR,              { "rnr_nak_retry_err",              "The number of received RNR NAK packets. The QP retry limit was not exceeded.",                                                                 "pkt", Group::Error,       true } },
            { Type::ROCE_ADP_RETRANS,               { "roce_adp_retrans",               "Counts the number of adaptive retransmissions for RoCE traffic",                                                                               "#",   Group::Informative, true } },
            { Type::ROCE_ADP_RETRANS_TO,            { "roce_adp_retrans_to",            "Counts the number of times RoCE traffic reached timeout due to adaptive retransmission",                                                       "#",   Group::Informative, true } },
            { Type::ROCE_SLOW_RESTART,              { "roce_slow_restart",              "Counts the number of times RoCE slow restart was used",                                                                                        "#",   Group::Informative, true } },
            { Type::ROCE_SLOW_RESTART_CNPS,         { "roce_slow_restart_cnps",         "Counts the number of times RoCE slow restart generated CNP packets",                                                                           "#",   Group::Informative, true } },
            { Type::ROCE_SLOW_RESTART_TRANS,        { "roce_slow_restart_trans",        "Counts the number of times RoCE slow restart changed state to slow restart",                                                                   "#",   Group::Informative, true } },
            { Type::RX_ATOMIC_REQUESTS,             { "rx_atomic_requests",             "The number of received ATOMIC request for the associated QPs.",                                                                                "#",   Group::Informative, true } },
            { Type::RX_DCT_CONNECT,                 { "rx_dct_connect",                 "The number of received connection request for the associated DCTs.",                                                                           "#",   Group::Informative, true } },
            { Type::RX_ICRC_ENCAPSULATED,           { "rx_icrc_encapsulated",           "The number of RoCE packets with ICRC errors.",                                                                                                 "pkt", Group::Error,       true } },
            { Type::RX_READ_REQUESTS,               { "rx_read_requests",               "The number of received READ requests for the associated QPs.",                                                                                 "#",   Group::Informative, true } },
            { Type::RX_WRITE_REQUESTS,              { "rx_write_requests",              "The number of received WRITE requests for the associated QPs.",                                                                                "#",   Group::Informative, true } },
        };
    }

    std::string name(void) const
    {
        const auto counters = get_known_counters();
        if (counters.count(type_) != 0)
        {
            return counters.at(type_).name;
        }
        return {};
    }

    std::string description(void) const
    {
        const auto counters = get_known_counters();
        if (counters.count(type_) != 0)
        {
            return counters.at(type_).description;
        }
        return {};
    }

    std::string unit(void) const
    {
        const auto counters = get_known_counters();
        if (counters.count(type_) != 0)
        {
            return counters.at(type_).unit;
        }
        return {};
    }

    bool isAccumulated(void) const
    {
        // sysfs counters are monotonically increasing; use accumulated_point with offset subtraction.
        return true;
    }

    double read(void) const
    {
        const auto      paths = candidate_paths_for_type_();
        std::uint64_t   raw   = 0;
        std::error_code last_ec;

        for (const auto& p : paths)
        {
            if (!fs::exists(p, last_ec) || last_ec)
            {
                continue;
            }
            raw = read_u64_from_file_(p);
            return apply_unit_conversion_(raw);
        }

        throw std::runtime_error("InfinibandCounter: counter not available: device=" +
                                 device_name_ + " port=" + std::to_string(port_) +
                                 " metric=" + name());
    }

    bool supported(void) const
    {
        try {
            (void)read();
            return true;
        }
        catch (...) { return false; }
    }

    const std::string& device_name(void) const
    {
        return device_name_;
    }
    
    uint32_t port(void) const
    {
        return port_;
    }
    
    Type type(void) const
    {
        return type_;
    }

    friend bool operator<( const InfinibandCounter& lhs,
                           const InfinibandCounter& rhs )
    {
        if (lhs.device_name_ != rhs.device_name_)
        {
            return lhs.device_name_ < rhs.device_name_;
        }
        if (lhs.port_ != rhs.port_)
        {
            return lhs.port_ < rhs.port_;
        }
        if (lhs.type_ != rhs.type_)
        {
            return lhs.type_ < rhs.type_;
        }
        return false;
    }

private:
    fs::path base_port_dir_(void) const
    {
        return fs::path("/sys/class/infiniband") / device_name_ / "ports" / std::to_string(port_);
    }

    static std::uint64_t read_u64_from_file_(const fs::path& p)
    {
        std::ifstream f(p);
        if (!f.is_open())
            throw std::runtime_error("InfinibandCounter: failed to open " + p.string());

        std::uint64_t v = 0;
        f >> v;
        if (f.fail())
            throw std::runtime_error("InfinibandCounter: failed to parse u64 from " + p.string());

        return v;
    }

    std::vector<fs::path> candidate_paths_for_type_(void) const
    {
        const fs::path counters     = base_port_dir_() / "counters";
        const fs::path hw_counters  = base_port_dir_() / "hw_counters";

        auto c = [&](const char* fname) { return counters / fname; };
        auto h = [&](const char* fname) { return hw_counters / fname; };

        switch (type_)
        {
            case Type::PORT_XMIT_CONSTRAINT_ERRORS:     return { c("port_xmit_constraint_errors") };
            case Type::PORT_XMIT_DATA:                  return { c("port_xmit_data") };
            case Type::PORT_XMIT_DISCARDS:              return { c("port_xmit_discards") };
            case Type::PORT_XMIT_PACKETS:               return { c("port_xmit_packets") };
            case Type::PORT_XMIT_WAIT:                  return { c("port_xmit_wait") };
            case Type::PORT_RCV_CONSTRAINT_ERRORS:      return { c("port_rcv_constraint_errors") };
            case Type::PORT_RCV_DATA:                   return { c("port_rcv_data") };
            case Type::PORT_RCV_ERRORS:                 return { c("port_rcv_errors") };
            case Type::PORT_RCV_PACKETS:                return { c("port_rcv_packets") };
            case Type::PORT_RCV_REMOTE_PHYSICAL_ERRORS: return { c("port_rcv_remote_physical_errors") };
            case Type::PORT_RCV_SWITCH_RELAY_ERRORS:    return { c("port_rcv_switch_relay_errors") };
            case Type::MULTICAST_XMIT_PACKETS:          return { c("multicast_xmit_packets") };
            case Type::MULTICAST_RCV_PACKETS:           return { c("multicast_rcv_packets") };
            case Type::UNICAST_XMIT_PACKETS:            return { c("unicast_xmit_packets") };
            case Type::UNICAST_RCV_PACKETS:             return { c("unicast_rcv_packets") };
            case Type::SYMBOL_ERROR:                    return { c("symbol_error") };
            case Type::LINK_ERROR_RECOVERY:             return { c("link_error_recovery") };
            case Type::LINK_DOWNED:                     return { c("link_downed") };
            case Type::LOCAL_LINK_INTEGRITY_ERRORS:     return { c("local_link_integrity_errors") };
            case Type::EXCESSIVE_BUFFER_OVERRUN_ERRORS: return { c("excessive_buffer_overrun_errors") };
            case Type::VL15_DROPPED:                    return { c("VL15_dropped"), c("vl15_dropped") };

            case Type::DUPLICATE_REQUEST:              return { h("duplicate_request") };
            case Type::IMPLIED_NAK_SEQ_ERR:            return { h("implied_nak_seq_err") };
            case Type::LIFESPAN:                       return { h("lifespan") };
            case Type::LOCAL_ACK_TIMEOUT_ERR:          return { h("local_ack_timeout_err") };
            case Type::OUT_OF_BUFFER:                  return { h("out_of_buffer") };
            case Type::OUT_OF_SEQUENCE:                return { h("out_of_sequence") };
            case Type::PACKET_SEQ_ERR:                 return { h("packet_seq_err") };
            case Type::REQ_CQE_ERROR:                  return { h("req_cqe_error") };
            case Type::REQ_CQE_FLUSH_ERROR:            return { h("req_cqe_flush_error") };
            case Type::REQ_REMOTE_ACCESS_ERRORS:       return { h("req_remote_access_errors") };
            case Type::REQ_REMOTE_INVALID_REQUEST:     return { h("req_remote_invalid_request") };
            case Type::REQ_RNR_RETRIES_EXCEEDED:       return { h("req_rnr_retries_exceeded") };
            case Type::REQ_TRANSPORT_RETRIES_EXCEEDED: return { h("req_transport_retries_exceeded") };
            case Type::RESP_CQE_ERROR:                 return { h("resp_cqe_error") };
            case Type::RESP_CQE_FLUSH_ERROR:           return { h("resp_cqe_flush_error") };
            case Type::RESP_LOCAL_LENGTH_ERROR:        return { h("resp_local_length_error") };
            case Type::RESP_REMOTE_ACCESS_ERRORS:      return { h("resp_remote_access_errors") };
            case Type::RNR_NAK_RETRY_ERR:              return { h("rnr_nak_retry_err") };
            case Type::ROCE_ADP_RETRANS:               return { h("roce_adp_retrans") };
            case Type::ROCE_ADP_RETRANS_TO:            return { h("roce_adp_retrans_to") };
            case Type::ROCE_SLOW_RESTART:              return { h("roce_slow_restart") };
            case Type::ROCE_SLOW_RESTART_CNPS:         return { h("roce_slow_restart_cnps") };
            case Type::ROCE_SLOW_RESTART_TRANS:        return { h("roce_slow_restart_trans") };
            case Type::RX_ATOMIC_REQUESTS:             return { h("rx_atomic_requests") };
            case Type::RX_DCT_CONNECT:                 return { h("rx_dct_connect") };
            case Type::RX_ICRC_ENCAPSULATED:           return { h("rx_icrc_encapsulated") };
            case Type::RX_READ_REQUESTS:               return { h("rx_read_requests") };
            case Type::RX_WRITE_REQUESTS:              return { h("rx_write_requests") };

            default:
                return {};
        }
    }

    double apply_unit_conversion_(std::uint64_t raw) const
    {
        // IB PortXmitData / PortRcvData are in 32-bit words.
        switch (type_)
        {
        case Type::PORT_XMIT_DATA:
        case Type::PORT_RCV_DATA:
            return static_cast<double>(raw) * 4.0;
        default:
            return static_cast<double>(raw);
        }
    }

private:
    std::string device_name_;
    uint32_t port_{0};
    Type type_{Type::INVALID};
};
