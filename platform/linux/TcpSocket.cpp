/*
 * TcpSocket.cpp
 */

#include <softadastra/transport/platform/linux/TcpSocket.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace softadastra::transport::platform::os_linux
{
  namespace
  {
    ::sockaddr_in make_sockaddr(const std::string &host, std::uint16_t port)
    {
      ::sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);

      if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
      {
        throw std::runtime_error("TcpSocket: invalid IPv4 address: " + host);
      }

      return addr;
    }

    ::timeval make_timeval(std::uint64_t timeout_ms)
    {
      ::timeval tv{};
      tv.tv_sec = static_cast<decltype(tv.tv_sec)>(timeout_ms / 1000ULL);
      tv.tv_usec = static_cast<decltype(tv.tv_usec)>((timeout_ms % 1000ULL) * 1000ULL);
      return tv;
    }
  } // namespace

  TcpSocket::TcpSocket() noexcept
      : fd_(-1)
  {
  }

  TcpSocket::TcpSocket(int fd) noexcept
      : fd_(fd)
  {
  }

  TcpSocket::~TcpSocket()
  {
    close();
  }

  TcpSocket::TcpSocket(TcpSocket &&other) noexcept
      : fd_(other.fd_)
  {
    other.fd_ = -1;
  }

  TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept
  {
    if (this != &other)
    {
      close();
      fd_ = other.fd_;
      other.fd_ = -1;
    }

    return *this;
  }

  bool TcpSocket::open()
  {
    close();

    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    return fd_ >= 0;
  }

  bool TcpSocket::bind(const std::string &host, std::uint16_t port)
  {
    if (!valid() && !open())
    {
      return false;
    }

    const auto addr = make_sockaddr(host, port);

    return ::bind(fd_,
                  reinterpret_cast<const ::sockaddr *>(&addr),
                  sizeof(addr)) == 0;
  }

  bool TcpSocket::listen(int backlog)
  {
    if (!valid())
    {
      return false;
    }

    return ::listen(fd_, backlog) == 0;
  }

  TcpSocket TcpSocket::accept()
  {
    if (!valid())
    {
      return TcpSocket{};
    }

    ::sockaddr_in client_addr{};
    ::socklen_t client_len = sizeof(client_addr);

    const int client_fd = ::accept(
        fd_,
        reinterpret_cast<::sockaddr *>(&client_addr),
        &client_len);

    if (client_fd < 0)
    {
      return TcpSocket{};
    }

    return TcpSocket{client_fd};
  }

  bool TcpSocket::connect(const std::string &host, std::uint16_t port)
  {
    if (!valid() && !open())
    {
      return false;
    }

    const auto addr = make_sockaddr(host, port);

    return ::connect(fd_,
                     reinterpret_cast<const ::sockaddr *>(&addr),
                     sizeof(addr)) == 0;
  }

  std::size_t TcpSocket::send_all(const void *data, std::size_t size)
  {
    if (!valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    const auto *bytes = static_cast<const std::uint8_t *>(data);
    std::size_t total_sent = 0;

    while (total_sent < size)
    {
      const ssize_t sent = ::send(
          fd_,
          bytes + total_sent,
          size - total_sent,
          0);

      if (sent < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }

        return total_sent;
      }

      if (sent == 0)
      {
        break;
      }

      total_sent += static_cast<std::size_t>(sent);
    }

    return total_sent;
  }

  std::size_t TcpSocket::recv_all(void *data, std::size_t size)
  {
    if (!valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    auto *bytes = static_cast<std::uint8_t *>(data);
    std::size_t total_read = 0;

    while (total_read < size)
    {
      const ssize_t n = ::recv(
          fd_,
          bytes + total_read,
          size - total_read,
          0);

      if (n < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }

        return total_read;
      }

      if (n == 0)
      {
        break;
      }

      total_read += static_cast<std::size_t>(n);
    }

    return total_read;
  }

  std::size_t TcpSocket::recv_some(void *data, std::size_t size)
  {
    if (!valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    for (;;)
    {
      const ssize_t n = ::recv(fd_, data, size, 0);

      if (n < 0)
      {
        if (errno == EINTR)
        {
          continue;
        }

        return 0;
      }

      if (n == 0)
      {
        return 0;
      }

      return static_cast<std::size_t>(n);
    }
  }

  void TcpSocket::close() noexcept
  {
    if (fd_ >= 0)
    {
      ::shutdown(fd_, SHUT_RDWR);
      ::close(fd_);
      fd_ = -1;
    }
  }

  bool TcpSocket::set_reuse_addr(bool enabled)
  {
    if (!valid())
    {
      return false;
    }

    const int value = enabled ? 1 : 0;

    return ::setsockopt(
               fd_,
               SOL_SOCKET,
               SO_REUSEADDR,
               &value,
               sizeof(value)) == 0;
  }

  bool TcpSocket::set_keepalive(bool enabled)
  {
    if (!valid())
    {
      return false;
    }

    const int value = enabled ? 1 : 0;

    return ::setsockopt(
               fd_,
               SOL_SOCKET,
               SO_KEEPALIVE,
               &value,
               sizeof(value)) == 0;
  }

  bool TcpSocket::set_recv_timeout_ms(std::uint64_t timeout_ms)
  {
    if (!valid())
    {
      return false;
    }

    const auto tv = make_timeval(timeout_ms);

    return ::setsockopt(
               fd_,
               SOL_SOCKET,
               SO_RCVTIMEO,
               &tv,
               sizeof(tv)) == 0;
  }

  bool TcpSocket::set_send_timeout_ms(std::uint64_t timeout_ms)
  {
    if (!valid())
    {
      return false;
    }

    const auto tv = make_timeval(timeout_ms);

    return ::setsockopt(
               fd_,
               SOL_SOCKET,
               SO_SNDTIMEO,
               &tv,
               sizeof(tv)) == 0;
  }

  bool TcpSocket::valid() const noexcept
  {
    return fd_ >= 0;
  }

  int TcpSocket::fd() const noexcept
  {
    return fd_;
  }

} // namespace softadastra::transport::platform::linux
