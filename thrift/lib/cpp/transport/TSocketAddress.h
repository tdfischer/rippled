/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#ifndef THRIFT_TRANSPORT_TSOCKETADDRESS_H_
#define THRIFT_TRANSPORT_TSOCKETADDRESS_H_ 1

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <features.h>
#include <netdb.h>
#include <cstddef>
#include <iostream>
#include <string>

namespace apache { namespace thrift { namespace transport {

class TSocketAddress {
 public:
  TSocketAddress() {
    storage_.addr.sa_family = AF_UNSPEC;
  }

  /**
   * Construct a TSocketAddress from a hostname and port.
   *
   * Note: If the host parameter is not a numeric IP address, hostname
   * resolution will be performed, which can be quite slow.
   *
   * Raises TTransportException on error.
   *
   * @param host The IP address (or hostname, if allowNameLookup is true)
   * @param port The port (in host byte order)
   * @pram allowNameLookup  If true, attempt to perform hostname lookup
   *        if the hostname does not appear to be a numeric IP address.
   *        This is potentially a very slow operation, so is disabled by
   *        default.
   */
  TSocketAddress(const char* host, uint16_t port,
                 bool allowNameLookup = false) {
    // Initialize the address family first,
    // since setFromHostPort() and setFromIpPort() will check it.
    storage_.addr.sa_family = AF_UNSPEC;

    if (allowNameLookup) {
      setFromHostPort(host, port);
    } else {
      setFromIpPort(host, port);
    }
  }

  TSocketAddress(const std::string& host, uint16_t port,
                 bool allowNameLookup = false) {
    // Initialize the address family first,
    // since setFromHostPort() and setFromIpPort() will check it.
    storage_.addr.sa_family = AF_UNSPEC;

    if (allowNameLookup) {
      setFromHostPort(host.c_str(), port);
    } else {
      setFromIpPort(host.c_str(), port);
    }
  }

  TSocketAddress(const TSocketAddress& addr) {
    storage_ = addr.storage_;
    if (addr.getFamily() == AF_UNIX) {
      storage_.un.init(addr.storage_.un);
    }
  }

  TSocketAddress& operator=(const TSocketAddress& addr) {
    if (getFamily() != AF_UNIX) {
      if (addr.getFamily() != AF_UNIX) {
        storage_ = addr.storage_;
      } else {
        storage_ = addr.storage_;
        storage_.un.init(addr.storage_.un);
      }
    } else {
      if (addr.getFamily() == AF_UNIX) {
        storage_.un.copy(addr.storage_.un);
      } else {
        storage_.un.free();
        storage_ = addr.storage_;
      }
    }
    return *this;
  }

#if __GXX_EXPERIMENTAL_CXX0X__
  TSocketAddress(TSocketAddress&& addr) {
    storage_ = addr.storage_;
    addr.storage_.addr.sa_family = AF_UNSPEC;
  }

#if __GNUC_PREREQ(4, 5)
  TSocketAddress& operator=(TSocketAddress&& addr) {
    std::swap(storage_, addr.storage_);
    return *this;
  }
#endif
#endif

  ~TSocketAddress() {
    if (storage_.addr.sa_family == AF_UNIX) {
      storage_.un.free();
    }
  }

  bool isInitialized() const {
    return (storage_.addr.sa_family != AF_UNSPEC);
  }

  /**
   * Return whether this address is within private network.
   *
   * According to RFC1918, the 10/8 prefix, 172.16/12 prefix, and 192.168/16
   * prefix are reserved for private networks.
   * fc00::/7 is the IPv6 version, defined in RFC4139.  IPv6 link-local
   * addresses (fe80::/10) are also considered private addresses.
   *
   * The loopback addresses 127/8 and ::1 are also regarded as private networks
   * for the purpose of this function.
   *
   * Returns true if this is a private network address, and false otherwise.
   */
  bool isPrivateAddress() const;

