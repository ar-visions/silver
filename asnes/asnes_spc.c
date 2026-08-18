
static none asnes_spc_step(spc_state spc) {
    u8 opcode = spc_R(spc->pc++);
    
    switch (opcode) {
    // Data Movement Instructions
    case 0x8F: { // MOV dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        spc_W(dp, imm);
        break;
    }

    case 0xCD: { // MOV #imm, X  
        spc->x = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xBD: { // MOV SP, X
        spc->sp = spc->x;
        break;
    }

    case 0x9D: { // MOV X, SP
        spc->x = spc->sp;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0x7D: { // MOV A, X
        spc->a = spc->x;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x5D: { // MOV X, A
        spc->x = spc->a;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xDD: { // MOV A, Y
        spc->a = spc->y;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xFD: { // MOV Y, A
        spc->y = spc->a;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    // Memory to Register Loads
    case 0xE4: { // MOV A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF4: { // MOV A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE5: { // MOV A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF5: { // MOV A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF6: { // MOV A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a = spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE6: { // MOV A, (X)
        spc->a = spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xBF: { // MOV A, (X)+
        spc->a = spc_R(spc->x);
        spc->x++;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE7: { // MOV A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xF7: { // MOV A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a = spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xE8: { // MOV X, #imm
        spc->x = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xF8: { // MOV X, dp
        u8 dp = spc_R(spc->pc++);
        spc->x = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xF9: { // MOV X, dp+Y
        u8 dp = spc_R(spc->pc++);
        spc->x = spc_R((dp + spc->y) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xE9: { // MOV X, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->x = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0x8D: { // MOV Y, #imm (duplicate of 0xCD but for Y)
        spc->y = spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xEB: { // MOV Y, dp
        u8 dp = spc_R(spc->pc++);
        spc->y = spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xFB: { // MOV Y, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->y = spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xEC: { // MOV Y, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->y = spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    // Register to Memory Stores
    case 0xC4: { // MOV dp, A
        u8 dp = spc_R(spc->pc++);
        spc_W(dp, spc->a);
        break;
    }

    case 0xD4: { // MOV dp+X, A
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->x) & 0xFF, spc->a);
        break;
    }

    case 0xC5: { // MOV abs, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->a);
        break;
    }

    case 0xD5: { // MOV abs+X, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W((addr + spc->x) & 0xFFFF, spc->a);
        break;
    }

    case 0xD6: { // MOV abs+Y, A
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W((addr + spc->y) & 0xFFFF, spc->a);
        break;
    }

    case 0xC6: { // MOV (X), A
        spc_W(spc->x, spc->a);
        break;
    }

    case 0xAF: { // MOV (X)+, A
        spc_W(spc->x, spc->a);
        spc->x++;
        break;
    }

    case 0xC7: { // MOV [dp+X], A
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc_W(addr, spc->a);
        break;
    }

    case 0xD7: { // MOV [dp]+Y, A
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc_W((addr + spc->y) & 0xFFFF, spc->a);
        break;
    }

    case 0xC9: { // MOV abs, X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->x);
        break;
    }

    case 0xD9: { // MOV dp+Y, X
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->y) & 0xFF, spc->x);
        break;
    }

    case 0xDB: { // MOV dp+X, Y
        u8 dp = spc_R(spc->pc++);
        spc_W((dp + spc->x) & 0xFF, spc->y);
        break;
    }

    case 0xCB: { // MOV dp, Y
        u8 dp = spc_R(spc->pc++);
        spc_W(dp, spc->y);
        break;
    }

    case 0xCC: { // MOV abs, Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(addr, spc->y);
        break;
    }

    // Memory to Memory
    case 0xFA: { // MOV dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 re = spc_R(src);
        spc_W(dst, re);
        break;
    }

    // Arithmetic Operations
    case 0x84: { // ADC A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x94: { // ADC A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x85: { // ADC A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x95: { // ADC A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x96: { // ADC A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x86: { // ADC A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x97: { // ADC A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x87: { // ADC A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x88: { // ADC A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a + value + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ result) & (value ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0x99: { // ADC (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val + y_val + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((x_val ^ result) & (y_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(spc->x, result & 0xFF);
        break;
    }

    case 0x89: { // ADC dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val + src_val + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((dst_val ^ result) & (src_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dst, result & 0xFF);
        break;
    }

    case 0x98: { // ADC dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val + imm + (spc->psw & 0x01);
        spc->psw = (spc->psw & ~0x8F) | 
                (result > 0xFF ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((dp_val ^ result) & (imm ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dp, result & 0xFF);
        break;
    }

    // SBC operations (similar pattern but subtracting)
    case 0xA4: { // SBC A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                (result < 0x100 ? 0x01 : 0) |
                ((result & 0xFF) == 0 ? 0x02 : 0) |
                (result & 0x80) |
                (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    // SBC Operations (remaining)
    case 0xB4: { // SBC A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA5: { // SBC A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB5: { // SBC A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB6: { // SBC A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA6: { // SBC A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB7: { // SBC A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA7: { // SBC A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xA8: { // SBC A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a - value - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((spc->a ^ value) & (spc->a ^ result) & 0x80) ? 0x40 : 0);
        spc->a = result & 0xFF;
        break;
    }

    case 0xB9: { // SBC (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val - y_val - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((x_val ^ y_val) & (x_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(spc->x, result & 0xFF);
        break;
    }

    case 0xA9: { // SBC dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val - src_val - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((dst_val ^ src_val) & (dst_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dst, result & 0xFF);
        break;
    }

    case 0xB8: { // SBC dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val - imm - (1 - (spc->psw & 0x01));
        spc->psw = (spc->psw & ~0x8F) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80) |
                    (((dp_val ^ imm) & (dp_val ^ result) & 0x80) ? 0x40 : 0);
        spc_W(dp, result & 0xFF);
        break;
    }

    // Comparison Operations
    case 0x64: { // CMP A, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x74: { // CMP A, dp+X
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R((dp + spc->x) & 0xFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x65: { // CMP A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x75: { // CMP A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->x) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x76: { // CMP A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x66: { // CMP A, (X)
        u8 value = spc_R(spc->x);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x77: { // CMP A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x67: { // CMP A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        u8 value = spc_R((addr + spc->y) & 0xFFFF);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x68: { // CMP A, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->a - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x79: { // CMP (X), (Y)
        u8 x_val = spc_R(spc->x);
        u8 y_val = spc_R(spc->y);
        u16 result = x_val - y_val;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x69: { // CMP dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 src_val = spc_R(src);
        u8 dst_val = spc_R(dst);
        u16 result = dst_val - src_val;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x78: { // CMP dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 dp_val = spc_R(dp);
        u16 result = dp_val - imm;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0xC8: { // CMP X, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x3E: { // CMP X, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x1E: { // CMP X, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->x - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0xAD: { // CMP Y, #imm
        u8 value = spc_R(spc->pc++);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x7E: { // CMP Y, dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    case 0x5E: { // CMP Y, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u16 result = spc->y - value;
        spc->psw = (spc->psw & ~0x83) | 
                    (result < 0x100 ? 0x01 : 0) |
                    ((result & 0xFF) == 0 ? 0x02 : 0) |
                    (result & 0x80);
        break;
    }

    // Logical Operations - AND
    case 0x24: { // AND A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a &= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x34: { // AND A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a &= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x25: { // AND A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x35: { // AND A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x36: { // AND A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a &= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x26: { // AND A, (X)
        spc->a &= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x37: { // AND A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a &= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x27: { // AND A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a &= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x28: { // AND A, #imm
        spc->a &= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x39: { // AND (X), (Y)
        u8 result = spc_R(spc->x) & spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x29: { // AND dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) & spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x38: { // AND dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) & imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Logical Operations - OR
    case 0x04: { // OR A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a |= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x14: { // OR A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a |= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x05: { // OR A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x15: { // OR A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x16: { // OR A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a |= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x06: { // OR A, (X)
        spc->a |= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x17: { // OR A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a |= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x07: { // OR A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a |= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x08: { // OR A, #imm
        spc->a |= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x19: { // OR (X), (Y)
        u8 result = spc_R(spc->x) | spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x09: { // OR dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) | spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x18: { // OR dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) | imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // EOR Operations
    case 0x44: { // EOR A, dp
        u8 dp = spc_R(spc->pc++);
        spc->a ^= spc_R(dp);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x54: { // EOR A, dp+X
        u8 dp = spc_R(spc->pc++);
        spc->a ^= spc_R((dp + spc->x) & 0xFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x45: { // EOR A, abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x55: { // EOR A, abs+X
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R((addr + spc->x) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x56: { // EOR A, abs+Y
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->a ^= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x46: { // EOR A, (X)
        spc->a ^= spc_R(spc->x);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x57: { // EOR A, [dp+X]
        u8 dp = spc_R(spc->pc++);
        u16 ptr = (dp + spc->x) & 0xFF;
        u16 addr = spc_R(ptr) | (spc_R(ptr + 1) << 8);
        spc->a ^= spc_R(addr);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x47: { // EOR A, [dp]+Y
        u8 dp = spc_R(spc->pc++);
        u16 addr = spc_R(dp) | (spc_R(dp + 1) << 8);
        spc->a ^= spc_R((addr + spc->y) & 0xFFFF);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x48: { // EOR A, #imm
        spc->a ^= spc_R(spc->pc++);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x59: { // EOR (X), (Y)
        u8 result = spc_R(spc->x) ^ spc_R(spc->y);
        spc_W(spc->x, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x49: { // EOR dp, dp
        u8 src = spc_R(spc->pc++);
        u8 dst = spc_R(spc->pc++);
        u8 result = spc_R(dst) ^ spc_R(src);
        spc_W(dst, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x58: { // EOR dp, #imm
        u8 dp = spc_R(spc->pc++);
        u8 imm = spc_R(spc->pc++);
        u8 result = spc_R(dp) ^ imm;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Increment/Decrement
    case 0xBC: { // INC A
        spc->a++;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x3D: { // INC X
        spc->x++;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xFC: { // INC Y
        spc->y++;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0xAB: { // INC dp
        u8 dp = spc_R(spc->pc++);
        u8 result = spc_R(dp) + 1;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0xBB: { // INC dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 result = spc_R(addr) + 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0xAC: { // INC abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 result = spc_R(addr) + 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x9C: { // DEC A
        spc->a--;
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x1D: { // DEC X
        spc->x--;
        spc->psw = (spc->psw & ~0x82) | (spc->x == 0 ? 0x02 : 0) | (spc->x & 0x80);
        break;
    }

    case 0xDC: { // DEC Y
        spc->y--;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }

    case 0x8B: { // DEC dp
        u8 dp = spc_R(spc->pc++);
        u8 result = spc_R(dp) - 1;
        spc_W(dp, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x9B: { // DEC dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 result = spc_R(addr) - 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    case 0x8C: { // DEC abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 result = spc_R(addr) - 1;
        spc_W(addr, result);
        spc->psw = (spc->psw & ~0x82) | (result == 0 ? 0x02 : 0) | (result & 0x80);
        break;
    }

    // Shift/Rotate Operations
    case 0x1C: { // ASL A
        u8 carry = (spc->a & 0x80) ? 0x01 : 0;
        spc->a <<= 1;
        spc->psw = (spc->psw & ~0x83) | carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x0B: { // ASL dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x1B: { // ASL dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x0C: { // ASL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 carry = (value & 0x80) ? 0x01 : 0;
        value <<= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x5C: { // LSR A
        u8 carry = spc->a & 0x01;
        spc->a >>= 1;
        spc->psw = (spc->psw & ~0x83) | carry | (spc->a == 0 ? 0x02 : 0);
        break;
    }

    case 0x4B: { // LSR dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x5B: { // LSR dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x4C: { // LSR abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 carry = value & 0x01;
        value >>= 1;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | carry | (value == 0 ? 0x02 : 0);
        break;
    }

    case 0x3C: { // ROL A
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (spc->a & 0x80) ? 0x01 : 0;
        spc->a = (spc->a << 1) | old_carry;
        spc->psw = (spc->psw & ~0x83) | new_carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x2B: { // ROL dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x3B: { // ROL dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x2C: { // ROL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = (value & 0x80) ? 0x01 : 0;
        value = (value << 1) | old_carry;
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x7C: { // ROR A
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = spc->a & 0x01;
        spc->a = (spc->a >> 1) | (old_carry << 7);
        spc->psw = (spc->psw & ~0x83) | new_carry | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0x6B: { // ROR dp
        u8 dp = spc_R(spc->pc++);
        u8 value = spc_R(dp);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(dp, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x7B: { // ROR dp+X
        u8 dp = spc_R(spc->pc++);
        u8 addr = (dp + spc->x) & 0xFF;
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    case 0x6C: { // ROR abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 value = spc_R(addr);
        u8 old_carry = spc->psw & 0x01;
        u8 new_carry = value & 0x01;
        value = (value >> 1) | (old_carry << 7);
        spc_W(addr, value);
        spc->psw = (spc->psw & ~0x83) | new_carry | (value == 0 ? 0x02 : 0) | (value & 0x80);
        break;
    }

    // Branch Instructions
    case 0x10: { // BPL rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x80)) spc->pc += offset;
        break;
    }

    case 0x30: { // BMI rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x80) spc->pc += offset;
        break;
    }

    case 0x50: { // BVC rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x40)) spc->pc += offset;
        break;
    }

    case 0x70: { // BVS rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x40) spc->pc += offset;
        break;
    }

    case 0x90: { // BCC rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x01)) spc->pc += offset;
        break;
    }

    case 0xB0: { // BCS rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x01) spc->pc += offset;
        break;
    }

    case 0xD0: { // BNE rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc->psw & 0x02)) spc->pc += offset;
        break;
    }

    case 0xF0: { // BEQ rel
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc->psw & 0x02) spc->pc += offset;
        break;
    }

    case 0x2F: { // BRA rel
        i8 offset = (i8)spc_R(spc->pc++);
        spc->pc += offset;
        break;
    }

    // Bit Operations
    case 0x0A: { // OR1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw |= 0x01;
        }
        break;
    }

    case 0x2A: { // OR1 C, /mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (!(spc_R(addr) & (1 << bit))) {
            spc->psw |= 0x01;
        }
        break;
    }

    case 0x4A: { // AND1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (!(spc_R(addr) & (1 << bit))) {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0x6A: { // AND1 C, /mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0x8A: { // EOR1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw ^= 0x01;
        }
        break;
    }

    case 0xAA: { // MOV1 C, mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        if (spc_R(addr) & (1 << bit)) {
            spc->psw |= 0x01;
        } else {
            spc->psw &= ~0x01;
        }
        break;
    }

    case 0xCA: { // MOV1 mem.bit, C
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        u8   r = spc_R(addr);
        if (spc->psw & 0x01) {
            spc_W(addr, r | (1 << bit));
        } else {
            spc_W(addr, r & ~(1 << bit));
        }
        break;
    }

    case 0xEA: { // NOT1 mem.bit
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        u8 bit = (addr >> 13) & 7;
        addr &= 0x1FFF;
        u8   r = spc_R(addr);
        spc_W(addr, r ^ (1 << bit));
        break;
    }

    // Set/Clear Bits
    case 0x02: { // SET1 dp.0
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x01);
        break;
    }

    case 0x22: { // SET1 dp.1
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x02);
        break;
    }

    case 0x42: { // SET1 dp.2
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x04);
        break;
    }

    case 0x62: { // SET1 dp.3
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x08);
        break;
    }

    case 0x82: { // SET1 dp.4
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x10);
        break;
    }

    case 0xA2: { // SET1 dp.5
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x20);
        break;
    }

    case 0xC2: { // SET1 dp.6
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x40);
        break;
    }

    case 0xE2: { // SET1 dp.7
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r | 0x80);
        break;
    }

    case 0x12: { // CLR1 dp.0
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x01);
        break;
    }

    case 0x32: { // CLR1 dp.1
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x02);
        break;
    }

    case 0x52: { // CLR1 dp.2
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x04);
        break;
    }

    case 0x72: { // CLR1 dp.3
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x08);
        break;
    }

    case 0x92: { // CLR1 dp.4
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x10);
        break;
    }

    case 0xB2: { // CLR1 dp.5
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x20);
        break;
    }

    case 0xD2: { // CLR1 dp.6
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x40);
        break;
    }

    case 0xF2: { // CLR1 dp.7
        u8 dp = spc_R(spc->pc++);
        u8  r = spc_R(dp);
        spc_W(dp, r & ~0x80);
        break;
    }

    // Branch on Bit
    case 0x03: { // BBS dp.0, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x01) spc->pc += offset;
        break;
    }

    case 0x23: { // BBS dp.1, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x02) spc->pc += offset;
        break;
    }

    case 0x43: { // BBS dp.2, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x04) spc->pc += offset;
        break;
    }

    case 0x63: { // BBS dp.3, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x08) spc->pc += offset;
        break;
    }

    case 0x83: { // BBS dp.4, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x10) spc->pc += offset;
        break;
    }

    case 0xA3: { // BBS dp.5, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x20) spc->pc += offset;
        break;
    }

    case 0xC3: { // BBS dp.6, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x40) spc->pc += offset;
        break;
    }

    case 0xE3: { // BBS dp.7, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (spc_R(dp) & 0x80) spc->pc += offset;
        break;
    }

    case 0x13: { // BBC dp.0, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x01)) spc->pc += offset;
        break;
    }

    case 0x33: { // BBC dp.1, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x02)) spc->pc += offset;
        break;
    }

    case 0x53: { // BBC dp.2, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x04)) spc->pc += offset;
        break;
    }

    case 0x73: { // BBC dp.3, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x08)) spc->pc += offset;
        break;
    }

    case 0x93: { // BBC dp.4, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x10)) spc->pc += offset;
        break;
    }

    case 0xB3: { // BBC dp.5, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x20)) spc->pc += offset;
        break;
    }

    case 0xD3: { // BBC dp.6, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x40)) spc->pc += offset;
        break;
    }

    case 0xF3: { // BBC dp.7, rel
        u8 dp = spc_R(spc->pc++);
        i8 offset = (i8)spc_R(spc->pc++);
        if (!(spc_R(dp) & 0x80)) spc->pc += offset;
        break;
    }

    // Jump/Call Instructions
    case 0x5F: { // JMP abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc->pc = addr;
        break;
    }

    case 0x1F: { // JMP [abs+X]
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        addr += spc->x;
        spc->pc = spc_R(addr) | (spc_R(addr + 1) << 8);
        break;
    }

    case 0x3F: { // CALL abs
        u16 addr = spc_R(spc->pc++) | (spc_R(spc->pc++) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x4F: { // PCALL up
        u8 page = spc_R(spc->pc++);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = 0xFF00 | page;
        break;
    }

    case 0x01: { // TCALL 0
        u16 addr = spc_R(0xFFDE) | (spc_R(0xFFDF) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x11: { // TCALL 1
        u16 addr = spc_R(0xFFDC) | (spc_R(0xFFDD) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x21: { // TCALL 2
        u16 addr = spc_R(0xFFDA) | (spc_R(0xFFDB) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x31: { // TCALL 3
        u16 addr = spc_R(0xFFD8) | (spc_R(0xFFD9) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x41: { // TCALL 4
        u16 addr = spc_R(0xFFD6) | (spc_R(0xFFD7) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x51: { // TCALL 5
        u16 addr = spc_R(0xFFD4) | (spc_R(0xFFD5) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x61: { // TCALL 6
        u16 addr = spc_R(0xFFD2) | (spc_R(0xFFD3) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x71: { // TCALL 7
        u16 addr = spc_R(0xFFD0) | (spc_R(0xFFD1) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x81: { // TCALL 8
        u16 addr = spc_R(0xFFCE) | (spc_R(0xFFCF) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x91: { // TCALL 9
        u16 addr = spc_R(0xFFCC) | (spc_R(0xFFCD) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xA1: { // TCALL 10
        u16 addr = spc_R(0xFFCA) | (spc_R(0xFFCB) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xB1: { // TCALL 11
        u16 addr = spc_R(0xFFC8) | (spc_R(0xFFC9) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xC1: { // TCALL 12
        u16 addr = spc_R(0xFFC6) | (spc_R(0xFFC7) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xD1: { // TCALL 13
        u16 addr = spc_R(0xFFC4) | (spc_R(0xFFC5) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xE1: { // TCALL 14
        u16 addr = spc_R(0xFFC2) | (spc_R(0xFFC3) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0xF1: { // TCALL 15
        u16 addr = spc_R(0xFFC0) | (spc_R(0xFFC1) << 8);
        spc_W(0x0100 + spc->sp--, (spc->pc >> 8) & 0xFF);
        spc_W(0x0100 + spc->sp--, spc->pc & 0xFF);
        spc->pc = addr;
        break;
    }

    case 0x6F: { // RET
        u8 lo = spc_R(0x0100 + ++spc->sp);
        u8 hi = spc_R(0x0100 + ++spc->sp);
        spc->pc = lo | (hi << 8);
        break;
    }

    case 0x7F: { // RETI
        spc->psw = spc_R(0x0100 + ++spc->sp);
        u8 lo = spc_R(0x0100 + ++spc->sp);
        u8 hi = spc_R(0x0100 + ++spc->sp);
        spc->pc = lo | (hi << 8);
        break;
    }

    // Stack Operations
    case 0x2D: { // PUSH A
        spc_W(0x0100 + spc->sp--, spc->a);
        break;
    }

    case 0x4D: { // PUSH X
        spc_W(0x0100 + spc->sp--, spc->x);
        break;
    }

    case 0x6D: { // PUSH Y
        spc_W(0x0100 + spc->sp--, spc->y);
        break;
    }

    case 0x0D: { // PUSH PSW
        spc_W(0x0100 + spc->sp--, spc->psw);
        break;
    }

    case 0xAE: { // POP A
        spc->a = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0xCE: { // POP X
        spc->x = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0xEE: { // POP Y
        spc->y = spc_R(0x0100 + ++spc->sp);
        break;
    }

    case 0x8E: { // POP PSW
        spc->psw = spc_R(0x0100 + ++spc->sp);
        break;
    }

    // Flag Operations
    case 0x60: { // CLRC
        spc->psw &= ~0x01;
        break;
    }

    case 0x80: { // SETC
        spc->psw |= 0x01;
        break;
    }

    case 0xE0: { // CLRV
        spc->psw &= ~0x40;
        break;
    }

    case 0x20: { // CLRP
        spc->psw &= ~0x20;
        break;
    }

    case 0x40: { // SETP
        spc->psw |= 0x20;
        break;
    }

    case 0xA0: { // EI
        spc->psw &= ~0x04;
        break;
    }

    case 0xC0: { // DI
        spc->psw |= 0x04;
        break;
    }

    // Special Operations
    case 0x9F: { // XCN A (already implemented above, but included here for completeness)
        spc->a = (spc->a << 4) | (spc->a >> 4);
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xDF: { // DAA A
        if ((spc->a & 0x0F) > 9 || (spc->psw & 0x08)) {
            spc->a += 6;
        }
        if ((spc->a & 0xF0) > 0x90 || (spc->psw & 0x01)) {
            spc->a += 0x60;
            spc->psw |= 0x01;
        } else {
            spc->psw &= ~0x01;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xBE: { // DAS A
        if (!(spc->psw & 0x08) && (spc->a & 0x0F) > 9) {
            spc->a -= 6;
        }
        if (!(spc->psw & 0x01) && spc->a > 0x99) {
            spc->a -= 0x60;
            spc->psw &= ~0x01;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xED: { // NOTC
        spc->psw ^= 0x01;
        break;
    }

    case 0x9E: { // DIV YA, X
        u16 ya = spc->a | (spc->y << 8);
        if (spc->x == 0) {
            spc->psw |= 0x40; // Set overflow
            spc->a = 0xFF;
            spc->y = 0xFF;
        } else {
            spc->psw &= ~0x40;
            spc->a = ya / spc->x;
            spc->y = ya % spc->x;
        }
        spc->psw = (spc->psw & ~0x82) | (spc->a == 0 ? 0x02 : 0) | (spc->a & 0x80);
        break;
    }

    case 0xCF: { // MUL YA
        u16 result = spc->a * spc->y;
        spc->a = result & 0xFF;
        spc->y = (result >> 8) & 0xFF;
        spc->psw = (spc->psw & ~0x82) | (spc->y == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }


    case 0xDA: { // MOVW YA, dp
        u8 dp = spc_R(spc->pc++);
        spc->a = spc_R(dp);
        spc->y = spc_R(dp + 1);
        u16 ya = spc->a | (spc->y << 8);
        spc->psw = (spc->psw & ~0x82) | (ya == 0 ? 0x02 : 0) | (spc->y & 0x80);
        break;
    }
    case 0xBA: { // MOVW dp, YA
        u8 dp = spc_R(spc->pc++);
        spc_W(dp,   spc->a);
        spc_W(dp+1, spc->y);
        break; 
    }

    // System
    case 0xFF: { // STOP
        // Halt the SPC700 processor
        // For emulation purposes, we can set a flag or just return
        print("SPC700 STOP instruction executed at PC: 0x%04X", spc->pc - 1);
        return; // Stop execution
    }

    case 0xEF: { // SLEEP
        // Put SPC700 to sleep until interrupt
        // For now, just continue (would normally wait for timer or other interrupt)
        break;
    }

    case 0x00: { // NOP
        // No operation
        break;
    }

    // Undefined/Invalid opcodes (handle as NOP or error)
    default:   // Handle undefined opcodes
        printf("unhandled spc opcode %02X at %04X\n", opcode, spc->pc - 1);
        break;
    }
}