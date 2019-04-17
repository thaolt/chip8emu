#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "chip8emu.h"

static uint8_t chip8_fontset[80] =
{
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

chip8emu *chip8emu_new(void)
{
    chip8emu* emu = malloc(sizeof (chip8emu));

    emu->pc     = 0x200;  /* Program counter starts at 0x200 */
    emu->opcode = 0;      /* Reset current opcode */
    emu->I      = 0;      /* Reset index register */
    emu->sp     = 0;      /* Reset stack pointer */

    memset(&(emu->gfx), 0, 64 * 32);      /* Clear display */
    memset(&(emu->key), 0, 16);           /* Clear key presses */
    memset(&(emu->stack), 0, 16);         /* Clear stack */
    memset(&(emu->V), 0, 16);             /* Clear registers V0-VF */
    memset(&(emu->memory), 0, 4096);      /* Clear memory */

    /* Load fontset */
    for(int i = 0; i < 80; ++i)
        emu->memory[i] = chip8_fontset[i];

    /* Reset timers */
    emu->delay_timer = 0;
    emu->sound_timer = 0;

    emu->draw_flag = false;
    emu->beep = 0;

    return emu;
}

void chip8emu_load_rom(chip8emu *emu, uint8_t *code, int code_size)
{
    for(int i = 0; i < code_size; ++i)
      emu->memory[i + 512] = code[i];
}

void chip8emu_free(chip8emu *emu)
{
    free(emu);
}

void chip8emu_exec_cycle(chip8emu *emu)
{
    emu->opcode = (uint16_t) (emu->memory[emu->pc] << 8 | emu->memory[emu->pc + 1]);

    switch(emu->opcode & 0xF000) {
    case 0x0000:
        switch (emu->opcode) {
        case 0x00E0: /* clear screen */
            memset(emu->gfx, 0, 64*32);
            emu->pc += 2;
            emu->draw_flag = true;
            break;

        case 0x00EE: /* subroutine return */
            emu->pc = emu->stack[--emu->sp & 0xF] + 2;
            break;

        default: /* 0NNN: call program at NNN address */
            break;
        }
        break;
    case 0x1000: /* absolute jump */
        emu->pc = emu->opcode & 0xFFF;
        break;
    case 0x2000: /* 2NNN: call subroutine */
        emu->stack[emu->sp] = emu->pc;
        ++emu->sp;
        emu->pc = emu->opcode & 0x0FFF;
        break;
    case 0x3000: /* 3XNN: Skips the next instruction if VX equals NN */
        if (emu->V[(emu->opcode & 0x0F00) >> 8] == (emu->opcode & 0x00FF)) {
            emu->pc += 4;
        } else {
            emu->pc += 2;
        }
        break;
    case 0x4000: /* 4XNN: Skips the next instruction if VX doesn't equal NN */
        if (emu->V[(emu->opcode & 0x0F00) >> 8] != (emu->opcode & 0x00FF)) {
            emu->pc += 4;
        } else {
            emu->pc += 2;
        }
        break;
    case 0x5000: /* 5XY0: Skips the next instruction if VX equals VY */
        if (emu->V[(emu->opcode & 0x0F00) >> 8] == emu->V[(emu->opcode & 0x00F0) >> 4]) {
            emu->pc += 4;
        } else {
            emu->pc += 2;
        }
        break;
    case 0x6000: /* 6XNN: Sets VX to NN */
        emu->V[(emu->opcode & 0x0F00) >> 8] = (emu->opcode & 0x00FF);
        emu->pc += 2;
        break;
    case 0x7000: /* 7XNN: Adds NN to VX */
        emu->V[(emu->opcode & 0x0F00) >> 8] += (emu->opcode & 0x00FF);
        emu->pc += 2;
        break;
    case 0x8000:
        switch (emu->opcode & 0x000F) {
        case 0x0000: /* 8XY0: Vx = Vy  */
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 4];
            emu->pc += 2;
            break;
        case 0x0001: /* 8XY1: Vx = Vx | Vy */
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 8] | emu->V[(emu->opcode & 0x00F0) >> 4];
            emu->pc += 2;
            break;
        case 0x0002: /* 8XY2: Vx = Vx & Vy*/
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 8] & emu->V[(emu->opcode & 0x00F0) >> 4];
            emu->pc += 2;
            break;
        case 0x0003: /* 8XY3: Vx = Vx XOR Vy */
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 8] ^ emu->V[(emu->opcode & 0x00F0) >> 4];
            emu->pc += 2;
            break;
        case 0x0004: /* 8XY4: Vx += Vy; Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't */
            if(emu->V[(emu->opcode & 0x00F0) >> 4] > (0xFF - emu->V[(emu->opcode & 0x0F00) >> 8]))
                emu->V[0xF] = 1; //carry
            else
                emu->V[0xF] = 0;
            emu->V[(emu->opcode & 0x0F00) >> 8] += emu->V[(emu->opcode & 0x00F0) >> 4];
            emu->pc += 2;
            break;
        case 0x0005: /* 8XY5: Vx -= Vy; VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't */
            if (((int)emu->V[(emu->opcode & 0x0F00) >> 8 ] - (int)emu->V[(emu->opcode & 0x00F0) >> 4]) >= 0) {
                emu->V[0xF] = 1;
            } else {
                emu->V[0xF] &= 0;
            }
            emu->V[(emu->opcode & 0x0F00) >> 8] += emu->V[(emu->opcode & 0x00F0) >> 4];
            emu->pc += 2;
            break;
        case 0x0006: /* 8XY6: Vx>>=1; Stores the least significant bit of VX in VF and then shifts VX to the right by 1 */
            emu->V[0xF] = emu->V[(emu->opcode & 0x0F00) >> 8] & 7;
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 8] >> 1;
            emu->pc += 2;
            break;
        case 0x0007: /* 8XY7: Vx=Vy-Vx; Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't */
            if (((int)emu->V[(emu->opcode & 0x0F00) >> 8] - (int)emu->V[(emu->opcode & 0x00F0) >> 4]) > 0) {
                emu->V[0xF] = 1;
            } else {
                emu->V[0xF] &= 0;
            }
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x00F0) >> 4] - emu->V[(emu->opcode & 0x0F00) >> 8];
            emu->pc += 2;
            break;
        case 0x000E: /* 8XYE: Vx<<=1; Stores the most significant bit of VX in VF and then shifts VX to the left by 1 */
            emu->V[0xF] = emu->V[(emu->opcode & 0x0F00) >> 8] >> 7;
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->V[(emu->opcode & 0x0F00) >> 8] << 1;
            emu->pc += 2;
            break;
        }
        break;
    case 0x9000: /* 9XY0: Skips the next instruction if VX doesn't equal VY */
        if(emu->V[(emu->opcode & 0x0F00) >> 8] != emu->V[(emu->opcode & 0x00F0) >> 4]) {
            emu->pc += 4;
        } else {
            emu->pc += 2;
        }
        break;
    case 0xA000: /* ANNN: Sets I to the address NNN */
        emu->I = emu->opcode & 0x0FFF;
        emu->pc += 2;
        break;
    case 0xB000: /* BNNN: Jumps to the address NNN plus V0 */
        emu->pc = (emu->opcode & 0x0FFF) + emu->V[0];
        break;
    case 0xC000: /* CXNN: Vx=rand() & NN */
        emu->V[(emu->opcode & 0x0F00) >> 8] = rand() & (emu->opcode & 0x00FF);
        emu->pc += 2;
        break;
    case 0xD000: { /* DXYN: draw(Vx,Vy,N); draw at X,Y width 8, height N sprite from I register */
        uint16_t x = emu->V[(emu->opcode & 0x0F00) >> 8];
        uint16_t y = emu->V[(emu->opcode & 0x00F0) >> 4];
        uint16_t height = emu->opcode & 0x000F;
        uint16_t pixel;

        emu->V[0xF] = 0;
        for (int yline = 0; yline < height; yline++) {
            pixel = emu->memory[emu->I + yline];
            for(int xline = 0; xline < 8; xline++) {
                if((pixel & (0x80 >> xline)) != 0) {
                    if(emu->gfx[(x + xline + ((y + yline) * 64))] == 1)
                        emu->V[0xF] = 1;
                    emu->gfx[x + xline + ((y + yline) * 64)] ^= 1;
                }
            }
        }

        emu->draw_flag = true;
        emu->pc += 2;
        break;
    }
    case 0xE000:
        switch (emu->opcode & 0x00FF) {
        case 0x009E: /* EX9E: Skips the next instruction if the key stored in VX is pressed */
            if(emu->key[emu->V[(emu->opcode & 0x0F00) >> 8]]) {
                emu->pc += 4;
            } else {
                emu->pc += 2;
            }
            break;
        case 0x00A1: /* EXA1: Skips the next instruction if the key stored in VX isn't pressed */
            if(!emu->key[emu->V[(emu->opcode & 0x0F00) >> 8]]) {
                emu->pc += 4;
            } else {
                emu->pc += 2;
            }
            break;
        default:
            printf ("Unknown opcode: 0x%X\n", emu->opcode);
        }
        break;
    case 0xF000:
        switch (emu->opcode & 0x00FF) {
        case 0x0007: /* FX07: Sets VX to the value of the delay timer */
            emu->V[(emu->opcode & 0x0F00) >> 8] = emu->delay_timer;
            emu->pc += 2;
            break;
        case 0x000A: /* FX0A: A key press is awaited, and then stored in VX. (blocking) */
            for (int i = 0; i < 0x10; i++) {
                if (emu->key[i]) {
                    emu->V[(emu->opcode & 0x0F00) >> 8] = i;
                    emu->pc += 2;
                }
            }
            break;
        case 0x0015: /* FX15: Sets the delay timer to VX */
            emu->delay_timer = emu->V[(emu->opcode & 0x0F00) >> 8];
            emu->pc += 2;
            break;
        case 0x0018: /* FX18: Sets the sound timer to VX */
            emu->sound_timer = emu->V[(emu->opcode & 0x0F00) >> 8];
            emu->pc += 2;
            break;
        case 0x001E: /* FX1E: Add VX to I register */
            emu->I += emu->V[(emu->opcode & 0x0F00) >> 8];
            emu->pc += 2;
            break;
        case 0x0029: /* FX29: I=sprite_addr[Vx]; Sets I to the location of the sprite for the character in VX */
            emu->I = emu->V[(emu->opcode & 0x0F00) >> 8] * 5;
            emu->pc += 2;
            break;
        case 0x0033: /* FX33: */
            emu->memory[emu->I]     = emu->V[(emu->opcode & 0x0F00) >> 8] / 100;
            emu->memory[emu->I + 1] = (emu->V[(emu->opcode & 0x0F00) >> 8] / 10) % 10;
            emu->memory[emu->I + 2] = (emu->V[(emu->opcode & 0x0F00) >> 8] % 100) % 10;
            emu->pc += 2;
            break;
        case 0x0055: /* FX55: */
            for (int i = 0; i <= ((emu->opcode & 0x0F00) >> 8); i++) {
                emu->memory[emu->I+i] = emu->V[i];
            }
            emu->pc += 2;
            break;
        case 0x0065: /* FX65: */
            for (int i = 0; i <= ((emu->opcode & 0x0F00) >> 8); i++) {
                emu->V[i] = emu->memory[emu->I + i];
            }
            emu->pc += 2;
            break;
        default:
            printf ("Unknown opcode: 0x%X\n", emu->opcode);
        }
        break;

    }

    // Update timers
    if(emu->delay_timer > 0)
        --emu->delay_timer;

    if(emu->sound_timer > 0) {
        if(emu->sound_timer == 1)
            emu->beep();
        --emu->sound_timer;
    }
}
