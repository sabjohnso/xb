#pragma once

/// @file
/// Bounded-TTL set used to detect replay of nonces, MessageIDs, or any
/// other one-shot identifier.  No background thread or global state —
/// expired entries are evicted lazily during @ref record_or_reject so
/// the cache fits xb's "no hidden state" style.

#include <chrono>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace xb {

  /// Default time a token is remembered before its identifier may be
  /// reused.  Matches the WS-Security UsernameToken Profile guidance
  /// to remember nonces "for a reasonable period" — long enough to
  /// frustrate replay across the typical message-skew window, short
  /// enough that long-running services do not accumulate state.
  inline constexpr std::chrono::seconds default_replay_ttl{600};

  /// Outcome of a record-or-reject query.
  enum class replay_outcome {
    accepted, ///< First time this key has been seen within the window.
    replayed, ///< Same key was recorded earlier and has not yet expired.
  };

  /// TTL set keyed by string identifier (e.g. base64 nonce, WSA
  /// MessageID URI).  Each entry is timestamped at insertion; on each
  /// query, expired entries are pruned before the duplicate check.
  ///
  /// Not thread-safe — wrap in a mutex if shared between request
  /// threads.  Capacity is unbounded by design; the lazy eviction keeps
  /// memory proportional to traffic times TTL.
  class replay_cache {
  public:
    /// Construct with a custom TTL.  Defaults to @ref default_replay_ttl.
    explicit replay_cache(std::chrono::seconds ttl = default_replay_ttl)
        : ttl_(ttl) {}

    /// Try to record @p key seen at @p now.  If the key was recorded
    /// within the previous TTL window, the entry is unchanged and the
    /// return value is @ref replay_outcome::replayed.  Otherwise the
    /// entry is recorded (or refreshed) and the return value is
    /// @ref replay_outcome::accepted.
    replay_outcome
    record_or_reject(const std::string& key,
                     std::chrono::sys_seconds now =
                         std::chrono::time_point_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now())) {
      evict_expired(now);
      auto [it, inserted] = entries_.try_emplace(key, now);
      if (inserted) return replay_outcome::accepted;
      if (it->second + ttl_ < now) {
        // Stale entry that survived eviction (shouldn't happen given
        // the eviction pass above, but be defensive).
        it->second = now;
        return replay_outcome::accepted;
      }
      return replay_outcome::replayed;
    }

    /// Number of keys currently in the cache (for tests/diagnostics).
    std::size_t
    size() const noexcept {
      return entries_.size();
    }

    /// Evict every entry whose timestamp is older than @c now - ttl.
    /// Called automatically by @ref record_or_reject; exposed for tests
    /// and for callers that want to drain memory between bursts.
    void
    evict_expired(std::chrono::sys_seconds now) {
      const auto cutoff = now - ttl_;
      for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second < cutoff) {
          it = entries_.erase(it);
        } else {
          ++it;
        }
      }
    }

  private:
    std::chrono::seconds ttl_;
    std::unordered_map<std::string, std::chrono::sys_seconds> entries_;
  };

} // namespace xb
