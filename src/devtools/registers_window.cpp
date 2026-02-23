#include "registers_window.h"
#include "../blitter.h"

ImVec2 RegistersWindow::Render() {
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::Begin("System Registers");

    uint8_t current_bank = _cartridge_state.bank_mask & 0x7F;
    uint8_t last_bank = _last_bank_mask & 0x7F;
    if (current_bank != last_bank) {
        BankTransition t;
        t.old_bank = last_bank;
        t.new_bank = current_bank;
        t.transition_count = ++_transition_counter;
        _bank_history.push_back(t);
        if (_bank_history.size() > MAX_HISTORY) {
            _bank_history.erase(_bank_history.begin());
        }
        _last_bank_mask = _cartridge_state.bank_mask;
    }

    if (ImGui::CollapsingHeader("ROM Bank (Cartridge)", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t bank_mask = _cartridge_state.bank_mask;
        bool ram_mode = !(bank_mask & 0x80);
        ImGui::Text("Current Bank: $%02X (%d)", bank_mask & 0x7F, bank_mask & 0x7F);
        ImGui::Text("Mode: %s", ram_mode ? "RAM" : "ROM");
        ImGui::Text("Shifter: $%02X", _cartridge_state.bank_shifter);
        ImGui::Separator();
        
        ImGui::Text("Transitions: %zu (total: %u)", _bank_history.size(), _transition_counter);
        if (ImGui::Button("Clear History")) {
            _bank_history.clear();
            _transition_counter = 0;
        }
        
        if (!_bank_history.empty() && ImGui::BeginTable("BankHistoryTable", 3, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, 
            ImVec2(0, 100))) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("To", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();
            
            for (int i = (int)_bank_history.size() - 1; i >= 0; --i) {
                const auto& t = _bank_history[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%u", t.transition_count);
                ImGui::TableNextColumn();
                ImGui::Text("$%02X", t.old_bank);
                ImGui::TableNextColumn();
                ImGui::Text("$%02X", t.new_bank);
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Banking Register ($2005)", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t banking = _system_state.banking;
        ImGui::Text("Raw Value: $%02X (%d)", banking, banking);
        ImGui::Separator();
        
        uint8_t gram_bank = banking & 0x07;
        ImGui::Text("GRAM Bank:     %d", gram_bank);
        ImGui::SameLine();
        ImGui::TextDisabled("(bits 0-2)");
        
        bool vram_page = (banking & 0x08) != 0;
        ImGui::Text("VRAM Page:     %d", vram_page ? 1 : 0);
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 3)");
        
        bool wrap_x = (banking & 0x10) != 0;
        ImGui::Text("X Wrap:        %s", wrap_x ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 4)");
        
        bool wrap_y = (banking & 0x20) != 0;
        ImGui::Text("Y Wrap:        %s", wrap_y ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 5)");
        
        uint8_t ram_bank = (banking >> 6) & 0x03;
        ImGui::Text("RAM Bank:      %d", ram_bank);
        ImGui::SameLine();
        ImGui::TextDisabled("(bits 6-7)");
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("DMA Control Register ($2007)", ImGuiTreeNodeFlags_DefaultOpen)) {
        uint8_t dma = _system_state.dma_control;
        ImGui::Text("Raw Value: $%02X (%d)", dma, dma);
        ImGui::Separator();
        
        bool copy_enable = (dma & 0x01) != 0;
        ImGui::Text("Copy Enable:   %s", copy_enable ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 0)");
        
        bool vid_out_page = (dma & 0x02) != 0;
        ImGui::Text("Display Page:  %d", vid_out_page ? 1 : 0);
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 1)");
        
        bool vsync_nmi = (dma & 0x04) != 0;
        ImGui::Text("VSync NMI:     %s", vsync_nmi ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 2)");
        
        bool color_fill = (dma & 0x08) != 0;
        ImGui::Text("Color Fill:    %s", color_fill ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 3)");
        
        bool gcarry = (dma & 0x10) != 0;
        ImGui::Text("G Carry:       %s", gcarry ? "16px" : "256px");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 4)");
        
        bool cpu_to_vram = (dma & 0x20) != 0;
        ImGui::Text("CPU Target:    %s", cpu_to_vram ? "VRAM" : "GRAM");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 5)");
        
        bool copy_irq = (dma & 0x40) != 0;
        ImGui::Text("Copy IRQ:      %s", copy_irq ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 6)");
        
        bool transparency = (dma & 0x80) != 0;
        ImGui::Text("Transparency:  %s", transparency ? "ON" : "OFF");
        ImGui::SameLine();
        ImGui::TextDisabled("(bit 7)");
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Blitter Registers ($4000-$4007)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Status: %s", _blitter.IsRunning() ? "RUNNING" : "IDLE");
        ImGui::Separator();

        if (ImGui::BeginTable("BlitterTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Hex", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("VX ($4000)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_VX));
            ImGui::TableNextColumn();
            ImGui::Text("Dest X: %d", _blitter.GetParam(Blitter::PARAM_VX));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("VY ($4001)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_VY));
            ImGui::TableNextColumn();
            ImGui::Text("Dest Y: %d", _blitter.GetParam(Blitter::PARAM_VY));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("GX ($4002)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_GX));
            ImGui::TableNextColumn();
            ImGui::Text("Source X: %d", _blitter.GetParam(Blitter::PARAM_GX));

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("GY ($4003)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_GY));
            ImGui::TableNextColumn();
            ImGui::Text("Source Y: %d", _blitter.GetParam(Blitter::PARAM_GY));

            uint8_t width_raw = _blitter.GetParam(Blitter::PARAM_WIDTH);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("WIDTH ($4004)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", width_raw);
            ImGui::TableNextColumn();
            ImGui::Text("W: %d, Dir: %s", width_raw & 0x7F, (width_raw & 0x80) ? "Rev" : "Fwd");

            uint8_t height_raw = _blitter.GetParam(Blitter::PARAM_HEIGHT);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("HEIGHT ($4005)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", height_raw);
            ImGui::TableNextColumn();
            ImGui::Text("H: %d, Dir: %s", height_raw & 0x7F, (height_raw & 0x80) ? "Rev" : "Fwd");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("TRIGGER ($4006)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_TRIGGER));
            ImGui::TableNextColumn();
            ImGui::Text("(write-only)");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("COLOR ($4007)");
            ImGui::TableNextColumn();
            ImGui::Text("$%02X", _blitter.GetParam(Blitter::PARAM_COLOR));
            ImGui::TableNextColumn();
            ImGui::Text("Fill color: %d", _blitter.GetParam(Blitter::PARAM_COLOR));

            ImGui::EndTable();
        }
    }

    ImGui::End();
    return ImGui::GetWindowSize();
}
