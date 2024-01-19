#include "c8_profiler.hh"
#include "c8_disassembler.h"
#include "c8_def.h"
#include "imgui.h"

#define MAX_OPCODE_SNAPSHOTS_COUNT 512

_C8_OpcodeSnapshot::_C8_OpcodeSnapshot(const chip8_t &context) 
    :   registers(context.registers, &context.registers[15]),
        pc(context.pc-2),
        sp(context.sp),
        addressI(context.addressI),
        dt(context.delayTimer),
        st(context.soundTimer) {}

void C8_Profiler::render(const int *opcode) {
    static int selected_index = -1;
    _C8_OpcodeSnapshot context = (m_opSnapshots.size() != 0 && selected_index != -1 ? m_opSnapshots.at(selected_index) : _C8_OpcodeSnapshot(m_context));

    ImVec2 dummy(0, 10);
    ImGui::SetNextWindowPos(ImVec2(0, VIEWPORT_HEIGHT));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, PROFILER_WINDOW_HEIGHT_EXTENT));
    ImGui::SetNextWindowCollapsed(false);
    ImGui::Begin("Profiler");


    if (!m_pause) {
        if (m_opSnapshots.size() > MAX_OPCODE_SNAPSHOTS_COUNT){
            m_opHistory.pop_front();
            m_opSnapshots.pop_front();
        }

        if (opcode != nullptr) {
            m_opHistory.push_back(*opcode);
            m_opSnapshots.push_back(_C8_OpcodeSnapshot(m_context));
        }
    }

    // debugger
    if (ImGui::Button(m_pause ? "Continue" : "Pause")) {
        m_pause = !m_pause;
    }

    ImGui::SameLine();

    if (m_pause && ImGui::Button("Step")) {
        m_step = true;
    }

    ImGui::Dummy(dummy);

    // display registers
    ImGui::BeginGroup();
    ImGui::Text("Registers");
    ImGui::Indent(16.f);

    for(int i = 0; i < REGISTER_COUNT; ++i) {
        ImGui::Text("V%X", i);
        ImGui::SameLine();
        ImGui::Text("%04X", context.registers[i]);
    }

    ImGui::Text("Delay timer %04X", context.dt);
    ImGui::Text("Sound timer %04X", context.st);
    //ImGui::Text("Key pressed %X",   context.keyPressed);

    ImGui::Unindent(16.f);
    ImGui::EndGroup();
    
    ImGui::SameLine();

    // pointers
    ImGui::BeginGroup();
    ImGui::Text("Pointers");

    ImGui::Indent(16.f);
    ImGui::Text("PC %04X", context.pc);
    ImGui::Text("SP %04X", context.sp);
    ImGui::Text("I  %04X", context.addressI);

    ImGui::Unindent(16.f);
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginChild("Opcodes");
    ImGui::Text("Execution");
    ImGui::Indent(16.f);

    size_t i = 0;
    for(const auto e : m_opHistory) {
        char text[64];
        const char *s = c8_disassemble(e);

        if (s != NULL) {
            snprintf(text, sizeof text / sizeof text[0], "$%04X %04X\t%s##%zu", m_opSnapshots.at(i).pc, e, s, i);

            if (ImGui::Selectable(text, selected_index == i)) {
                selected_index = i;
            }

            free((void*)s);
        }

        ++i;
    }

    if (!m_pause)
        ImGui::SetScrollY(ImGui::GetScrollMaxY());

    ImGui::Unindent(16.f);
    ImGui::EndChild();

    ImGui::End();
}

bool C8_Profiler::shouldStep() {
    bool tmp = m_step;
    m_step = false;
    return !m_pause || tmp;
}
