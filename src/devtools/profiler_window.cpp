#include "../imgui/imgui.h"
#include "profiler_window.h"
#include "implot.h"
#include "../audio_coprocessor.h"
#include "../tinyfd/tinyfiledialogs.h"
#include <fstream>

static float prof_R[8] = {1,    1, 1, 0, 0, 0.5f, 0.5f, 1};
static float prof_G[8] = {0, 0.5f, 1, 1, 0,    0, 0.5f, 1};
static float prof_B[8] = {0,    0, 0, 0, 1, 0.5f, 0.5f, 1};

void ProfilerWindow::aggregate_function_stats(Profiler::DeepProfileCallNode* node, std::map<std::string, FunctionStats>& stats) {
    if (node == nullptr) return;
    
    if (!node->name.empty() && node->name != "root") {
        auto& entry = stats[node->name];
        if (entry.name.empty()) {
            entry.name = node->name;
            entry.totalCycles = 0;
            entry.callCount = 0;
            entry.minCycles = UINT32_MAX;
            entry.maxCycles = 0;
        }
        entry.totalCycles += node->duration;
        entry.callCount++;
        if (node->duration < entry.minCycles) entry.minCycles = node->duration;
        if (node->duration > entry.maxCycles) entry.maxCycles = node->duration;
    }
    
    for (auto& child : node->children) {
        if (child != nullptr) {
            aggregate_function_stats(child, stats);
        }
    }
}

