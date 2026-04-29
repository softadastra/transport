/**
 *
 *  @file TcpSocket.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_TCP_SOCKET_HPP
#define SOFTADASTRA_TRANSPORT_TCP_SOCKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include <softadastra/core/Core.hpp>

namespace softadastra::transport::platform::os_linux
{
  namespace core_time = softadastra::core::time;

  /**
   * @brief Minimal Linux TCP socket wrapper.
   *
   * TcpSocket is a small RAII wrapper around a blocking IPv4 TCP socket.
   *
   * It is used by the transport module to implement the Linux TCP backend.
   *
   * Current scope:
   * - open / close
   * - bind / listen / accept
   * - connect
   * - send / receive
   * - basic socket options
   *
   * The socket owns its file descriptor and closes it in the destructor.
   */
  class TcpSocket
  {
  public:
    /**
     * @brief Creates an invalid socket.
     */
    TcpSocket() noexcept;

    /**
     * @brief Adopts an existing socket file descriptor.
     *
     * @param fd Existing socket file descriptor.
     */
    explicit TcpSocket(int fd) noexcept;

    /**
     * @brief Closes the socket if it is still open.
     */
    ~TcpSocket();

    TcpSocket(const TcpSocket &) = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    /**
     * @brief Move-constructs a socket.
     *
     * Ownership of the file descriptor is transferred.
     *
     * @param other Source socket.
     */
    TcpSocket(TcpSocket &&other) noexcept;

    /**
     * @brief Move-assigns a socket.
     *
     * Any currently owned descriptor is closed before taking ownership.
     *
     * @param other Source socket.
     * @return This socket.
     */
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    /**
     * @brief Opens a new IPv4 TCP socket.
     *
     * @return true on success.
     */
    [[nodiscard]] bool open();

    /**
     * @brief Binds the socket to host and port.
     *
     * @param host Local bind host.
     * @param port Local bind port.
     * @return true on success.
     */
    [[nodiscard]] bool bind(
        const std::string &host,
        std::uint16_t port);

    /**
     * @brief Puts the socket into listening mode.
     *
     * @param backlog Listen backlog.
     * @return true on success.
     */
    [[nodiscard]] bool listen(int backlog = 16);

    /**
     * @brief Accepts one incoming connection.
     *
     * Returns an invalid TcpSocket on failure.
     *
     * @return Accepted socket.
     */
    [[nodiscard]] TcpSocket accept();

    /**
     * @brief Connects to a remote host and port.
     *
     * @param host Remote host.
     * @param port Remote port.
     * @return true on success.
     */
    [[nodiscard]] bool connect(
        const std::string &host,
        std::uint16_t port);

    /**
     * @brief Sends bytes until size bytes are sent or an error occurs.
     *
     * @param data Source buffer.
     * @param size Number of bytes to send.
     * @return Number of bytes successfully sent.
     */
    [[nodiscard]] std::size_t send_all(
        const void *data,
        std::size_t size);

    /**
     * @brief Reads bytes until size bytes are read or an error occurs.
     *
     * @param data Destination buffer.
     * @param size Number of bytes to read.
     * @return Number of bytes successfully read.
     */
    [[nodiscard]] std::size_t recv_all(
        void *data,
        std::size_t size);

    /**
     * @brief Reads some bytes once.
     *
     * Returns 0 on EOF or failure.
     *
     * @param data Destination buffer.
     * @param size Maximum number of bytes to read.
     * @return Number of bytes read.
     */
    [[nodiscard]] std::size_t recv_some(
        void *data,
        std::size_t size);

    /**
     * @brief Closes the socket.
     */
    void close() noexcept;

    /**
     * @brief Enables or disables SO_REUSEADDR.
     *
     * @param enabled Whether the option should be enabled.
     * @return true on success.
     */
    [[nodiscard]] bool set_reuse_addr(bool enabled);

    /**
     * @brief Enables or disables SO_KEEPALIVE.
     *
     * @param enabled Whether the option should be enabled.
     * @return true on success.
     */
    [[nodiscard]] bool set_keepalive(bool enabled);

    /**
     * @brief Sets receive timeout.
     *
     * @param timeout Timeout duration.
     * @return true on success.
     */
    [[nodiscard]] bool set_recv_timeout(core_time::Duration timeout);

    /**
     * @brief Sets send timeout.
     *
     * @param timeout Timeout duration.
     * @return true on success.
     */
    [[nodiscard]] bool set_send_timeout(core_time::Duration timeout);

    /**
     * @brief Sets receive timeout in milliseconds.
     *
     * @param timeout_ms Timeout in milliseconds.
     * @return true on success.
     */
    [[nodiscard]] bool set_recv_timeout_ms(std::uint64_t timeout_ms)
    {
      return set_recv_timeout(
          core_time::Duration::from_millis(
              static_cast<core_time::Duration::rep>(timeout_ms)));
    }

    /**
     * @brief Sets send timeout in milliseconds.
     *
     * @param timeout_ms Timeout in milliseconds.
     * @return true on success.
     */
    [[nodiscard]] bool set_send_timeout_ms(std::uint64_t timeout_ms)
    {
      return set_send_timeout(
          core_time::Duration::from_millis(
              static_cast<core_time::Duration::rep>(timeout_ms)));
    }

    /**
     * @brief Returns whether the socket owns a valid file descriptor.
     *
     * @return true when valid.
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Backward-compatible valid alias.
     *
     * @return true when valid.
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return is_valid();
    }

    /**
     * @brief Returns the raw file descriptor.
     *
     * @return File descriptor, or -1 if invalid.
     */
    [[nodiscard]] int fd() const noexcept;

  private:
    int fd_{-1};
  };

} // namespace softadastra::transport::platform::os_linux

#endif // SOFTADASTRA_TRANSPORT_TCP_SOCKET_HPP
