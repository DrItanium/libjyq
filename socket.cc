/* Copyright ©2007-2010 Kris Maglione <maglione.k at Gmail>
 * Copyright ©2004-2006 Anselm R. Garbe <garbeam at gmail dot com>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <map>
#include <string>
#include "Msg.h"
#include "jyq.h"
#include "socket.h"


/* Note: These functions modify the strings that they are passed.
 *   The lookup function duplicates the original string, so it is
 *   not modified.
 */

namespace jyq {
namespace {
std::string
getPort(const std::string& addr) {
    if (auto spos = addr.find('!'); spos == std::string::npos) {
        throw Exception("no port provided");
    } else {
        return addr.substr(spos);
    }
}

int
sock_unix(const std::string& address, sockaddr_un *sa, socklen_t *salen) {

	memset(sa, 0, sizeof *sa);

	sa->sun_family = AF_UNIX;
    address.copy(sa->sun_path, sizeof(sa->sun_path));
	*salen = SUN_LEN(sa);

	if (auto fd = socket(AF_UNIX, SOCK_STREAM, 0); fd < 0) {
        return -1;
    } else {
        return fd;
    }
}

int
dial_unix(const std::string& address) {
	sockaddr_un sa;
	socklen_t salen;

    if (int fd = sock_unix(address, &sa, &salen); fd == -1) {
        return fd;
    } else {
        if(connect(fd, (sockaddr*) &sa, salen)) {
            ::close(fd);
            return -1;
        }
        return fd;
    }
}

int
announce_unix(const std::string& file) {
	sockaddr_un sa;
	socklen_t salen;

	signal(SIGPIPE, SIG_IGN);

    if (int fd = sock_unix(file, &sa, &salen); fd == -1) {
        return fd;
    } else {
        auto fail = [](int fd) {
            ::close(fd);
            return -1;
        };
	    const int yes = 1;
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof yes) < 0) {
            return fail(fd);
        }
        unlink(file.c_str());
        if(bind(fd, (sockaddr*)&sa, salen) < 0) {
            return fail(fd);
        }
        chmod(file.c_str(), S_IRWXU);
        if(::listen(fd, maximum::Cache) < 0) {
            return fail(fd);
        }

        return fd;
    }

}

template<bool announce>
addrinfo*
alookup(const std::string& host) {
	/* Truncates host at '!' */
    if (auto port = getPort(host); port.empty()) {
        return nullptr;
    } else {
        bool useHost = true;
        addrinfo hints;
        addrinfo* ret = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        /// @todo figure out how to zero out a c structure without memset
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if constexpr (announce) {
            hints.ai_flags = AI_PASSIVE;
            // was originally if !host.compare("*") then useHost = false
            useHost = host.compare("*");
        }

        if (int err = getaddrinfo(useHost ? host.c_str() : nullptr, port.c_str(), &hints, &ret); err) {
            wErrorString("getaddrinfo: ", gai_strerror(err));
            return nullptr;
        } else {
            return ret;
        }
    }
}

int
ai_socket(addrinfo *ai) {
	return socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
}

int
dial_tcp(const std::string& host) {
	if (auto aip = alookup<false>(host); !aip) {
        return -1;
    } else {
        int fd;
        // delay throwing until the last possible point since we have to go
        // through a set of connections
        std::optional<jyq::Exception> potentialError = std::nullopt;
        for(auto ai = aip; ai; ai = ai->ai_next) {
            fd = ai_socket(ai);
            if(fd == -1) {
                potentialError.emplace("socket: ", strerror(errno));
                continue;
            }

            if(connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
                // clear any potential errors since we got there successfully
                potentialError.reset();
                break;
            }

            ::close(fd);
            fd = -1;
            potentialError.emplace("connect: ", strerror(errno));
        }
        

        freeaddrinfo(aip);
        if (potentialError) {
            throw *potentialError;
        } else {
            return fd;
        }
    }
}

int
announce_tcp(const std::string& host) {
    if (addrinfo* aip = alookup<true>(host); !aip) {
        return -1;
    } else {
        int fd = 0;
        /* Probably don't need to loop */
        for(addrinfo* ai = aip; ai; ai = ai->ai_next) {
            fd = ai_socket(ai);
            if(fd == -1) {
                continue;
            }

            if(bind(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
                ::close(fd);
                fd = -1;
                continue;
            }

            if(::listen(fd, maximum::Cache) < 0) {
                ::close(fd);
                fd = -1;
                continue;
            }
            break;
        }

        freeaddrinfo(aip);
        return fd;
    }
}

} // end namespace

