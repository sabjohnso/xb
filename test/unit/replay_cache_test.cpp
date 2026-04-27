/// @file
/// Tests for xb::replay_cache — first-seen accepts, duplicates reject,
/// and expired entries are recyclable.

#include <xb/replay_cache.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("replay_cache accepts a first-seen key", "[replay_cache]") {
  xb::replay_cache cache;
  auto now = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  CHECK(cache.record_or_reject("nonce-a", now) == xb::replay_outcome::accepted);
  CHECK(cache.size() == 1);
}

TEST_CASE("replay_cache rejects an immediate replay",
          "[replay_cache][security]") {
  xb::replay_cache cache;
  auto now = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  REQUIRE(cache.record_or_reject("nonce-a", now) ==
          xb::replay_outcome::accepted);
  CHECK(cache.record_or_reject("nonce-a", now) == xb::replay_outcome::replayed);
  CHECK(cache.record_or_reject("nonce-a", now + 30s) ==
        xb::replay_outcome::replayed);
}

TEST_CASE("replay_cache distinguishes different keys", "[replay_cache]") {
  xb::replay_cache cache;
  auto now = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  CHECK(cache.record_or_reject("a", now) == xb::replay_outcome::accepted);
  CHECK(cache.record_or_reject("b", now) == xb::replay_outcome::accepted);
  CHECK(cache.record_or_reject("a", now) == xb::replay_outcome::replayed);
}

TEST_CASE("replay_cache recycles a key after the TTL elapses",
          "[replay_cache]") {
  xb::replay_cache cache{60s};
  auto t0 = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  REQUIRE(cache.record_or_reject("nonce-a", t0) ==
          xb::replay_outcome::accepted);
  // Just after TTL — the original entry must have been evicted, so
  // reusing the same key is allowed.
  auto t1 = t0 + 61s;
  CHECK(cache.record_or_reject("nonce-a", t1) == xb::replay_outcome::accepted);
}

TEST_CASE("replay_cache evicts expired entries on record", "[replay_cache]") {
  xb::replay_cache cache{10s};
  auto t = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  cache.record_or_reject("a", t);
  cache.record_or_reject("b", t);
  cache.record_or_reject("c", t);
  REQUIRE(cache.size() == 3);

  // Inserting after the TTU passes evicts the others.
  cache.record_or_reject("d", t + 60s);
  CHECK(cache.size() == 1);
}

TEST_CASE("replay_cache evict_expired is callable directly", "[replay_cache]") {
  xb::replay_cache cache{10s};
  auto t = std::chrono::sys_seconds{std::chrono::seconds{1000}};
  cache.record_or_reject("a", t);
  cache.record_or_reject("b", t);
  REQUIRE(cache.size() == 2);
  cache.evict_expired(t + 60s);
  CHECK(cache.size() == 0);
}
