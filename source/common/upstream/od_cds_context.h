#pragma once

#include <limits>

#include "envoy/common/optref.h"
#include "envoy/stream_info/filter_state.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/common/basic_resource_impl.h"

namespace Envoy {
namespace Http {
class OnDemandAsyncClientProvider;
}
namespace Upstream {

// On-demand configuration and pending-request budget shared across workers.
// Each primary or mirror discovery configuration has its own instance.
class OdCdsContext {
public:
  static constexpr uint64_t DefaultMaxPendingRequests = std::numeric_limits<uint64_t>::max();

  // Counts one request waiting for cluster discovery. Destroying this object
  // decrements the count.
  //
  // Keeps the counter alive if the configuration is replaced while the request
  // is waiting. Existing requests decrement their original counter; the new
  // configuration starts with a separate counter at zero.
  class PendingRequestReservation {
  public:
    ~PendingRequestReservation() { pending_requests_->dec(); }
    PendingRequestReservation(const PendingRequestReservation&) = delete;
    PendingRequestReservation& operator=(const PendingRequestReservation&) = delete;

  private:
    friend class OdCdsContext;
    explicit PendingRequestReservation(std::shared_ptr<BasicResourceLimitImpl> pending_requests)
        : pending_requests_(std::move(pending_requests)) {
      pending_requests_->inc();
    }
    const std::shared_ptr<BasicResourceLimitImpl> pending_requests_;
  };
  using PendingRequestReservationPtr = std::unique_ptr<PendingRequestReservation>;

  OdCdsContext(OdCdsApiHandlePtr api, std::chrono::milliseconds timeout,
               uint64_t max_pending_requests = DefaultMaxPendingRequests,
               std::shared_ptr<Http::OnDemandAsyncClientProvider> client_provider = nullptr)
      : api_(std::move(api)), timeout_(timeout), client_provider_(std::move(client_provider)),
        pending_requests_(std::make_shared<BasicResourceLimitImpl>(max_pending_requests)) {}
  PendingRequestReservationPtr tryReservePendingRequestSlot() const {
    if (!pending_requests_->canCreate()) {
      return nullptr;
    }
    return PendingRequestReservationPtr(new PendingRequestReservation(pending_requests_));
  }

  OdCdsApiHandle& api() const { return *api_; }
  std::chrono::milliseconds timeout() const { return timeout_; }
  OptRef<Http::OnDemandAsyncClientProvider> clientProvider() const {
    return makeOptRefFromPtr(client_provider_.get());
  }

private:
  const OdCdsApiHandlePtr api_;
  const std::chrono::milliseconds timeout_;
  const std::shared_ptr<Http::OnDemandAsyncClientProvider> client_provider_;
  const std::shared_ptr<BasicResourceLimitImpl> pending_requests_;
};
using OdCdsContextSharedPtr = std::shared_ptr<const OdCdsContext>;

// Per-request wrapper that passes the mirror discovery context from the on-demand
// filter to the router.
class OdCdsFilterState : public StreamInfo::FilterState::Object {
public:
  explicit OdCdsFilterState(OdCdsContextSharedPtr mirror_discovery_context)
      : mirror_discovery_context_(std::move(mirror_discovery_context)) {}
  static constexpr absl::string_view key() { return "envoy.upstream.od_cds_context"; }
  OdCdsContextSharedPtr mirrorDiscoveryContextSharedPtr() const {
    return mirror_discovery_context_;
  }

private:
  const OdCdsContextSharedPtr mirror_discovery_context_;
};

} // namespace Upstream
} // namespace Envoy
