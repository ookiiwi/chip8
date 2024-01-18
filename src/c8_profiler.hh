#ifndef C8_PROFILER_HH
#define C8_PROFILER_HH

#include "c8_helper.h"
#include "chip8.h"
#include <vector>
#include <deque>

struct _C8_OpcodeSnapshot {
    _C8_OpcodeSnapshot(const chip8_t &context);

    std::vector<BYTE>       registers;
    WORD                    pc;
    WORD                    sp;
    WORD                    addressI;
    BYTE                    dt;
    BYTE                    st;
};

class C8_Profiler {
public:
    C8_Profiler(const chip8_t &context) : m_context(context), m_pause(false), m_step(false) {}

    void render(const int *opcode);
    bool shouldStep();
private:
    const chip8_t &m_context;
    bool m_pause;
    bool m_step;
    std::deque<WORD> m_opHistory;
    std::deque<_C8_OpcodeSnapshot> m_opSnapshots;
};

#endif