#pragma once
#include "debug_window.h"
#include "../system_state.h"
#include "profiler.h"
#include <vector>

class Blitter;

struct BankTransition {
    uint8_t old_bank;
    uint8_t new_bank;
    uint32_t transition_count;
};

class RegistersWindow : public DebugWindow {
private:
    SystemState& _system_state;
    CartridgeState& _cartridge_state;
    Blitter& _blitter;
    
    // Bank transition tracking
    uint8_t _last_bank_mask = 0;
    std::vector<BankTransition> _bank_history;
    uint32_t _transition_counter = 0;
    static const size_t MAX_HISTORY = 64;
protected:
    ImVec2 Render();
public:
    RegistersWindow(SystemState& system_state, CartridgeState& cartridge_state, Blitter& blitter, Profiler& profiler): 
        _system_state(system_state), _cartridge_state(cartridge_state), _blitter(blitter) {
        _last_bank_mask = cartridge_state.bank_mask;
    };
};
