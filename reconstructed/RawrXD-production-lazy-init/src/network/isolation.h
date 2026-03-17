/**
 * @file network_isolation.h
 * @brief Network isolation and firewall rule management
 */

#ifndef NETWORK_ISOLATION_H
#define NETWORK_ISOLATION_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <Fwpmu.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "fwpuclnt.lib")
#else
// Linux/Unix stubs - would use netfilter/iptables in production
#endif

/**
 * @class NetworkAddress
 * @brief IP address and port specification
 */
class NetworkAddress
{
public:
    enum IPVersion { IPv4, IPv6 };
    enum Direction { INBOUND, OUTBOUND, BIDIRECTIONAL };
    
    QString ipAddress;  // CIDR notation (e.g., "192.168.1.0/24" or "0.0.0.0/0" for any)
    int port = 0;       // 0 = any port
    IPVersion version = IPv4;
    
    NetworkAddress() = default;
    NetworkAddress(const QString& ip, int p = 0, IPVersion v = IPv4)
        : ipAddress(ip), port(p), version(v) {}
    
    bool isValid() const;
    QJsonObject toJson() const;
    static NetworkAddress fromJson(const QJsonObject& json);
    
    static NetworkAddress anyIPv4(int port = 0) { return NetworkAddress("0.0.0.0/0", port, IPv4); }
    static NetworkAddress anyIPv6(int port = 0) { return NetworkAddress("::/0", port, IPv6); }
};

/**
 * @class FirewallRule
 * @brief Network firewall rule
 */
class FirewallRule
{
public:
    enum Action { ALLOW, BLOCK, LOG };
    enum Protocol { ANY, TCP, UDP, ICMP };
    
    QString ruleId;
    QString ruleName;
    QString description;
    
    bool enabled = true;
    Action action = ALLOW;
    Protocol protocol = ANY;
    NetworkAddress::Direction direction = NetworkAddress::OUTBOUND;
    
    NetworkAddress source;
    NetworkAddress destination;
    
    int priority = 1000;  // Lower = higher priority
    bool logMatches = false;
    
    FirewallRule() = default;
    
    QJsonObject toJson() const;
    static FirewallRule fromJson(const QJsonObject& json);
    
    bool matches(const QString& srcIP, int srcPort, const QString& dstIP, int dstPort, 
                Protocol proto, NetworkAddress::Direction dir) const;
};

/**
 * @class DnsFilter
 * @brief DNS filtering and redirection
 */
class DnsFilter
{
public:
    enum FilterAction { ALLOW, BLOCK, REDIRECT };
    
    QString domain;
    FilterAction action = ALLOW;
    QString redirectAddress;  // For REDIRECT action
    
    bool matchesWildcard(const QString& queryDomain) const;
    
    QJsonObject toJson() const;
    static DnsFilter fromJson(const QJsonObject& json);
};

/**
 * @class TrafficInspector
 * @brief Monitor and inspect network traffic
 */
class TrafficInspector
{
public:
    struct TrafficStatistics {
        uint64_t bytesIn = 0;
        uint64_t bytesOut = 0;
        uint64_t packetsIn = 0;
        uint64_t packetsOut = 0;
        uint64_t connectionCount = 0;
    };
    
    struct ConnectionInfo {
        QString localAddress;
        int localPort = 0;
        QString remoteAddress;
        int remotePort = 0;
        QString protocol;
        QString state;
        uint32_t processId = 0;
        uint64_t bytesTransferred = 0;
    };
    
    TrafficInspector();
    
    /**
     * Get current traffic statistics
     */
    TrafficStatistics getStatistics() const;
    
    /**
     * Get active connections
     */
    std::vector<ConnectionInfo> getActiveConnections() const;
    
    /**
     * Get connections for a specific process
     */
    std::vector<ConnectionInfo> getProcessConnections(uint32_t processId) const;
    
    /**
     * Start monitoring (platform-specific)
     */
    void startMonitoring();
    
    /**
     * Stop monitoring
     */
    void stopMonitoring();
    
    /**
     * Clear statistics
     */
    void clearStatistics();
    
    /**
     * Get statistics for a specific IP
     */
    TrafficStatistics getStatisticsForIP(const QString& ipAddress) const;
    
private:
    mutable QMutex m_mutex;
    TrafficStatistics m_stats;
    std::vector<ConnectionInfo> m_connections;
    bool m_monitoring = false;
    
    // Platform-specific methods
    void updateConnectionsWindows();
    void updateConnectionsLinux();
};

/**
 * @class NetworkIsolationPolicy
 * @brief Complete network isolation policy
 */
class NetworkIsolationPolicy
{
public:
    enum IsolationLevel {
        NONE,        // No restrictions
        RESTRICTED,  // Only allow whitelisted addresses
        ISOLATED,    // Block all external, allow only internal
        AIRGAPPED    // No network access at all
    };
    