  /**
   * Return whether this address is a loopback address.
   */
  bool isLoopbackAddress() const;

  void reset() {
    prepFamilyChange(AF_UNSPEC);
    storage_.addr.sa_family = AF_UNSPEC;
  }

  /**
   * Initialize this TSocketAddress from a hostname and port.
   *
   * Note: If the host parameter is not a numeric IP address, hostname
   * resolution will be performed, which can be quite slow.
   *
   * If the hostname resolves to multiple addresses, only the first will be
   * returned.
   *
   * Raises TTransportException on error.
   *
   * @param host The hostname or IP address
   * @param port The port (in host byte order)
   */
  void setFromHostPort(const char* host, uint16_t port);

  void setFromHostPort(const std::string& host, uint16_t port) {
    setFromHostPort(host.c_str(), port);
  }

  /**
   * Initialize this TSocketAddress from an IP address and port.
   *
   * This is similar to setFromHostPort(), but only accepts numeric IP
   * addresses.  If the IP string does not look like an IP address, it throws a
   * TTransportException rather than trying to perform a hostname resolution.
   *
   * Raises TTransportException on error.
   *
   * @param ip The IP address, as a human-readable string.
   * @param port The port (in host byte order)
   */
  void setFromIpPort(const char* ip, uint16_t port);

  void setFromIpPort(const std::string& ip, uint16_t port) {
    setFromIpPort(ip.c_str(), port);
  }

  /**
   * Initialize this TSocketAddress from a local port number.
   *
   * This is intended to be used by server code to determine the address to
   * listen on.
   *
   * If the current machine has any IPv6 addresses configured, an IPv6 address
   * will be returned (since connections from IPv4 clients can be mapped to the
   * IPv6 address).  If the machine does not have any IPv6 addresses, an IPv4
   * address will be returned.
   */
  void setFromLocalPort(uint16_t port);

  /**
   * Initialize this TSocketAddress from a local port number.
   *
   * This version of setFromLocalPort() accepts the port as a string.  A
   * TTransportException will be raised if the string does not refer to a port
   * number.  Non-numeric service port names are not accepted.
   */
  void setFromLocalPort(const char* port);
  void setFromLocalPort(const std::string& port) {
    return setFromLocalPort(port.c_str());
  }

  /**
   * Initialize this TSocketAddress from a local port number and optional IP
   * address.
   *
   * The addressAndPort string may be specified either as "<ip>:<port>", or
   * just as "<port>".  If the IP is not specified, the address will be
   * initialized to 0, so that a server socket bound to this address will
   * accept connections on all local IP addresses.
   *
   * Both the IP address and port number must be numeric.  DNS host names and
   * non-numeric service port names are not accepted.
   */
  void setFromLocalIpPort(const char* addressAndPort);
  void setFromLocalIpPort(const std::string& addressAndPort) {
    return setFromLocalIpPort(addressAndPort.c_str());
  }

  /**
   * Initialize this TSocketAddress from an IP address and port number.
   *
   * The addressAndPort string must be of the form "<ip>:<port>".  E.g.,
   * "10.0.0.1:1234".
   *
   * Both the IP address and port number must be numeric.  DNS host names and
   * non-numeric service port names are not accepted.
   */
  void setFromIpPort(const char* addressAndPort);
  void setFromIpPort(const std::string& addressAndPort) {
    return setFromIpPort(addressAndPort.c_str());
  }

  /**
   * Initialize this TSocketAddress from a host name and port number.
   *
   * The addressAndPort string must be of the form "<host>:<port>".  E.g.,
   * "www.facebook.com:443".
   *
   * If the host name is not a numeric IP address, a DNS lookup will be
   * performed.  Beware that the DNS lookup may be very slow.  The port number
   * must be numeric; non-numeric service port names are not accepted.
   */
  void setFromHostPort(const char* hostAndPort);
  void setFromHostPort(const std::string& hostAndPort) {
    return setFromHostPort(hostAndPort.c_str());
  }