void ProfilerWindow::recurse_tree_nodes(Profiler::DeepProfileCallNode* node, uint64_t totalCycles, uint64_t startTime) {
    ImGui::PushID(node);
    bool opened = ImGui::TreeNode(node, "%s : %d", node->name.c_str(), node->duration);
    ImGui::SameLine(ImGui::GetWindowWidth()-288);
    if(ImGui::Button(">")) {
        _profiler.deepProfileZoomFocus = node;
    }
    ImGui::SameLine(ImGui::GetWindowWidth()-256);
    ImGui::Text("%.1f%%", (100.0f * (float) node->duration) / ((float) totalCycles));
    ImGui::SameLine(ImGui::GetWindowWidth()-216);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = (200.0f * (float) node->duration) / ((float) totalCycles);
    float offset = (200.0f * (float) (node->offset - startTime)) / ((float) totalCycles);
    if(width < 1) width = 1;
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(p.x,p.y), ImVec2(p.x+200, p.y+16),
        IM_COL32( 64, 64,128,255));
    ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(p.x+offset,p.y), ImVec2(p.x+width+offset, p.y+16),
        IM_COL32(255,255,0,255));
    //ImGui::GetForegroundDrawList()->AddText(ImVec2(p.x-32,p.y),IM_COL32_WHITE, "%%");
    ImGui::NewLine();
    if(opened) {
        for(auto& child : node->children) {
            recurse_tree_nodes(child, totalCycles, startTime);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

ImVec2 ProfilerWindow::Render() {

    ImVec2 sizeOut = {0, 0};

    ImGui::Begin("Profiler", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    ImGui::BeginTabBar("profilertabs", 0);
    if(ImGui::BeginTabItem("Stopwatches")) {
        ImGui::Text("FPS: %d", _profiler.fps);
        ImGui::Text("ACP: %d", AudioCoprocessor::singleton_acp_state->last_irq_cycles);
        ImGui::Text("Graph max:");
        ImGui::SameLine();
        ImGui::InputFloat("##", &max_scale, 0, 0, "%.2f");

        ImGui::Checkbox("Per frame", &_profiler.measure_by_frameflip);
        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
            ImGui::SetTooltip("Aggregate per buffer swap instead of per vsync");

        if(ImPlot::BeginPlot("Timer history")) {
            ImPlot::SetupAxes("", "");
            ImPlot::SetupAxesLimits(0,256, 0, max_scale, ImPlotCond_Always);
            for(int i = 0; i < PROFILER_ENTRIES; ++i) {
                if(profilerSeen[i] && profilerVis[i]) {
                    ImPlot::PlotLine<float>("", _profiler.profilingHistory[i], PROFILER_HISTORY, 1, 0, ImPlotSpec(
                        ImPlotProp_LineColor, ImVec4(prof_R[i % 7], prof_G[i % 7], prof_B[i % 7], 1.0f),
                        ImPlotProp_Offset, _profiler.history_num
                    ));
                }
            }
            ImPlot::PlotLine<float>("Blitter", _profiler.blitter_history, PROFILER_HISTORY, 1, 0, ImPlotSpec(
                        ImPlotProp_LineColor, ImVec4(prof_R[7], prof_G[7], prof_B[7], 1.0f),
                        ImPlotProp_Offset, _profiler.history_num
            ));
            ImPlot::EndPlot();
        }
        


        ImGui::BeginChild("Scrolling");
        ImGui::Text("Blit Pixels/Frame: %lu px", _profiler.last_blitter_activity);
        for(int i = 0; i < PROFILER_ENTRIES; ++i) {
            if(_profiler.profilingLastSample[i] != 0) {
                if(!profilerSeen[i]) {
                    profilerVis[i] = true;
                }
                profilerSeen[i] = true;
            }
            if(profilerSeen[i]) {
                ImGui::PushID(i);
                ImGui::Checkbox("##", &profilerVis[i]);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(prof_R[i % 8], prof_G[i % 8], prof_B[i % 8], 1.0f), 
                "Timer %02d: %u / %u", i, _profiler.profilingLastSample[i], _profiler.profilingLastSampleCount[i]);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if(ImGui::BeginTabItem("Deep Profile")) {
        if(_profiler.lastDeepProfileRoot == nullptr) {
            ImGui::Text("Run a deep profile to see results here");
            // Clear cache when no profile exists
            if (_lastCachedRoot != nullptr) {
                _lastCachedRoot = nullptr;
                _cachedStats.clear();
            }
        } else {
            // Rebuild cached stats if the profile root changed
            if (_lastCachedRoot != _profiler.lastDeepProfileRoot) {
                _lastCachedRoot = _profiler.lastDeepProfileRoot;
                std::map<std::string, FunctionStats> statsMap;
                aggregate_function_stats(_profiler.lastDeepProfileRoot, statsMap);
                _cachedStats.clear();
                for (auto& pair : statsMap) {
                    _cachedStats.push_back(pair.second);
                }
                // Sort by total cycles descending
                std::sort(_cachedStats.begin(), _cachedStats.end(), 
                    [](const FunctionStats& a, const FunctionStats& b) {
                        return a.totalCycles > b.totalCycles;
                    });
            }

            ImGui::BeginTabBar("deepprofiletabs", 0);
            
            // Tree View Tab
            if(ImGui::BeginTabItem("Call Tree")) {
                if(_profiler.deepProfileZoomFocus == nullptr) {
                    ImGui::Text("Total: %d cycles", _profiler.lastDeepProfileRoot->duration);
                    for(auto& node : _profiler.lastDeepProfileRoot->children) {
                        recurse_tree_nodes(node, _profiler.lastDeepProfileRoot->duration, 0);
                    }
                } else {
                    ImGui::Text("%s: %d cycles", _profiler.deepProfileZoomFocus->name.c_str(), _profiler.deepProfileZoomFocus->duration);
                    ImGui::SameLine();
                    if(ImGui::Button("<##unzoom")) {
                        _profiler.deepProfileZoomFocus = nullptr;
                    } else {
                        for(auto& node : _profiler.deepProfileZoomFocus->children) {
                            recurse_tree_nodes(node, _profiler.deepProfileZoomFocus->duration, _profiler.deepProfileZoomFocus->offset);
                        }
                    }
                }
                ImGui::EndTabItem();
            }
            
            // Hotspots Tab (sorted by total time)
            if(ImGui::BeginTabItem("Hotspots")) {
                ImGui::Text("Functions sorted by total time (most expensive first)");
                ImGui::Text("Total frame: %d cycles", _profiler.lastDeepProfileRoot->duration);
                
                // Export to CSV button
                if (ImGui::Button("Export to CSV")) {
                    const char* filterPatterns[1] = {"*.csv"};
                    const char* savePath = tinyfd_saveFileDialog(
                        "Export Hotspots to CSV",
                        "hotspots.csv",
                        1,
                        filterPatterns,
                        "CSV Files"
                    );
                    
                    if (savePath != nullptr) {
                        std::ofstream csvFile(savePath);
                        if (csvFile.is_open()) {
                            uint64_t totalCycles = _profiler.lastDeepProfileRoot->duration;
                            csvFile << "Function,Total Cycles,Percentage,Call Count,Average,Min,Max\n";
                            for (const auto& stat : _cachedStats) {
                                float pct = (100.0f * (float)stat.totalCycles) / (float)totalCycles;
                                uint64_t avg = stat.callCount > 0 ? stat.totalCycles / stat.callCount : 0;
                                
                                csvFile << stat.name << ","
                                       << stat.totalCycles << ","
                                       << pct << ","
                                       << stat.callCount << ","
                                       << avg << ","
                                       << stat.minCycles << ","
                                       << stat.maxCycles << "\n";
                            }
                            
                            csvFile.close();
                        }
                    }
                }
                
                ImGui::Separator();
                
                if (ImGui::BeginTable("HotspotsTable", 6, 
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | 
                    ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable,
                    ImVec2(0, 600))) {
                    
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                    ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableSetupColumn("Min/Max", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableHeadersRow();
                    
                    uint64_t totalCycles = _profiler.lastDeepProfileRoot->duration;
                    
                    for (const auto& stat : _cachedStats) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", stat.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", (unsigned long long)stat.totalCycles);
                        ImGui::TableNextColumn();
                        float pct = (100.0f * (float)stat.totalCycles) / (float)totalCycles;
                        ImGui::Text("%.1f%%", pct);
                        ImGui::TableNextColumn();
                        ImGui::Text("%u", stat.callCount);
                        ImGui::TableNextColumn();
                        uint64_t avg = stat.callCount > 0 ? stat.totalCycles / stat.callCount : 0;
                        ImGui::Text("%llu", (unsigned long long)avg);
                        ImGui::TableNextColumn();
                        ImGui::Text("%u/%u", stat.minCycles, stat.maxCycles);
                    }
                    
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    ImGui::SetWindowPos({0, 0});
    ImGui::SetWindowSize({800,800});
    sizeOut = ImVec2(800, 800);
    ImGui::End();
    return sizeOut;
}