    IsolationLevel level = NONE;
    bool blockDns = false;
    bool allowLoopback = true;
    bool allowLocalNetwork = true;  // 192.168.0.0/16, 10.0.0.0/8, etc.
    
    std::vector<NetworkAddress> whitelistedAddresses;
    std::vector<FirewallRule> rules;
    std::vector<DnsFilter> dnsFilters;
    
    QJsonObject toJson() const;
    static NetworkIsolationPolicy fromJson(const QJsonObject& json);
};

/**
 * @class NetworkIsolationManager
 * @brief Main network isolation enforcement
 */
class NetworkIsolationManager : public QObject
{
    Q_OBJECT
    
public:
    explicit NetworkIsolationManager(QObject* parent = nullptr);
    ~NetworkIsolationManager();
    
    /**
     * Add a firewall rule
     */
    QString addRule(const FirewallRule& rule);
    
    /**
     * Remove a firewall rule
     */
    bool removeRule(const QString& ruleId);
    
    /**
     * Update a firewall rule
     */
    bool updateRule(const QString& ruleId, const FirewallRule& rule);
    
    /**
     * Get all rules
     */
    std::vector<FirewallRule> getAllRules() const;
    
    /**
     * Get rule by ID
     */
    FirewallRule getRule(const QString& ruleId) const;
    
    /**
     * Clear all rules
     */
    void clearAllRules();
    
    /**
     * Add DNS filter
     */
    QString addDnsFilter(const DnsFilter& filter);
    
    /**
     * Remove DNS filter
     */
    bool removeDnsFilter(const QString& domain);
    
    /**
     * Get all DNS filters
     */
    std::vector<DnsFilter> getAllDnsFilters() const;
    
    /**
     * Clear all DNS filters
     */
    void clearDnsFilters();
    
    /**
     * Set isolation policy
     */
    void setIsolationPolicy(const NetworkIsolationPolicy& policy);
    
    /**
     * Get current isolation policy
     */
    NetworkIsolationPolicy getIsolationPolicy() const;
    
    /**
     * Check if connection is allowed
     */
    bool isConnectionAllowed(const QString& srcIP, int srcPort, 
                           const QString& dstIP, int dstPort,
                           FirewallRule::Protocol protocol = FirewallRule::ANY) const;
    
    /**
     * Check if DNS query is allowed
     */
    bool isDnsQueryAllowed(const QString& domain) const;
    
    /**
     * Get blocking reason
     */
    QString getBlockingReason(const QString& srcIP, int srcPort, 
                             const QString& dstIP, int dstPort) const;
    
    /**
     * Get traffic inspector
     */
    TrafficInspector* getTrafficInspector() { return m_trafficInspector.get(); }
    
    /**
     * Enable rule enforcement on system (Windows Firewall API)
     */
    bool enforceRulesOnSystem();
    
    /**
     * Disable rule enforcement
     */
    bool disableEnforcement();
    
    /**
     * Get enforcement status
     */
    bool isEnforcementActive() const { return m_enforcementActive; }
    
    /**
     * Export configuration
     */
    QJsonObject exportConfiguration() const;
    
    /**
     * Import configuration
     */
    bool importConfiguration(const QJsonObject& json);
    
signals:
    /**
     * Emitted when a rule is added
     */
    void ruleAdded(const QString& ruleId);
    
    /**
     * Emitted when a rule is removed
     */
    void ruleRemoved(const QString& ruleId);
    
    /**
     * Emitted when a connection is blocked
     */
    void connectionBlocked(const QString& srcIP, int srcPort, const QString& dstIP, int dstPort);
    
    /**
     * Emitted when a DNS query is blocked
     */
    void dnsQueryBlocked(const QString& domain);
    
    /**
     * Emitted when enforcement is toggled
     */
    void enforcementStatusChanged(bool active);
    
    /**
     * Emitted on error
     */
    void errorOccurred(const QString& error);
    
private:
    mutable QMutex m_mutex;
    
    std::map<QString, FirewallRule> m_rules;
    std::vector<DnsFilter> m_dnsFilters;
    NetworkIsolationPolicy m_policy;
    std::unique_ptr<TrafficInspector> m_trafficInspector;
    
    bool m_enforcementActive = false;
    
    // Windows-specific handles
#ifdef _WIN32
    HANDLE m_filterEngineHandle = NULL;
    HANDLE m_providerHandle = NULL;
    std::vector<UINT64> m_filterIds;
#endif
    
    /**
     * Generate unique rule ID
     */
    QString generateRuleId();
    
    /**
     * Apply rules to Windows Firewall
     */
    bool applyRulesToWindowsFirewall();
    
    /**
     * Clear Windows Firewall rules
     */
    bool clearWindowsFirewallRules();
};

#endif // NETWORK_ISOLATION_H
