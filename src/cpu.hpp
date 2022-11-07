#pragma once

#include <cstring>

#include "stack.hpp"
#include "types.hpp"

typedef void (*Instruction)(struct Cpu *cpu, u16 op);

const u8 font_data[] = {
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct Cpu {
  u16 address_i = 0;
  u8 delay_timer;
  u8 game_memory[0xfff];
  bool input[0x10] = {};
  Instruction *instructions;
  u16 program_counter = 0x200;
  u8 registers[16];
  bool requires_repaint = false;
  bool running = true;
  u32 screen_data[32][64];
  u8 sound_timer;
  Stack<u16, 32> stack; // NOTE(steven): Not sure how big the stack should be

  Cpu(Instruction *instructions) : instructions(instructions) {
    memcpy(&this->game_memory[0x50], font_data, sizeof(font_data));
  }

  void simulate() {
    u16 op = this->get_current_opcode();
    this->step_program_counter();

    Instruction instruction = instructions[(op & 0xf000) >> 12];
    assert(instruction != nullptr);

    if (instruction != nullptr) {
      instruction(this, op);
    }

    const bool screen_data_changed = (op & 0xf000) == 0xd000 || op == 0x00e0;
    if (screen_data_changed) {
      this->requires_repaint = true;
    }
  }

  void step_program_counter() {
    this->program_counter = (this->program_counter + 2) & 0x0fff;
  }

  void update_timers() {
    if (this->delay_timer > 0) {
      this->delay_timer--;
    }

    if (this->sound_timer > 0) {
      this->sound_timer--;
    }
  }

protected:
  u16 get_current_opcode() const {
    const u8 *from = &this->game_memory[this->program_counter];
    return (*from << 8) | *(from + 1);
  }
};