  /**
   * Initialize this TSocketAddress from a local unix path.
   *
   * Raises TTransportException on error.
   */
  void setFromPath(const char* path) {
    setFromPath(path, strlen(path));
  }

  void setFromPath(const std::string& path) {
    setFromPath(path.data(), path.length());
  }

  void setFromPath(const char* path, size_t length);

  /**
   * Initialize this TSocketAddress from a socket's peer address.
   *
   * Raises TTransportException on error.
   */
  void setFromPeerAddress(int socket);

  /**
   * Initialize this TSocketAddress from a socket's local address.
   *
   * Raises TTransportException on error.
   */
  void setFromLocalAddress(int socket);

  /**
   * Initialize this TSocketAddress from a struct sockaddr.
   *
   * Raises TTransportException on error.
   *
   * This method is not supported for AF_UNIX addresses.  For unix addresses,
   * the address length must be explicitly specified.
   *
   * @param address  A struct sockaddr.  The size of the address is implied
   *                 from address->sa_family.
   */
  void setFromSockaddr(const struct sockaddr* address);

  /**
   * Initialize this TSocketAddress from a struct sockaddr.
   *
   * Raises TTransportException on error.
   *
   * @param address  A struct sockaddr.
   * @param addrlen  The length of address data available.  This must be long
   *                 enough for the full address type required by
   *                 address->sa_family.
   */
  void setFromSockaddr(const struct sockaddr* address,
                       socklen_t addrlen);

  /**
   * Initialize this TSocketAddress from a struct sockaddr_in.
   */
  void setFromSockaddr(const struct sockaddr_in* address);

  /**
   * Initialize this TSocketAddress from a struct sockaddr_in6.
   */
  void setFromSockaddr(const struct sockaddr_in6* address);

  /**
   * Initialize this TSocketAddress from a struct sockaddr_un.
   *
   * Note that the addrlen parameter is necessary to properly detect anonymous
   * addresses, which have 0 valid path bytes, and may not even have a NUL
   * character at the start of the path.
   *
   * @param address  A struct sockaddr_un.
   * @param addrlen  The length of address data.  This should include all of
   *                 the valid bytes of sun_path, not including any NUL
   *                 terminator.
   */
  void setFromSockaddr(const struct sockaddr_un* address,
                       socklen_t addrlen);

  /**
   * Get a pointer to the struct sockaddr data that can be used for manually
   * modifying the data.
   *
   * addressUpdated() must be called after you finish modifying the socket data
   * before you perform any other operations on the TSocketAddress.
   *
   * For example, to use this to store the address returned by an accept()
   * call:
   *
   *   socklen_t addrlen;
   *   struct sockaddr *storage = addr.getMutableAddress(AF_INET, &addrlen);
   *   int newSock = accept(sock, storage, &addrlen);
   *   if (newSock < 0) {
   *     // error handling
   *   }
   *   addr.addressUpdated(AF_INET, addrlen);
   *
   * @param family     The type of address data you plan to put in the
   *                   sockaddr.  This is necessary since some address families
   *                   require more storage than others.
   * @param sizeReturn The length of the returned sockaddr will be returned via
   *                   this argument.
   */
  struct sockaddr* getMutableAddress(sa_family_t family,
                                     socklen_t *sizeReturn);

  /**
   * Indicate that the address data was updated after a call to
   * getMutableAddress().
   *
   * @param expectedFamily  This must be the same value that you passed to
   *                        the getMutableAddress() call.  This is used to
   *                        verify that the address data written into the
   *                        sockaddr is actually of the same type that you
   *                        specified when you called getMutableAddress().
   * @param addrlen         The length of the new address data written into the
   *                        sockaddr.
   */
  void addressUpdated(sa_family_t expectedFamily, socklen_t addrlen) {
    if (getFamily() != expectedFamily) {
      // This should pretty much never happen.
      addressUpdateFailure(expectedFamily);
    }
    if (getFamily() == AF_UNIX) {
      updateUnixAddressLength(addrlen);
    }
  }

