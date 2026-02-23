#pragma once
#include "debug_window.h"
#include "profiler.h"
#include "memory_map.h"
#include "../mos6502/mos6502.h"

class CallstackWindow : public DebugWindow {
private:
    Profiler& _profiler;
    MemoryMap*& _memorymap;
    ImVec2 Render();
public:
    bool enabled = false;
    CallstackWindow(Profiler& profiler, MemoryMap*& memorymap, mos6502*& cpu): 
        _profiler(profiler), _memorymap(memorymap) {};
};
