#pragma once

#include <stdint.h>

// Latest request per outlet. Absolute ON/OFF writes are safe to retry, but an
// older target must never survive a newer request. Time comparisons wrap with
// millis(); the command lifetime is deliberately much shorter than its wrap.
struct ApcDelivery {
  enum Phase : uint8_t { Idle, Queued, Awaiting, Confirmed, Unconfirmed };
  static constexpr uint32_t kRetryMs = 6000;
  static constexpr uint32_t kLifetimeMs = 45000;
  static constexpr uint8_t kMaxAttempts = 3;

  Phase phase = Idle;
  uint32_t id = 0;
  uint32_t queuedAt = 0;
  uint32_t sentAt = 0;
  uint8_t target = 0;
  uint8_t attempts = 0;

  void queue(uint32_t newId, uint8_t value, uint32_t now) {
    id = newId;
    target = value;
    queuedAt = now;
    sentAt = 0;
    attempts = 0;
    phase = Queued;
  }

  bool pending() const { return phase == Queued || phase == Awaiting; }

  // Returns true only when a write needs another attempt and a fresh session.
  bool tick(uint32_t now) {
    if (!pending()) return false;
    if (now - queuedAt >= kLifetimeMs) {
      phase = Unconfirmed;
      return false;
    }
    if (phase != Awaiting || now - sentAt < kRetryMs) return false;
    if (attempts >= kMaxAttempts) {
      phase = Unconfirmed;
      return false;
    }
    phase = Queued;
    return true;
  }

  void sent(uint32_t now) {
    if (phase != Queued) return;
    ++attempts;
    sentAt = now;
    phase = Awaiting;
  }

  bool confirm(uint32_t requestId, uint8_t value) {
    if (id != requestId || target != value || attempts == 0 ||
        phase == Queued || phase == Idle) return false;
    phase = Confirmed;
    return true;
  }

  const char* status() const {
    switch (phase) {
      case Queued: return "queued";
      case Awaiting: return "awaiting_confirmation";
      case Confirmed: return "confirmed";
      case Unconfirmed: return "unconfirmed";
      default: return "idle";
    }
  }
};
