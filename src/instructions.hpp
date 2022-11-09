#pragma once

#include <cassert>

#include "cpu.hpp"
#include "types.hpp"

void x0(Cpu *cpu, u16 op) {
  switch (op & 0xff) {
    // CLS
    case 0xe0: {
      memset(cpu->screen_data, 0, sizeof(cpu->screen_data));
    } break;

    // RET
    case 0xee: {
      cpu->program_counter = cpu->stack.pop();
    } break;

    default: assert(false);
  }
}

// JP addr
void x1NNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->program_counter = nnn;
}

// CALL addr
void x2NNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->stack.push(cpu->program_counter);
  cpu->program_counter = nnn;
}

// SE Vx, nn
void x3XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  if (cpu->registers[x] == nn) {
    cpu->step_program_counter();
  }
}

// SNE Vx, nn
void x4XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  if (cpu->registers[x] != nn) {
    cpu->step_program_counter();
  }
}

// SE Vx, Vy
void x5XY0(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 y = (u8)(op >> 4) & 0x0f;
  if (cpu->registers[x] == cpu->registers[y]) {
    cpu->step_program_counter();
  }
}

// LD Vx, nn
void x6XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  cpu->registers[x] = nn;
}

// ADD Vx, nn
void x7XNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = (u8)op;
  cpu->registers[x] += nn;
}

void x8(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 xn = cpu->registers[x];

  const u8 y = (u8)(op >> 4) & 0x0f;
  const u8 yn = cpu->registers[y];

  switch (op & 0x0f) {
    // LD Vx, Vy
    case 0x00: {
      cpu->registers[x] = yn;
    } break;

    // OR Vx, Vy
    case 0x01: {
      cpu->registers[x] |= yn;
    } break;

    // AND Vx, Vy
    case 0x02: {
      cpu->registers[x] &= yn;
    } break;

    // XOR Vx, Vy
    case 0x03: {
      cpu->registers[x] ^= yn;
    } break;

    // ADD Vx, Vy
    case 0x04: {
      const u16 result = xn + yn;
      cpu->registers[0x0f] = result > 0xff ? 1 : 0;
      cpu->registers[x] = result;
    } break;

    // SUB Vx, Vy
    case 0x05: {
      cpu->registers[0x0f] = xn >= yn ? 1 : 0;
      cpu->registers[x] -= yn;
    } break;

    // SHR Vx {, Vy}
    case 0x06: {
      // cpu->registers[0x0f] = yn & 0x01;
      // cpu->registers[x] = yn >> 1;
      cpu->registers[0x0f] = xn & 0x01;
      cpu->registers[x] = xn >> 1;
    } break;

    // SUBN Vx, Vy
    case 0x07: {
      cpu->registers[0x0f] = yn >= xn ? 1 : 0;
      cpu->registers[x] = yn - xn;
    } break;

    // SHL Vx {, Vy}
    case 0x0e: {
      // cpu->registers[0x0f] = (yn >> 7) & 0x01;
      // cpu->registers[x] = yn << 1;
      cpu->registers[0x0f] = (xn >> 7) & 0x01;
      cpu->registers[x] = xn << 1;
    } break;

    default: assert(false);
  }
}

// SNE Vx, Vy
void x9XY0(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 y = (u8)(op >> 4) & 0x0f;
  if (cpu->registers[x] != cpu->registers[y]) {
    cpu->step_program_counter();
  }
}

// LD I, addr
void xANNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->address_i = nnn;
}

// JP V0, addr
void xBNNN(Cpu *cpu, u16 op) {
  const u16 nnn = op & 0x0fff;
  cpu->program_counter = nnn + cpu->registers[0];
}

// RND Vx, nn
void xCXNN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 nn = op & 0xff;
  cpu->registers[x] = (rand() % 0xff) & nn;
}

// DRW Vx, Vy, n
void xDXYN(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 xn = cpu->registers[x] & 63;

  const u8 y = (u8)(op >> 4) & 0x0f;
  const u8 yn = cpu->registers[y] & 31;

  const u8 n = (u8)op & 0x0f;

  cpu->registers[0x0f] = 0;

  for (u8 row = 0; row < n; row++) {
    const u8 draw_line = cpu->game_memory[cpu->address_i + row];
    for (u8 column = 0; column < 8; column++) {
      const u8 x_coord = xn + column;
      const u8 y_coord = yn + row;

      if (
        x_coord < 0 || x_coord > 63 || 
        y_coord < 0 || y_coord > 31
      ) {
        continue;
      }

      if ((draw_line & (0x80 >> column)) != 0) {
        if (cpu->screen_data[y_coord][x_coord] != 0) {
          cpu->registers[0x0f] = 1;
        }
        cpu->screen_data[y_coord][x_coord] ^= 0x0000ffff;
      }
    }
  }
}

void xE(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  const u8 xn = cpu->registers[x];

  switch (op & 0xff) {
    // SKNP Vx
    case 0xa1: {
      if (!cpu->input[xn]) {
        cpu->step_program_counter();
      }
    } break;

    // SKP Vx
    case 0x9e: {
      if (cpu->input[xn]) {
        cpu->step_program_counter();
      }
    } break;

    default: assert(false);
  }
}

void xF(Cpu *cpu, u16 op) {
  const u8 x = (u8)(op >> 8) & 0x0f;
  switch (op & 0xff) {
    // LD Vx, DT
    case 0x07: {
      cpu->registers[x] = cpu->delay_timer;
    } break;

    // LD Vx, K
    case 0x0a: {
      for (u8 key = 0; key < 0x10; key++) {
        if (cpu->input[key]) {
          cpu->registers[x] = key;
          return;
        }
      }
      cpu->program_counter = (cpu->program_counter - 2) & 0x0fff;
    } break;

    // LD DT, Vx
    case 0x15: {
      cpu->delay_timer = cpu->registers[x];
    } break;

    // LD ST, Vx
    case 0x18: {
      cpu->sound_timer = cpu->registers[x];
    } break;

    // ADD I, Vx
    case 0x1e: {
      cpu->address_i = (cpu->address_i + cpu->registers[x]) & 0x0fff;
    } break;

    // LD F, Vx
    case 0x29: {
      cpu->address_i = 0x50 + 5 * cpu->registers[x];
    } break;

    // LD B, Vx
    case 0x33: {
      const u8 xn = cpu->registers[x];
      cpu->game_memory[cpu->address_i] = (f32)xn / 100;
      cpu->game_memory[cpu->address_i + 1] = (f32)(xn % 100) / 10;
      cpu->game_memory[cpu->address_i + 2] = xn % 10;
    } break;

    // LD [I], Vx
    case 0x55: {
      for (u8 i = 0; i <= x; i++) {
        cpu->game_memory[cpu->address_i + i] = cpu->registers[i];
      }
      cpu->address_i = (cpu->address_i + x + 1) & 0x0fff;
    } break;

    // LD Vx, [I]
    case 0x65: {
      for (u8 i = 0; i <= x; i++) {
        cpu->registers[i] = cpu->game_memory[cpu->address_i + i];
      }
      cpu->address_i = (cpu->address_i + x + 1) & 0x0fff;
    } break;

    default: assert(false);
  }
}