std::tuple<std::string, std::string>
Connection::decompose(const std::string& address) {
	if (auto addrPos = address.find('!'); addrPos == std::string::npos) {
        throw Exception("no address type defined!");
    } else {
        std::string type(address.substr(0, addrPos));
        return std::make_tuple(address.substr(0, addrPos), address.substr(addrPos+1));
	}
}

/**
 * Function: dial
 * Function: announce
 *
 * Params:
 *	address: An address on which to connect or listen,
 *		 specified in the Plan 9 resources
 *		 specification format
 *		 (<protocol>!address[!<port>])
 *
 * These functions hide some of the ugliness of Berkely
 * Sockets. dial connects to the resource at P<address>,
 * while announce begins listening on P<address>.
 *
 * Returns:
 *	These functions return file descriptors on success, and -1
 *	on failure. errbuf(3) may be inspected on failure.
 * See also:
 *	socket(2)
 */
Connection
Connection::dial(const std::string& address) {
    auto [kind, path] = decompose(address);
    if (auto target = getCtab().find(kind); target != getCtab().end()) {
        return Connection(target->second.dial(path));
    } else {
        throw Exception("Given kind '", kind, "' is not a registered connection creator type!");
    }
}

Connection
Connection::announce(const std::string& address) {
    auto [kind, path] = decompose(address);
    if (auto target = getCtab().find(kind); target != getCtab().end()) {
        return Connection(target->second.announce(path));
    } else {
        throw Exception("Given kind '", kind, "' is not a registered connection creator type!");
    }
}

Connection::Connection(int fid) : _fid(fid) { }

ssize_t 
Connection::write(const std::string& msg, size_t count) {
    return ::write(_fid, msg.c_str(), count);
}
ssize_t
Connection::write(const std::string& msg) {
    return write(msg, msg.length());
}

ssize_t
Connection::read(std::string& msg, size_t count) {
    msg.reserve(count);
    return ::read(_fid, msg.data(), count);
}

ssize_t
Connection::write(char* c, size_t count) {
    return ::write(_fid, c, count);
}
ssize_t
Connection::read(char* c, size_t count) {
    return ::read(_fid, c, count);
}

bool
Connection::shutdown(int how) {
    return ::shutdown(_fid, how) == 0;
}

bool
Connection::close() {
    return ::close(_fid) == 0;
}

Connection::operator int() const {
    return _fid;
}

std::map<std::string, Connection::Creator>&
Connection::getCtab() noexcept {
    static std::map<std::string, Connection::Creator> _map;
    return _map;
}

void
Connection::registerCreator(const std::string& name, Connection::Action dial, Connection::Action announce) {
    if (auto result = getCtab().emplace(name, Creator(name, dial, announce)); !result.second) {
        throw Exception(name, " already registered as a creator kind!");
    }
}
Connection::Creator::Creator(const std::string& name, Action dial, Action announce) : _name(name), _dial(dial), _announce(announce) { }

int
Connection::Creator::dial(const std::string& address) {
    return _dial(address);
}

int
Connection::Creator::announce(const std::string& address) {
    return _announce(address);
}

Connection::CreatorRegistrar::CreatorRegistrar(const std::string& name, Action dial, Action announce) {
    Connection::registerCreator(name, dial, announce);
}

// the backslash versions are for gdb on my machine that automatically insert a backslash before the connection type in history
static Connection::CreatorRegistrar unixConnection("unix", dial_unix, announce_unix);
static Connection::CreatorRegistrar unixGdbConnection("\\unix", dial_unix, announce_unix);
static Connection::CreatorRegistrar tcpConnection("tcp", dial_tcp, announce_tcp);
static Connection::CreatorRegistrar tcpGdbConnection("\\tcp", dial_tcp, announce_tcp);
static Connection::CreatorRegistrar debugConnection("debug", 
        [](const auto& address) {
            std::cout << "dial address: " << address << std::endl;
            return -1;
        },
        [](const auto& address) {
            std::cout << "announce address: " << address << std::endl;
            return -1;
        });

} // end namespace jyq
