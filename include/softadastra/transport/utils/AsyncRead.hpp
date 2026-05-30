/**
 *
 *  @file AsyncRead.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_ASYNC_READ_HPP
#define SOFTADASTRA_TRANSPORT_ASYNC_READ_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>

namespace softadastra::transport::utils
{
  namespace async_core = vix::async::core;
  namespace async_net = vix::async::net;

  /**
   * @brief Reads exactly the requested number of bytes from a TCP stream.
   *
   * TCP is a byte stream. A single async_read() call may return fewer bytes
   * than requested. This helper keeps reading until the full buffer is filled,
   * the stream closes, or an async error is thrown by the underlying runtime.
   *
   * @param stream TCP stream.
   * @param buffer Destination byte buffer.
   * @param token Optional cancellation token.
   * @return true when the full buffer was read, false on clean EOF.
   */
  [[nodiscard]] inline async_core::task<bool>
  read_exactly(
      async_net::tcp_stream &stream,
      std::span<std::byte> buffer,
      async_core::cancel_token token = {})
  {
    std::size_t offset = 0;

    while (offset < buffer.size())
    {
      const std::size_t read =
          co_await stream.async_read(
              buffer.subspan(offset),
              token);

      if (read == 0)
      {
        co_return false;
      }

      offset += read;
    }

    co_return true;
  }

  /**
   * @brief Reads exactly buffer.size() bytes into a uint8_t span.
   *
   * @param stream TCP stream.
   * @param buffer Destination uint8_t buffer.
   * @param token Optional cancellation token.
   * @return true when the full buffer was read, false on clean EOF.
   */
  [[nodiscard]] inline async_core::task<bool>
  read_exactly(
      async_net::tcp_stream &stream,
      std::span<std::uint8_t> buffer,
      async_core::cancel_token token = {})
  {
    co_return co_await read_exactly(
        stream,
        std::as_writable_bytes(buffer),
        std::move(token));
  }

  /**
   * @brief Reads exactly buffer.size() bytes into a uint8_t vector.
   *
   * The vector must already have the expected size.
   *
   * @param stream TCP stream.
   * @param buffer Destination vector.
   * @param token Optional cancellation token.
   * @return true when the full buffer was read, false on clean EOF.
   */
  [[nodiscard]] inline async_core::task<bool>
  read_exactly(
      async_net::tcp_stream &stream,
      std::vector<std::uint8_t> &buffer,
      async_core::cancel_token token = {})
  {
    co_return co_await read_exactly(
        stream,
        std::span<std::uint8_t>{buffer.data(), buffer.size()},
        std::move(token));
  }

} // namespace softadastra::transport::utils

#endif // SOFTADASTRA_TRANSPORT_ASYNC_READ_HPP
