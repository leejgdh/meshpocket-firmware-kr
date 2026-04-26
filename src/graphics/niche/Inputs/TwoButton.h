#ifdef MESHTASTIC_INCLUDE_NICHE_GRAPHICS

/*

Re-usable NicheGraphics input source

Short and Long press for up to two buttons
Interrupt driven

*/

#pragma once

#include "configuration.h"

#include "assert.h"
#include "functional"

#ifdef ARCH_ESP32
#include "esp_sleep.h" // For light-sleep handling
#endif

#include "Observer.h"

namespace NicheGraphics::Inputs
{

class TwoButton : protected concurrency::OSThread
{
  public:
    typedef std::function<void()> Callback;

    static uint8_t getUserButtonPin(); // Resolve the GPIO, considering the various possible source of definition

    static TwoButton *getInstance(); // Create or get the singleton instance
    void start();                    // Start handling button input
    void stop();                     // Stop handling button input (disconnect ISRs for sleep)
    void setWiring(uint8_t whichButton, uint8_t pin, bool internalPullup = false);
    void setTiming(uint8_t whichButton, uint32_t debounceMs, uint32_t longpressMs);
    void setHandlerDown(uint8_t whichButton, Callback onDown);
    void setHandlerUp(uint8_t whichButton, Callback onUp);
    void setHandlerShortPress(uint8_t whichButton, Callback onShortPress);
    void setHandlerLongPress(uint8_t whichButton, Callback onLongPress);

    // Register a double-press handler. Once registered, every short press on
    // this button is delayed by `windowMs` (default 300ms) so the library can
    // wait to see whether a second press follows. If a second press arrives
    // inside the window the double handler fires; otherwise the single
    // short-press handler fires after the window expires. Buttons without a
    // double handler keep the original zero-latency short-press behavior.
    void setHandlerDoublePress(uint8_t whichButton, Callback onDoublePress, uint32_t windowMs = 300);

    // Disconnect and reconnect interrupts for light sleep
#ifdef ARCH_ESP32
    int beforeLightSleep(void *unused);
    int afterLightSleep(esp_sleep_wakeup_cause_t cause);
#endif

  private:
    // Internal state of a specific button
    enum State {
        REST,            // Up, no activity
        IRQ,             // Down detected, not yet handled
        POLLING_UNFIRED, // Down handled, polling for release
        POLLING_FIRED,   // Longpress fired, button still held
        PENDING_DOUBLE,  // Short release seen, waiting to see if a 2nd press arrives
        SECOND_DOWN,     // 2nd press of a double seen; double already fired; waiting for release
    };

    // Contains info about a specific button
    // (Array of this struct below)
    class Button
    {
      public:
        // Per-button config
        uint8_t pin = 0xFF;                 // 0xFF: unset
        bool activeLogic = LOW;             // Active LOW by default. Currently unimplemented.
        uint32_t debounceLength = 50;       // Minimum length for shortpress, in ms
        uint32_t longpressLength = 500;     // How long after button down to fire longpress, in ms
        uint32_t doublepressWindow = 300;   // Window after release in which a 2nd press counts as double, ms
        bool doubleEnabled = false;         // True once a double-press handler is registered
        volatile State state = State::REST; // Internal state
        volatile uint32_t irqAtMillis;      // millis() when button went down
        volatile uint32_t releaseAtMillis;  // millis() of last short release (used while PENDING_DOUBLE)

        // Per-button event callbacks
        static void noop(){};
        std::function<void()> onDown = noop;
        std::function<void()> onUp = noop;
        std::function<void()> onShortPress = noop;
        std::function<void()> onLongPress = noop;
        std::function<void()> onDoublePress = noop;
    };

#ifdef ARCH_ESP32
    // Get notified when lightsleep begins and ends
    CallbackObserver<TwoButton, void *> lsObserver = CallbackObserver<TwoButton, void *>(this, &TwoButton::beforeLightSleep);
    CallbackObserver<TwoButton, esp_sleep_wakeup_cause_t> lsEndObserver =
        CallbackObserver<TwoButton, esp_sleep_wakeup_cause_t>(this, &TwoButton::afterLightSleep);
#endif

    int32_t runOnce() override; // Timer method. Polls for button release

    void startThread(); // Start polling for release
    void stopThread();  // Stop polling for release

    static void isrPrimary();   // Detect start of press
    static void isrSecondary(); // Detect start of press (optional aux button)

    TwoButton(); // Constructor made private: force use of Button::instance()

    // Info about both buttons
    Button buttons[2];
};

}; // namespace NicheGraphics::Inputs

#endif