/*
 * TcpSocket.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_TCP_SOCKET_HPP
#define SOFTADASTRA_TRANSPORT_TCP_SOCKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace softadastra::transport::platform::os_linux
{
  /**
   * @brief Minimal Linux TCP socket wrapper
   *
   * This class provides a small RAII wrapper around a blocking
   * IPv4 TCP socket for the transport module.
   *
   * Current scope:
   * - open / close
   * - bind / listen / accept
   * - connect
   * - send / receive
   * - basic socket options
   */
  class TcpSocket
  {
  public:
    /**
     * @brief Create an invalid socket
     */
    TcpSocket() noexcept;

    /**
     * @brief Adopt an existing socket file descriptor
     */
    explicit TcpSocket(int fd) noexcept;

    /**
     * @brief Destructor closes the socket if still open
     */
    ~TcpSocket();

    TcpSocket(const TcpSocket &) = delete;
    TcpSocket &operator=(const TcpSocket &) = delete;

    TcpSocket(TcpSocket &&other) noexcept;
    TcpSocket &operator=(TcpSocket &&other) noexcept;

    /**
     * @brief Open a new IPv4 TCP socket
     */
    bool open();

    /**
     * @brief Bind the socket to host:port
     */
    bool bind(const std::string &host, std::uint16_t port);

    /**
     * @brief Put the socket into listening mode
     */
    bool listen(int backlog = 16);

    /**
     * @brief Accept one incoming connection
     *
     * Returns an invalid TcpSocket on failure.
     */
    TcpSocket accept();

    /**
     * @brief Connect to a remote host:port
     */
    bool connect(const std::string &host, std::uint16_t port);

    /**
     * @brief Send exactly up to size bytes
     *
     * Returns the number of bytes successfully sent.
     */
    std::size_t send_all(const void *data, std::size_t size);

    /**
     * @brief Read exactly up to size bytes
     *
     * Returns the number of bytes successfully read.
     */
    std::size_t recv_all(void *data, std::size_t size);

    /**
     * @brief Read some bytes once
     *
     * Returns 0 on EOF or failure.
     */
    std::size_t recv_some(void *data, std::size_t size);

    /**
     * @brief Close the socket
     */
    void close() noexcept;

    /**
     * @brief Enable or disable SO_REUSEADDR
     */
    bool set_reuse_addr(bool enabled);

    /**
     * @brief Enable or disable SO_KEEPALIVE
     */
    bool set_keepalive(bool enabled);

    /**
     * @brief Set receive timeout in milliseconds
     */
    bool set_recv_timeout_ms(std::uint64_t timeout_ms);

    /**
     * @brief Set send timeout in milliseconds
     */
    bool set_send_timeout_ms(std::uint64_t timeout_ms);

    /**
     * @brief Return whether the socket is valid
     */
    bool valid() const noexcept;

    /**
     * @brief Return the raw file descriptor
     */
    int fd() const noexcept;

  private:
    int fd_;
  };

} // namespace softadastra::transport::platform::linux

#endif
