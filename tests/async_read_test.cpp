/**
 *
 *  @file async_read_test.cpp
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <vix/async/core/task.hpp>
#include <vix/async/core/cancel.hpp>
#include <vix/async/net/tcp.hpp>

#include <softadastra/transport/utils/AsyncRead.hpp>

namespace async_core = vix::async::core;
namespace async_net = vix::async::net;
namespace transport_utils = softadastra::transport::utils;

namespace
{
  class FakeTcpStream final : public async_net::tcp_stream
  {
  public:
    explicit FakeTcpStream(
        std::vector<std::uint8_t> data,
        std::size_t chunk_size)
        : data_(std::move(data)),
          chunk_size_(chunk_size)
    {
    }

    async_core::task<void> async_connect(
        const async_net::tcp_endpoint &,
        async_core::cancel_token = {}) override
    {
      open_ = true;
      co_return;
    }

    async_core::task<std::size_t> async_read(
        std::span<std::byte> buffer,
        async_core::cancel_token token = {}) override
    {
      if (token.is_cancelled())
      {
        throw std::system_error(
            async_core::cancelled_ec());
      }

      if (!open_)
      {
        co_return 0;
      }

      if (offset_ >= data_.size())
      {
        co_return 0;
      }

      const std::size_t remaining =
          data_.size() - offset_;

      const std::size_t wanted =
          buffer.size();

      const std::size_t chunk =
          chunk_size_ == 0 ? wanted : chunk_size_;

      std::size_t count = remaining < wanted ? remaining : wanted;
      count = count < chunk ? count : chunk;

      std::memcpy(
          buffer.data(),
          data_.data() + offset_,
          count);

      offset_ += count;

      co_return count;
    }

    async_core::task<std::size_t> async_write(
        std::span<const std::byte>,
        async_core::cancel_token = {}) override
    {
      co_return 0;
    }

    void close() noexcept override
    {
      open_ = false;
    }

    bool is_open() const noexcept override
    {
      return open_;
    }

  private:
    std::vector<std::uint8_t> data_{};
    std::size_t chunk_size_{1};
    std::size_t offset_{0};
    bool open_{true};
  };

  async_core::task<void> test_read_exactly_reads_full_buffer()
  {
    std::vector<std::uint8_t> data{
        1, 2, 3, 4, 5, 6};

    FakeTcpStream stream{
        data,
        2};

    std::vector<std::uint8_t> output(6);

    const bool ok =
        co_await transport_utils::read_exactly(
            stream,
            output);

    assert(ok);
    assert(output == data);

    co_return;
  }

  async_core::task<void> test_read_exactly_handles_partial_chunks()
  {
    std::vector<std::uint8_t> data{
        10, 20, 30, 40, 50};

    FakeTcpStream stream{
        data,
        1};

    std::vector<std::uint8_t> output(5);

    const bool ok =
        co_await transport_utils::read_exactly(
            stream,
            output);

    assert(ok);
    assert(output == data);

    co_return;
  }

  async_core::task<void> test_read_exactly_returns_false_on_eof()
  {
    std::vector<std::uint8_t> data{
        1, 2, 3};

    FakeTcpStream stream{
        data,
        2};

    std::vector<std::uint8_t> output(6);

    const bool ok =
        co_await transport_utils::read_exactly(
            stream,
            output);

    assert(!ok);

    co_return;
  }

  async_core::task<void> test_read_exactly_zero_size_buffer()
  {
    std::vector<std::uint8_t> data{
        1, 2, 3};

    FakeTcpStream stream{
        data,
        1};

    std::vector<std::uint8_t> output{};

    const bool ok =
        co_await transport_utils::read_exactly(
            stream,
            output);

    assert(ok);
    assert(output.empty());

    co_return;
  }

  async_core::task<void> test_read_exactly_cancelled_token()
  {
    std::vector<std::uint8_t> data{
        1, 2, 3, 4};

    FakeTcpStream stream{
        data,
        2};

    std::vector<std::uint8_t> output(4);

    async_core::cancel_source source;
    source.request_cancel();

    bool thrown = false;

    try
    {
      [[maybe_unused]] const bool ok =
          co_await transport_utils::read_exactly(
              stream,
              output,
              source.token());
    }
    catch (const std::system_error &error)
    {
      thrown =
          error.code() == async_core::cancelled_ec();
    }

    assert(thrown);

    co_return;
  }

  async_core::task<void> run_tests()
  {
    co_await test_read_exactly_reads_full_buffer();
    co_await test_read_exactly_handles_partial_chunks();
    co_await test_read_exactly_returns_false_on_eof();
    co_await test_read_exactly_zero_size_buffer();
    co_await test_read_exactly_cancelled_token();

    co_return;
  }

} // namespace

int main()
{
  auto tests = run_tests();

  while (tests.handle() && !tests.handle().done())
  {
    tests.handle().resume();
  }

  return 0;
}
