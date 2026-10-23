#pragma once

#include <stdint.h>

// Companion WiFi retry rules (our addition). No Arduino code here, so the host tests in
// test/test_ours_wifi_retry can drive it. WifiLink.cpp turns the actions into WiFi calls.
//   - One try lasts up to ATTEMPT_MS. After a failed try the WiFi radio is off until the next one.
//   - Waits between tries: 10 s, 30 s, 1 min, 2 min, then 5 min.
//   - Gives up after GIVE_UP_MS without a stable link, or after AUTH_FAIL_LIMIT wrong-password
//     failures. WiFi then stays off until the next turn-on from the page or the next boot.
//   - A link is stable after STABLE_MS. Only a drop from a stable link starts the rules fresh, so
//     weak WiFi that joins and drops again still reaches the give-up time.
class WifiRetry {
public:
  enum State : uint8_t { OFF, CONNECTING, WAITING, CONNECTED, GAVE_UP };
  enum Reason : uint8_t { NO_REASON, NO_NETWORK, WRONG_PASSWORD };
  enum Action : uint8_t { DO_NOTHING, START_TRY, STOP_RADIO };

  // enum constants: usable by value or by reference without a separate definition (C++11)
  enum : uint32_t {
    ATTEMPT_MS = 15000UL,
    STABLE_MS = 5UL * 60UL * 1000UL,
    GIVE_UP_MS = 15UL * 60UL * 1000UL,
    AUTH_FAIL_LIMIT = 3
  };

  static uint32_t waitAfter(uint8_t failures) {   // failures counts from 1
    static const uint32_t waits[] = { 10000UL, 30000UL, 60000UL, 120000UL, 300000UL };
    uint8_t i = failures == 0 ? 0 : failures - 1;
    return waits[i < 4 ? i : 4];
  }

  Action start(uint32_t now) {
    _state = CONNECTING;
    _since = now;
    _try_at = now;
    _failures = 0;
    _auth_fails = 0;
    _reason = NO_REASON;
    _stable = false;
    return START_TRY;
  }

  Action stop() {
    _state = OFF;
    return STOP_RADIO;
  }

  void onConnected(uint32_t now) {
    if (_state != CONNECTING) return;   // a late event after a timed-out try changes nothing
    _state = CONNECTED;
    _connected_at = now;
    _reason = NO_REASON;
  }

  Action onDisconnected(uint32_t now, bool wrong_password) {
    if (_state == CONNECTED) {
      if (_stable) {   // a good link dropped: start the rules fresh
        _since = now;
        _failures = 0;
        _auth_fails = 0;
        _stable = false;
      }
      return fail(now, NO_NETWORK);
    }
    if (_state == CONNECTING) return fail(now, wrong_password ? WRONG_PASSWORD : NO_NETWORK);
    return DO_NOTHING;   // radio is off or WiFi gave up: late events change nothing
  }

  Action poll(uint32_t now) {
    switch (_state) {
      case CONNECTED:
        if (!_stable && now - _connected_at >= STABLE_MS) {
          _stable = true;
          _failures = 0;
          _auth_fails = 0;
        }
        return DO_NOTHING;
      case CONNECTING:
        if (now - _try_at >= ATTEMPT_MS) return fail(now, NO_NETWORK);
        return DO_NOTHING;
      case WAITING:
        if ((int32_t)(now - _next_at) >= 0) {
          _state = CONNECTING;
          _try_at = now;
          return START_TRY;
        }
        return DO_NOTHING;
      default:
        return DO_NOTHING;
    }
  }

  State state() const { return _state; }
  Reason reason() const { return _reason; }
  uint32_t secondsToNextTry(uint32_t now) const {
    return _state == WAITING && (int32_t)(_next_at - now) > 0 ? (_next_at - now + 999) / 1000 : 0;
  }

private:
  Action fail(uint32_t now, Reason why) {
    _reason = why;
    if (_failures < 255) _failures++;
    if (why == WRONG_PASSWORD) _auth_fails++;
    if (_auth_fails >= AUTH_FAIL_LIMIT || now - _since >= GIVE_UP_MS) {
      _state = GAVE_UP;
      return STOP_RADIO;
    }
    _state = WAITING;
    _next_at = now + waitAfter(_failures);
    return STOP_RADIO;
  }

  State _state = OFF;
  Reason _reason = NO_REASON;
  uint8_t _failures = 0;
  uint8_t _auth_fails = 0;
  bool _stable = false;
  uint32_t _since = 0;
  uint32_t _try_at = 0;
  uint32_t _next_at = 0;
  uint32_t _connected_at = 0;
};
