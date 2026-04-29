/**
 *
 *  @file TransportContext.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_CONTEXT_HPP
#define SOFTADASTRA_TRANSPORT_CONTEXT_HPP

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>

namespace softadastra::transport::core
{
  namespace sync_engine = softadastra::sync::engine;
  namespace core_errors = softadastra::core::errors;
  namespace core_types = softadastra::core::types;

  /**
   * @brief Shared runtime dependencies for the transport module.
   *
   * TransportContext groups the objects required by the transport layer.
   *
   * It provides access to:
   * - the active TransportConfig
   * - the SyncEngine used as sync source and sink
   *
   * The context does not own these objects.
   * The caller must ensure they outlive the transport components using them.
   */
  struct TransportContext
  {
    /**
     * @brief Transport configuration.
     */
    const TransportConfig *config{nullptr};

    /**
     * @brief Sync engine used as the message source and sink bridge.
     */
    sync_engine::SyncEngine *sync{nullptr};

    /**
     * @brief Result type returned by checked accessors.
     */
    template <typename T>
    using Result = core_types::Result<T, core_errors::Error>;

    /**
     * @brief Creates an empty invalid context.
     */
    TransportContext() = default;

    /**
     * @brief Creates a transport context from dependencies.
     *
     * @param transport_config Transport configuration reference.
     * @param sync_engine Sync engine reference.
     */
    TransportContext(
        const TransportConfig &transport_config,
        sync_engine::SyncEngine &sync_engine) noexcept
        : config(&transport_config),
          sync(&sync_engine)
    {
    }

    /**
     * @brief Returns true if the context is usable.
     *
     * @return true when config and sync engine are present and valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return config != nullptr &&
             sync != nullptr &&
             config->is_valid() &&
             sync->is_valid();
    }

    /**
     * @brief Returns the transport configuration pointer.
     *
     * @return TransportConfig pointer, or nullptr.
     */
    [[nodiscard]] const TransportConfig *
    config_ptr() const noexcept
    {
      return config;
    }

    /**
     * @brief Returns the sync engine pointer.
     *
     * @return SyncEngine pointer, or nullptr.
     */
    [[nodiscard]] sync_engine::SyncEngine *
    sync_ptr() const noexcept
    {
      return sync;
    }

    /**
     * @brief Returns the transport configuration as a Result.
     *
     * @return TransportConfig pointer on success, Error on failure.
     */
    [[nodiscard]] Result<const TransportConfig *>
    config_checked() const
    {
      if (config == nullptr)
      {
        return Result<const TransportConfig *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "transport context config is null"));
      }

      if (!config->is_valid())
      {
        return Result<const TransportConfig *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "transport config is invalid"));
      }

      return Result<const TransportConfig *>::ok(config);
    }

    /**
     * @brief Returns the sync engine as a Result.
     *
     * @return SyncEngine pointer on success, Error on failure.
     */
    [[nodiscard]] Result<sync_engine::SyncEngine *>
    sync_checked() const
    {
      if (sync == nullptr)
      {
        return Result<sync_engine::SyncEngine *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "transport context sync engine is null"));
      }

      if (!sync->is_valid())
      {
        return Result<sync_engine::SyncEngine *>::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "sync engine is invalid"));
      }

      return Result<sync_engine::SyncEngine *>::ok(sync);
    }

    /**
     * @brief Returns true if a configuration is present.
     *
     * @return true when config is not null.
     */
    [[nodiscard]] bool has_config() const noexcept
    {
      return config != nullptr;
    }

    /**
     * @brief Returns true if a sync engine is present.
     *
     * @return true when sync is not null.
     */
    [[nodiscard]] bool has_sync() const noexcept
    {
      return sync != nullptr;
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_CONTEXT_HPP