  const struct sockaddr* getAddress() const {
    if (getFamily() != AF_UNIX) {
      return &storage_.addr;
    } else {
      return reinterpret_cast<const struct sockaddr*>(storage_.un.addr);
    }
  }

  /**
   * Return the total number of bytes available for address storage.
   */
  socklen_t getStorageSize() const {
    if (getFamily() != AF_UNIX) {
      return sizeof(storage_);
    } else {
      return sizeof(*storage_.un.addr);
    }
  }

  /**
   * Return the number of bytes actually used for this address.
   *
   * For an uninitialized socket, this returns sizeof(struct sockaddr),
   * even though some of those bytes may not be initialized.
   */
  socklen_t getActualSize() const;

  sa_family_t getFamily() const {
    return storage_.addr.sa_family;
  }

  /**
   * Get a string representation of the IPv4 or IPv6 address.
   *
   * Raises TTransportException if an error occurs (for example, if the address
   * is not an IPv4 or IPv6 address).
   */
  std::string getAddressStr() const;

  /**
   * Get a string representation of the IPv4 or IPv6 address.
   *
   * Raises TTransportException if an error occurs (for example, if the address
   * is not an IPv4 or IPv6 address).
   */
  void getAddressStr(char* buf, size_t buflen) const;

  /**
   * Get the IPv4 or IPv6 port for this address.
   *
   * Raises TTransportException if this is not an IPv4 or IPv6 address.
   *
   * @return Returns the port, in host byte order.
   */
  uint16_t getPort() const;

  /**
   * Set the IPv4 or IPv6 port for this address.
   *
   * Raises TTransportException if this is not an IPv4 or IPv6 address.
   */
  void setPort(uint16_t port);

  /**
   * Return true if this is an IPv4-mapped IPv6 address.
   */
  bool isIPv4Mapped() const {
    return (storage_.addr.sa_family == AF_INET6 &&
            IN6_IS_ADDR_V4MAPPED(&storage_.ipv6.sin6_addr));
  }

  /**
   * Convert an IPv4-mapped IPv6 address to an IPv4 address.
   *
   * Raises TTransportException if this is not an IPv4-mapped IPv6 address.
   */
  void convertToIPv4();

  /**
   * Try to convert an address to IPv4.
   *
   * This attempts to convert an address to an IPv4 address if possible.
   * If the address is an IPv4-mapped IPv6 address, it is converted to an IPv4
   * address and true is returned.  Otherwise nothing is done, and false is
   * returned.
   */
  bool tryConvertToIPv4();

  /**
   * Get string representation of the host name (or IP address if the host name
   * cannot be resolved).
   *
   * Warning: Using this method is strongly discouraged.  It performs a
   * DNS lookup, which may block for many seconds.
   *
   * Raises TTransportException if an error occurs.
   */
  std::string getHostStr() const;

  /**
   * Get the path name for a Unix domain socket.
   *
   * Returns a std::string containing the path.  For anonymous sockets, an
   * empty string is returned.
   *
   * For addresses in the abstract namespace (Linux-specific), a std::string
   * containing binary data is returned.  In this case the first character will
   * always be a NUL character.
   *
   * Raises TTransportException if called on a non-Unix domain socket.
   */
  std::string getPath() const;

  /**
   * Get human-readable string representation of the address.
   *
   * This prints a string representation of the address, for human consumption.
   * For IP addresses, the string is of the form "<IP>:<port>".
   */
  std::string describe() const;

  bool operator==(const TSocketAddress& other) const;
  bool operator!=(const TSocketAddress& other) const {
    return !(*this == other);
  }

