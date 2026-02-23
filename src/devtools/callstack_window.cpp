#include "callstack_window.h"
#include "source_map.h"
#include <sstream>
#include <iomanip>

ImVec2 CallstackWindow::Render() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("Call Stack");

    enabled = _profiler.callstack_tracking_enabled;
    if(ImGui::Checkbox("Enable Tracking", &enabled)) {
        _profiler.callstack_tracking_enabled = enabled;
        if(!enabled) {
            _profiler.timeline.clear();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Tracking callstack has a performance impact.\nDisable when not needed.");
    }

    ImGui::Separator();

    if (!enabled) {
        ImGui::TextDisabled("Callstack tracking is disabled");
        ImGui::End();
        return ImGui::GetWindowSize();
    }

    ImGui::Text("Timeline entries: %zu", _profiler.timeline.size());

    if (_profiler.timeline.empty()) {
        ImGui::TextDisabled("No call stack data available");
        ImGui::End();
        return ImGui::GetWindowSize();
    }

    std::vector<std::pair<uint16_t, uint16_t>> callstack; // (origin, destination)
    
    for (const auto& event : _profiler.timeline) {
        if (event.type == Profiler::ProfileEventType::CALL) {
            callstack.push_back(std::make_pair(event.origin, event.destination));
        } else if (event.type == Profiler::ProfileEventType::RETURN) {
            if (!callstack.empty()) {
                callstack.pop_back();
            }
        }
    }

    ImGui::Text("Stack Depth: %zu", callstack.size());
    ImGui::Separator();

    if (ImGui::BeginTable("CallstackTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int depth = 0;
        for (const auto& frame : callstack) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", depth);
            ImGui::TableNextColumn();
            ImGui::Text("$%04X", frame.second);
            ImGui::TableNextColumn();
            if (_memorymap != nullptr) {
                Symbol sym;
                if (_memorymap->FindAddress(frame.second, &sym)) {
                    ImGui::Text("%s", sym.name.c_str());
                } else {
                    ImGui::TextDisabled("Unknown");
                }
            } else {
                ImGui::TextDisabled("No symbol map");
            }
            ImGui::TableNextColumn();
            if (SourceMap::singleton != nullptr) {
                SourceMapSearchResult result = SourceMap::singleton->Search(frame.first, 0);
                if (result.found) {
                    std::stringstream ss;
                    ss << result.line->file << ":" << result.line->line;
                    ImGui::Text("%s", ss.str().c_str());
                } else {
                    ImGui::TextDisabled("Unknown");
                }
            } else {
                ImGui::TextDisabled("No source map");
            }

            depth++;
        }

        ImGui::EndTable();
    }

    ImGui::End();
    return ImGui::GetWindowSize();
}
