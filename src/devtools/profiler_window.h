
#include "debug_window.h"
#include "profiler.h"
#include <map>
#include <algorithm>

struct FunctionStats {
    std::string name;
    uint64_t totalCycles;
    uint32_t callCount;
    uint32_t minCycles;
    uint32_t maxCycles;
};

class ProfilerWindow : public DebugWindow {
private:
    Profiler& _profiler;
    ImVec2 Render();
    float max_scale = 2.0f;
    bool profilerVis[PROFILER_ENTRIES] = {0};
    bool profilerSeen[PROFILER_ENTRIES] = {0};
    void recurse_tree_nodes(Profiler::DeepProfileCallNode* node, uint64_t totalCycles, uint64_t startTime);
    void aggregate_function_stats(Profiler::DeepProfileCallNode* node, std::map<std::string, FunctionStats>& stats);
    std::vector<FunctionStats> _cachedStats;
    Profiler::DeepProfileCallNode* _lastCachedRoot = nullptr;
public:
    ProfilerWindow(Profiler& profiler): _profiler(profiler) {};
};