  /**
   * Check whether the first N bits of this address match the first N
   * bits of another address.
   * @note returns false if the addresses are not from the same
   *       address family or if the family is neither IPv4 nor IPv6
   */
  bool prefixMatch(const TSocketAddress& other, unsigned prefixLength) const;

  /**
   * Use this operator for storing maps based on TSocketAddress.
   */
  bool operator<(const TSocketAddress& other) const;

  /**
   * Compuate a hash of a TSocketAddress.
   */
  size_t hash() const;

 private:
  /**
   * Unix socket addresses require more storage than IPv4 and IPv6 addresses,
   * and are comparatively little-used.
   *
   * Therefore TSocketAddress' internal storage_ member variable doesn't
   * contain room for a full unix address, to avoid wasting space in the common
   * case.  When we do need to store a Unix socket address, we use this
   * ExternalUnixAddr structure to allocate a struct sockaddr_un separately on
   * the heap.
   */
  struct ExternalUnixAddr {
    sa_family_t family;
    struct sockaddr_un *addr;
    socklen_t len;

    socklen_t pathLength() const {
      return len - offsetof(struct sockaddr_un, sun_path);
    }

    void init() {
      family = AF_UNIX;
      addr = new sockaddr_un;
      addr->sun_family = AF_UNIX;
      len = 0;
    }
    void init(const ExternalUnixAddr &other) {
      family = AF_UNIX;
      addr = new sockaddr_un;
      len = other.len;
      memcpy(addr, other.addr, len);
      // Fill the rest with 0s, just for safety
      memset(reinterpret_cast<char*>(addr) + len, 0,
             sizeof(struct sockaddr_un) - len);
    }
    void copy(const ExternalUnixAddr &other) {
      len = other.len;
      memcpy(addr, other.addr, len);
    }
    void free() {
      family = AF_UNSPEC;
      delete addr;
    }
  };

  struct addrinfo* getAddrInfo(const char* host, uint16_t port, int flags);
  struct addrinfo* getAddrInfo(const char* host, const char* port, int flags);
  void setFromAddrInfo(const struct addrinfo* results);
  void setFromLocalAddr(const struct addrinfo* results);
  int setFromSocket(int socket, int (*fn)(int, struct sockaddr*, socklen_t*));
  std::string getIpString(int flags) const;
  void getIpString(char *buf, size_t buflen, int flags) const;

  void addressUpdateFailure(sa_family_t expectedFamily);
  void updateUnixAddressLength(socklen_t addrlen);

  void prepFamilyChange(sa_family_t newFamily) {
    if (newFamily != AF_UNIX) {
      if (getFamily() == AF_UNIX) {
        storage_.un.free();
      }
    } else {
      if (getFamily() != AF_UNIX) {
        storage_.un.init();
      }
    }
  }

  /*
   * storage_ contains room for a full IPv4 or IPv6 address, so they can be
   * stored inline without a separate allocation on the heap.
   *
   * If we need to store a Unix socket address, ExternalUnixAddr is a shim to
   * track a struct sockaddr_un allocated separately on the heap.
   */
  union {
    sockaddr addr;
    sockaddr_in ipv4;
    sockaddr_in6 ipv6;
    ExternalUnixAddr un;
  } storage_;
};

/**
 * Hash a TSocketAddress object.
 *
 * boost::hash uses hash_value(), so this allows boost::hash to automatically
 * work for TSocketAddress.
 */
size_t hash_value(const TSocketAddress& address);

std::ostream& operator<<(std::ostream& os, const TSocketAddress& addr);

}}} // apache::thrift::transport

namespace std {

// Provide an implementation for std::hash<TSocketAddress>
template<>
struct hash<apache::thrift::transport::TSocketAddress> {
  size_t operator()(
      const apache::thrift::transport::TSocketAddress& addr) const {
    return addr.hash();
  }
};

}

#endif // THRIFT_TRANSPORT_TSOCKETADDRESS_H_