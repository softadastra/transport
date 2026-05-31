/**
 *
 *  @file TcpSocket.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Softadastra.
 *  All rights reserved.
 *  https://github.com/softadastra/softadastra
 *
 *  Licensed under the Apache License, Version 2.0.
 *
 *  Softadastra Transport
 *
 */

#include <softadastra/transport/platform/windows/TcpSocket.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>
#include <ws2tcpip.h>

namespace softadastra::transport::platform::os_windows
{
  namespace
  {
    [[nodiscard]] bool make_sockaddr(
        const std::string &host,
        std::uint16_t port,
        ::sockaddr_in &addr) noexcept
    {
      addr = {};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);

      return ::inet_pton(
                 AF_INET,
                 host.c_str(),
                 &addr.sin_addr) == 1;
    }

    [[nodiscard]] DWORD make_timeout_ms(
        core_time::Duration timeout) noexcept
    {
      const auto millis = timeout.millis();

      if (millis <= 0)
      {
        return 0;
      }

      const auto max_value =
          static_cast<core_time::Duration::rep>(
              (std::numeric_limits<DWORD>::max)());

      if (millis > max_value)
      {
        return (std::numeric_limits<DWORD>::max)();
      }

      return static_cast<DWORD>(millis);
    }

    [[nodiscard]] int chunk_size(std::size_t remaining) noexcept
    {
      const auto max_int =
          static_cast<std::size_t>((std::numeric_limits<int>::max)());

      return static_cast<int>(std::min(remaining, max_int));
    }
  } // namespace

  TcpSocket::TcpSocket() noexcept = default;

  TcpSocket::TcpSocket(SOCKET socket) noexcept
      : socket_(socket)
  {
  }

  TcpSocket::~TcpSocket()
  {
    close();
  }

  TcpSocket::TcpSocket(TcpSocket &&other) noexcept
      : socket_(other.socket_)
  {
    other.socket_ = INVALID_SOCKET;
  }

  TcpSocket &TcpSocket::operator=(TcpSocket &&other) noexcept
  {
    if (this != &other)
    {
      close();
      socket_ = other.socket_;
      other.socket_ = INVALID_SOCKET;
    }

    return *this;
  }

  bool TcpSocket::open()
  {
    close();

    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    return socket_ != INVALID_SOCKET;
  }

  bool TcpSocket::bind(
      const std::string &host,
      std::uint16_t port)
  {
    if (!is_valid() && !open())
    {
      return false;
    }

    ::sockaddr_in addr{};

    if (!make_sockaddr(host, port, addr))
    {
      return false;
    }

    return ::bind(
               socket_,
               reinterpret_cast<const ::sockaddr *>(&addr),
               sizeof(addr)) == 0;
  }

  bool TcpSocket::listen(int backlog)
  {
    if (!is_valid())
    {
      return false;
    }

    return ::listen(socket_, backlog) == 0;
  }

  TcpSocket TcpSocket::accept()
  {
    if (!is_valid())
    {
      return TcpSocket{};
    }

    ::sockaddr_in client_addr{};
    int client_len = sizeof(client_addr);

    const SOCKET client_socket =
        ::accept(
            socket_,
            reinterpret_cast<::sockaddr *>(&client_addr),
            &client_len);

    if (client_socket == INVALID_SOCKET)
    {
      return TcpSocket{};
    }

    return TcpSocket{client_socket};
  }

  bool TcpSocket::connect(
      const std::string &host,
      std::uint16_t port)
  {
    if (!is_valid() && !open())
    {
      return false;
    }

    ::sockaddr_in addr{};

    if (!make_sockaddr(host, port, addr))
    {
      return false;
    }

    return ::connect(
               socket_,
               reinterpret_cast<const ::sockaddr *>(&addr),
               sizeof(addr)) == 0;
  }

  std::size_t TcpSocket::send_all(
      const void *data,
      std::size_t size)
  {
    if (!is_valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    const auto *bytes =
        static_cast<const char *>(data);

    std::size_t total_sent = 0;

    while (total_sent < size)
    {
      const int to_send = chunk_size(size - total_sent);

      const int sent =
          ::send(
              socket_,
              bytes + total_sent,
              to_send,
              0);

      if (sent == SOCKET_ERROR)
      {
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

  std::size_t TcpSocket::recv_all(
      void *data,
      std::size_t size)
  {
    if (!is_valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    auto *bytes =
        static_cast<char *>(data);

    std::size_t total_read = 0;

    while (total_read < size)
    {
      const int to_read = chunk_size(size - total_read);

      const int received =
          ::recv(
              socket_,
              bytes + total_read,
              to_read,
              0);

      if (received == SOCKET_ERROR)
      {
        return total_read;
      }

      if (received == 0)
      {
        break;
      }

      total_read += static_cast<std::size_t>(received);
    }

    return total_read;
  }

  std::size_t TcpSocket::recv_some(
      void *data,
      std::size_t size)
  {
    if (!is_valid() || data == nullptr || size == 0)
    {
      return 0;
    }

    const int received =
        ::recv(
            socket_,
            static_cast<char *>(data),
            chunk_size(size),
            0);

    if (received == SOCKET_ERROR || received == 0)
    {
      return 0;
    }

    return static_cast<std::size_t>(received);
  }

  void TcpSocket::close() noexcept
  {
    if (socket_ == INVALID_SOCKET)
    {
      return;
    }

    ::shutdown(socket_, SD_BOTH);
    ::closesocket(socket_);

    socket_ = INVALID_SOCKET;
  }

  bool TcpSocket::set_reuse_addr(bool enabled)
  {
    if (!is_valid())
    {
      return false;
    }

    const BOOL value = enabled ? TRUE : FALSE;

    return ::setsockopt(
               socket_,
               SOL_SOCKET,
               SO_REUSEADDR,
               reinterpret_cast<const char *>(&value),
               sizeof(value)) == 0;
  }

  bool TcpSocket::set_keepalive(bool enabled)
  {
    if (!is_valid())
    {
      return false;
    }

    const BOOL value = enabled ? TRUE : FALSE;

    return ::setsockopt(
               socket_,
               SOL_SOCKET,
               SO_KEEPALIVE,
               reinterpret_cast<const char *>(&value),
               sizeof(value)) == 0;
  }

  bool TcpSocket::set_recv_timeout(core_time::Duration timeout)
  {
    if (!is_valid())
    {
      return false;
    }

    const DWORD value = make_timeout_ms(timeout);

    return ::setsockopt(
               socket_,
               SOL_SOCKET,
               SO_RCVTIMEO,
               reinterpret_cast<const char *>(&value),
               sizeof(value)) == 0;
  }

  bool TcpSocket::set_send_timeout(core_time::Duration timeout)
  {
    if (!is_valid())
    {
      return false;
    }

    const DWORD value = make_timeout_ms(timeout);

    return ::setsockopt(
               socket_,
               SOL_SOCKET,
               SO_SNDTIMEO,
               reinterpret_cast<const char *>(&value),
               sizeof(value)) == 0;
  }

  bool TcpSocket::is_valid() const noexcept
  {
    return socket_ != INVALID_SOCKET;
  }

  SOCKET TcpSocket::native_handle() const noexcept
  {
    return socket_;
  }

} // namespace softadastra::transport::platform::os_windows
