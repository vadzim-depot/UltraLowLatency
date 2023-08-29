#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "Macros.h"

#include "Logging.h"

namespace Common
{
    /// Represents the maximum number of pending / unaccepted TCP connections.
    constexpr int MaxTCPServerBacklog = 1024;

    /// Convert interface name "eth0" to ip "123.123.123.123".
    auto GetIFaceIP(const std::string &iface) -> std::string;

    /// Sockets will not block on read, but instead return immediately if data is not available.
    auto SetNonBlocking(int fd) -> bool;

    /// Disable Nagle's algorithm and associated delays.
    auto SetNoDelay(int fd) -> bool;

    /// Allow software receive timestamps on incoming packets.
    auto SetSOTimestamp(int fd) -> bool;

    /// Check the errno variable to see if an operation would have blocked if the socket was not set to non-blocking.
    auto WouldBlock() -> bool;

    /// Set the Time To Live (TTL) value on multicast packets so they can only pass through a certain number of hops before being discarded.
    auto SetMultiCastTTL(int fd, int ttl) -> bool;

    /// Set the Time To Live (TTL) value on non-multicast packets so they can only pass through a certain number of hops before being discarded.
    auto SetTTL(int fd, int ttl) -> bool;

    /// Add / Join membership / subscription to the multicast stream specified and on the interface specified.
    auto Join(int fd, const std::string &ip, const std::string &iface, int port) -> bool;

    /// Create a TCP / UDP socket to either connect to or listen for data on or listen for connections on the specified interface and IP:port information.
    auto CreateSocket(CLogger& logger, const std::string& t_ip, const std::string& iface, int port, bool is_udp, bool is_blocking, bool is_listening, int ttl, bool needs_so_timestamp) -> int;
}
