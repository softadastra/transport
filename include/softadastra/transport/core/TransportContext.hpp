/*
 * TransportContext.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_CONTEXT_HPP
#define SOFTADASTRA_TRANSPORT_CONTEXT_HPP

#include <stdexcept>

#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>

namespace softadastra::transport::core
{
  namespace sync_engine = softadastra::sync::engine;

  /**
   * @brief Shared runtime dependencies for the transport module
   *
   * The transport layer uses SyncEngine as its integration point
   * for receiving remote operations and acknowledgements.
   */
  struct TransportContext
  {
    /**
     * Transport configuration
     */
    const TransportConfig *config{nullptr};

    /**
     * Sync engine used as the message sink/source bridge
     */
    sync_engine::SyncEngine *sync{nullptr};

    /**
     * @brief Check whether the context is usable
     */
    bool valid() const noexcept
    {
      return config != nullptr &&
             sync != nullptr &&
             config->valid();
    }

    /**
     * @brief Return transport configuration
     */
    const TransportConfig &config_ref() const
    {
      if (config == nullptr)
      {
        throw std::runtime_error(
            "TransportContext: config is null");
      }

      return *config;
    }

    /**
     * @brief Return sync engine reference
     */
    sync_engine::SyncEngine &sync_ref() const
    {
      if (sync == nullptr)
      {
        throw std::runtime_error(
            "TransportContext: sync is null");
      }

      return *sync;
    }
  };

} // namespace softadastra::transport::core

#endif
