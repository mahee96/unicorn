/*
 * Tiny Code Interpreter (TCI) for QEMU / Unicorn engine
 *
 * Executes TCG bytecodes in pure C software interpreter mode without JIT or RWX memory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "qemu/osdep.h"
#include "tcg/tcg.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "qemu/host-utils.h"

void tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_addr, uintptr_t addr)
{
    memcpy((void *)jmp_addr, &addr, sizeof(addr));
}

static inline uint8_t read_b(const uint8_t **tb_ptr)
{
    uint8_t val = **tb_ptr;
    *tb_ptr += 1;
    return val;
}

static inline uint64_t read_l(const uint8_t **tb_ptr)
{
    uint64_t val;
    memcpy(&val, *tb_ptr, sizeof(val));
    *tb_ptr += sizeof(val);
    return val;
}

static inline TCGReg read_r(const uint8_t **tb_ptr)
{
    return (TCGReg)read_b(tb_ptr);
}

static bool tci_compare32(uint32_t v1, uint32_t v2, TCGCond cond)
{
    switch (cond) {
    case TCG_COND_EQ:  return v1 == v2;
    case TCG_COND_NE:  return v1 != v2;
    case TCG_COND_LT:  return (int32_t)v1 < (int32_t)v2;
    case TCG_COND_GE:  return (int32_t)v1 >= (int32_t)v2;
    case TCG_COND_LE:  return (int32_t)v1 <= (int32_t)v2;
    case TCG_COND_GT:  return (int32_t)v1 > (int32_t)v2;
    case TCG_COND_LTU: return v1 < v2;
    case TCG_COND_GEU: return v1 >= v2;
    case TCG_COND_LEU: return v1 <= v2;
    case TCG_COND_GTU: return v1 > v2;
    default: g_assert_not_reached();
    }
}

static bool tci_compare64(uint64_t v1, uint64_t v2, TCGCond cond)
{
    switch (cond) {
    case TCG_COND_EQ:  return v1 == v2;
    case TCG_COND_NE:  return v1 != v2;
    case TCG_COND_LT:  return (int64_t)v1 < (int64_t)v2;
    case TCG_COND_GE:  return (int64_t)v1 >= (int64_t)v2;
    case TCG_COND_LE:  return (int64_t)v1 <= (int64_t)v2;
    case TCG_COND_GT:  return (int64_t)v1 > (int64_t)v2;
    case TCG_COND_LTU: return v1 < v2;
    case TCG_COND_GEU: return v1 >= v2;
    case TCG_COND_LEU: return v1 <= v2;
    case TCG_COND_GTU: return v1 > v2;
    default: g_assert_not_reached();
    }
}

uintptr_t tcg_qemu_tb_exec(CPUArchState *env, uint8_t *tb_ptr)
{
    uint64_t regs[TCG_TARGET_NB_REGS];
    uint64_t stack_frame[CPU_TEMP_BUF_NLONGS];
    regs[TCG_AREG0] = (uintptr_t)env;
    regs[TCG_REG_CALL_STACK] = (uintptr_t)stack_frame;

    while (1) {
        TCGOpcode opc = (TCGOpcode)read_b((const uint8_t **)&tb_ptr);

        switch (opc) {
        case INDEX_op_exit_tb: {
            uint64_t ret = read_l((const uint8_t **)&tb_ptr);
            return (uintptr_t)ret;
        }
        case INDEX_op_goto_tb: {
            uint64_t ptr = read_l((const uint8_t **)&tb_ptr);
            if (ptr != 0) {
                tb_ptr = (uint8_t *)(uintptr_t)ptr;
            }
            break;
        }
        case INDEX_op_br: {
            uint64_t ptr = read_l((const uint8_t **)&tb_ptr);
            tb_ptr = (uint8_t *)(uintptr_t)ptr;
            break;
        }
        case INDEX_op_brcond_i32: {
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            uint64_t ptr = read_l((const uint8_t **)&tb_ptr);
            if (tci_compare32((uint32_t)regs[r1], (uint32_t)regs[r2], cond)) {
                tb_ptr = (uint8_t *)(uintptr_t)ptr;
            }
            break;
        }
        case INDEX_op_brcond_i64: {
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            uint64_t ptr = read_l((const uint8_t **)&tb_ptr);
            if (tci_compare64(regs[r1], regs[r2], cond)) {
                tb_ptr = (uint8_t *)(uintptr_t)ptr;
            }
            break;
        }
        case INDEX_op_goto_ptr: {
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            if (regs[r1] == 0) {
                return 0;
            }
            tb_ptr = (uint8_t *)(uintptr_t)regs[r1];
            break;
        }
        case INDEX_op_mov_i32:
        case INDEX_op_mov_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[rs];
            break;
        }
        case INDEX_op_movi_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            uint64_t val = read_l((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)val;
            break;
        }
        case INDEX_op_movi_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            uint64_t val = read_l((const uint8_t **)&tb_ptr);
            regs[rd] = val;
            break;
        }
        case INDEX_op_setcond_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            regs[rd] = tci_compare32((uint32_t)regs[r1], (uint32_t)regs[r2], cond);
            break;
        }
        case INDEX_op_setcond_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            regs[rd] = tci_compare64(regs[r1], regs[r2], cond);
            break;
        }
        case INDEX_op_movcond_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg c1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg c2 = read_r((const uint8_t **)&tb_ptr);
            TCGReg v1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg v2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            regs[rd] = tci_compare32((uint32_t)regs[c1], (uint32_t)regs[c2], cond) ? (uint32_t)regs[v1] : (uint32_t)regs[v2];
            break;
        }
        case INDEX_op_movcond_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg c1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg c2 = read_r((const uint8_t **)&tb_ptr);
            TCGReg v1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg v2 = read_r((const uint8_t **)&tb_ptr);
            TCGCond cond = (TCGCond)read_b((const uint8_t **)&tb_ptr);
            regs[rd] = tci_compare64(regs[c1], regs[c2], cond) ? regs[v1] : regs[v2];
            break;
        }

        /* Frame loads */
        case INDEX_op_ld8u_i32:
        case INDEX_op_ld8u_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            regs[rd] = *(uint8_t *)(uintptr_t)(regs[rs] + off);
            break;
        }
        case INDEX_op_ld8s_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(int8_t)*(uint8_t *)(uintptr_t)(regs[rs] + off);
            break;
        }
        case INDEX_op_ld8s_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)(int8_t)*(uint8_t *)(uintptr_t)(regs[rs] + off);
            break;
        }
        case INDEX_op_ld16u_i32:
        case INDEX_op_ld16u_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint16_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = v;
            break;
        }
        case INDEX_op_ld16s_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            int16_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = (uint32_t)v;
            break;
        }
        case INDEX_op_ld16s_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            int16_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = (int64_t)v;
            break;
        }
        case INDEX_op_ld32u_i64:
        case INDEX_op_ld_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint32_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = v;
            break;
        }
        case INDEX_op_ld32s_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            int32_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = (int64_t)v;
            break;
        }
        case INDEX_op_ld_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint64_t v;
            memcpy(&v, (void *)(uintptr_t)(regs[rs] + off), sizeof(v));
            regs[rd] = v;
            break;
        }

        /* Frame stores */
        case INDEX_op_st8_i32:
        case INDEX_op_st8_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint8_t v = (uint8_t)regs[rd];
            *(uint8_t *)(uintptr_t)(regs[rs] + off) = v;
            break;
        }
        case INDEX_op_st16_i32:
        case INDEX_op_st16_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint16_t v = (uint16_t)regs[rd];
            memcpy((void *)(uintptr_t)(regs[rs] + off), &v, sizeof(v));
            break;
        }
        case INDEX_op_st32_i64:
        case INDEX_op_st_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint32_t v = (uint32_t)regs[rd];
            memcpy((void *)(uintptr_t)(regs[rs] + off), &v, sizeof(v));
            break;
        }
        case INDEX_op_st_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint64_t off = read_l((const uint8_t **)&tb_ptr);
            uint64_t v = regs[rd];
            memcpy((void *)(uintptr_t)(regs[rs] + off), &v, sizeof(v));
            break;
        }

        /* SoftMMU Memory Access */
        case INDEX_op_qemu_ld_i32:
        case INDEX_op_qemu_ld_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg raddr = read_r((const uint8_t **)&tb_ptr);
            TCGMemOpIdx oi = (TCGMemOpIdx)read_l((const uint8_t **)&tb_ptr);
            MemOp mop = get_memop(oi);
            target_ulong addr = regs[raddr];
            uintptr_t ra = 0;

            switch (mop & (MO_BSWAP | MO_SSIZE)) {
            case MO_UB:
                regs[rd] = helper_ret_ldub_mmu(env, addr, oi, ra);
                break;
            case MO_SB:
                regs[rd] = helper_ret_ldsb_mmu(env, addr, oi, ra);
                break;
            case MO_LEUW:
                regs[rd] = helper_le_lduw_mmu(env, addr, oi, ra);
                break;
            case MO_LESW:
                regs[rd] = helper_le_ldsw_mmu(env, addr, oi, ra);
                break;
            case MO_LEUL:
                regs[rd] = helper_le_ldul_mmu(env, addr, oi, ra);
                break;
            case MO_LESL:
                regs[rd] = helper_le_ldsl_mmu(env, addr, oi, ra);
                break;
            case MO_LEQ:
                regs[rd] = helper_le_ldq_mmu(env, addr, oi, ra);
                break;
            case MO_BEUW:
                regs[rd] = helper_be_lduw_mmu(env, addr, oi, ra);
                break;
            case MO_BESW:
                regs[rd] = helper_be_ldsw_mmu(env, addr, oi, ra);
                break;
            case MO_BEUL:
                regs[rd] = helper_be_ldul_mmu(env, addr, oi, ra);
                break;
            case MO_BESL:
                regs[rd] = helper_be_ldsl_mmu(env, addr, oi, ra);
                break;
            case MO_BEQ:
                regs[rd] = helper_be_ldq_mmu(env, addr, oi, ra);
                break;
            default:
                g_assert_not_reached();
            }
            break;
        }
        case INDEX_op_qemu_st_i32:
        case INDEX_op_qemu_st_i64: {
            TCGReg rval = read_r((const uint8_t **)&tb_ptr);
            TCGReg raddr = read_r((const uint8_t **)&tb_ptr);
            TCGMemOpIdx oi = (TCGMemOpIdx)read_l((const uint8_t **)&tb_ptr);
            MemOp mop = get_memop(oi);
            target_ulong addr = regs[raddr];
            uint64_t val = regs[rval];
            uintptr_t ra = 0;

            switch (mop & (MO_BSWAP | MO_SIZE)) {
            case MO_UB:
                helper_ret_stb_mmu(env, addr, val, oi, ra);
                break;
            case MO_LEUW:
                helper_le_stw_mmu(env, addr, val, oi, ra);
                break;
            case MO_LEUL:
                helper_le_stl_mmu(env, addr, val, oi, ra);
                break;
            case MO_LEQ:
                helper_le_stq_mmu(env, addr, val, oi, ra);
                break;
            case MO_BEUW:
                helper_be_stw_mmu(env, addr, val, oi, ra);
                break;
            case MO_BEUL:
                helper_be_stl_mmu(env, addr, val, oi, ra);
                break;
            case MO_BEQ:
                helper_be_stq_mmu(env, addr, val, oi, ra);
                break;
            default:
                g_assert_not_reached();
            }
            break;
        }

        /* Arithmetic */
        case INDEX_op_add_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] + regs[r2]);
            break;
        }
        case INDEX_op_add_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] + regs[r2];
            break;
        }
        case INDEX_op_sub_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] - regs[r2]);
            break;
        }
        case INDEX_op_sub_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] - regs[r2];
            break;
        }
        case INDEX_op_mul_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] * regs[r2]);
            break;
        }
        case INDEX_op_mul_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] * regs[r2];
            break;
        }
        case INDEX_op_add2_i32: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg al = read_r((const uint8_t **)&tb_ptr);
            TCGReg ah = read_r((const uint8_t **)&tb_ptr);
            TCGReg bl = read_r((const uint8_t **)&tb_ptr);
            TCGReg bh = read_r((const uint8_t **)&tb_ptr);
            uint64_t sum = (uint64_t)(uint32_t)regs[al] + (uint32_t)regs[bl];
            uint32_t h = (uint32_t)regs[ah] + (uint32_t)regs[bh] + (sum >> 32);
            regs[rl] = (uint32_t)sum;
            regs[rh] = h;
            break;
        }
        case INDEX_op_add2_i64: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg al = read_r((const uint8_t **)&tb_ptr);
            TCGReg ah = read_r((const uint8_t **)&tb_ptr);
            TCGReg bl = read_r((const uint8_t **)&tb_ptr);
            TCGReg bh = read_r((const uint8_t **)&tb_ptr);
            uint64_t a_l = regs[al], b_l = regs[bl];
            uint64_t sum_l = a_l + b_l;
            uint64_t carry = (sum_l < a_l);
            uint64_t sum_h = regs[ah] + regs[bh] + carry;
            regs[rl] = sum_l;
            regs[rh] = sum_h;
            break;
        }
        case INDEX_op_sub2_i32: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg al = read_r((const uint8_t **)&tb_ptr);
            TCGReg ah = read_r((const uint8_t **)&tb_ptr);
            TCGReg bl = read_r((const uint8_t **)&tb_ptr);
            TCGReg bh = read_r((const uint8_t **)&tb_ptr);
            uint32_t a_l = (uint32_t)regs[al], b_l = (uint32_t)regs[bl];
            uint32_t borrow = (a_l < b_l);
            regs[rl] = a_l - b_l;
            regs[rh] = (uint32_t)regs[ah] - (uint32_t)regs[bh] - borrow;
            break;
        }
        case INDEX_op_sub2_i64: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg al = read_r((const uint8_t **)&tb_ptr);
            TCGReg ah = read_r((const uint8_t **)&tb_ptr);
            TCGReg bl = read_r((const uint8_t **)&tb_ptr);
            TCGReg bh = read_r((const uint8_t **)&tb_ptr);
            uint64_t a_l = regs[al], b_l = regs[bl];
            uint64_t borrow = (a_l < b_l);
            regs[rl] = a_l - b_l;
            regs[rh] = regs[ah] - regs[bh] - borrow;
            break;
        }
        case INDEX_op_mulu2_i32: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg a = read_r((const uint8_t **)&tb_ptr);
            TCGReg b = read_r((const uint8_t **)&tb_ptr);
            uint64_t res = (uint64_t)(uint32_t)regs[a] * (uint32_t)regs[b];
            regs[rl] = (uint32_t)res;
            regs[rh] = (uint32_t)(res >> 32);
            break;
        }
        case INDEX_op_muls2_i32: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg a = read_r((const uint8_t **)&tb_ptr);
            TCGReg b = read_r((const uint8_t **)&tb_ptr);
            int64_t res = (int64_t)(int32_t)regs[a] * (int32_t)regs[b];
            regs[rl] = (uint32_t)res;
            regs[rh] = (uint32_t)(res >> 32);
            break;
        }
        case INDEX_op_mulu2_i64: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg a = read_r((const uint8_t **)&tb_ptr);
            TCGReg b = read_r((const uint8_t **)&tb_ptr);
            uint64_t l, h;
            mulu64(&l, &h, regs[a], regs[b]);
            regs[rl] = l;
            regs[rh] = h;
            break;
        }
        case INDEX_op_muls2_i64: {
            TCGReg rl = read_r((const uint8_t **)&tb_ptr);
            TCGReg rh = read_r((const uint8_t **)&tb_ptr);
            TCGReg a = read_r((const uint8_t **)&tb_ptr);
            TCGReg b = read_r((const uint8_t **)&tb_ptr);
            uint64_t l, h;
            muls64(&l, &h, regs[a], regs[b]);
            regs[rl] = l;
            regs[rh] = h;
            break;
        }
        case INDEX_op_muluh_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            uint64_t res = (uint64_t)(uint32_t)regs[r1] * (uint32_t)regs[r2];
            regs[rd] = (uint32_t)(res >> 32);
            break;
        }
        case INDEX_op_mulsh_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            int64_t res = (int64_t)(int32_t)regs[r1] * (int32_t)regs[r2];
            regs[rd] = (uint32_t)(res >> 32);
            break;
        }
        case INDEX_op_muluh_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            uint64_t l, h;
            mulu64(&l, &h, regs[r1], regs[r2]);
            regs[rd] = h;
            break;
        }
        case INDEX_op_mulsh_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            uint64_t l, h;
            muls64(&l, &h, regs[r1], regs[r2]);
            regs[rd] = h;
            break;
        }
        case INDEX_op_div_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)((int32_t)regs[r1] / (int32_t)regs[r2]);
            break;
        }
        case INDEX_op_divu_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] / (uint32_t)regs[r2];
            break;
        }
        case INDEX_op_rem_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)((int32_t)regs[r1] % (int32_t)regs[r2]);
            break;
        }
        case INDEX_op_remu_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] % (uint32_t)regs[r2];
            break;
        }
        case INDEX_op_div_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)regs[r1] / (int64_t)regs[r2];
            break;
        }
        case INDEX_op_divu_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] / regs[r2];
            break;
        }
        case INDEX_op_rem_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)regs[r1] % (int64_t)regs[r2];
            break;
        }
        case INDEX_op_remu_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] % regs[r2];
            break;
        }

        /* Bitwise Logic */
        case INDEX_op_and_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] & regs[r2]);
            break;
        }
        case INDEX_op_and_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] & regs[r2];
            break;
        }
        case INDEX_op_or_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] | regs[r2]);
            break;
        }
        case INDEX_op_or_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] | regs[r2];
            break;
        }
        case INDEX_op_xor_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] ^ regs[r2]);
            break;
        }
        case INDEX_op_xor_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] ^ regs[r2];
            break;
        }
        case INDEX_op_andc_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] & ~regs[r2]);
            break;
        }
        case INDEX_op_andc_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] & ~regs[r2];
            break;
        }
        case INDEX_op_orc_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(regs[r1] | ~regs[r2]);
            break;
        }
        case INDEX_op_orc_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] | ~regs[r2];
            break;
        }
        case INDEX_op_eqv_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(~(regs[r1] ^ regs[r2]));
            break;
        }
        case INDEX_op_eqv_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ~(regs[r1] ^ regs[r2]);
            break;
        }
        case INDEX_op_nand_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(~(regs[r1] & regs[r2]));
            break;
        }
        case INDEX_op_nand_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ~(regs[r1] & regs[r2]);
            break;
        }
        case INDEX_op_nor_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(~(regs[r1] | regs[r2]));
            break;
        }
        case INDEX_op_nor_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ~(regs[r1] | regs[r2]);
            break;
        }
        case INDEX_op_not_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(~regs[rs]);
            break;
        }
        case INDEX_op_not_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ~regs[rs];
            break;
        }
        case INDEX_op_neg_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(-regs[rs]);
            break;
        }
        case INDEX_op_neg_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = -regs[rs];
            break;
        }

        /* Shifts & Rotates */
        case INDEX_op_shl_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] << (regs[r2] & 31);
            break;
        }
        case INDEX_op_shl_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] << (regs[r2] & 63);
            break;
        }
        case INDEX_op_shr_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] >> (regs[r2] & 31);
            break;
        }
        case INDEX_op_shr_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] >> (regs[r2] & 63);
            break;
        }
        case INDEX_op_sar_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)((int32_t)regs[r1] >> (regs[r2] & 31));
            break;
        }
        case INDEX_op_sar_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)regs[r1] >> (regs[r2] & 63);
            break;
        }
        case INDEX_op_rotl_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = rol32((uint32_t)regs[r1], regs[r2] & 31);
            break;
        }
        case INDEX_op_rotr_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ror32((uint32_t)regs[r1], regs[r2] & 31);
            break;
        }
        case INDEX_op_rotl_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = rol64(regs[r1], regs[r2] & 63);
            break;
        }
        case INDEX_op_rotr_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ror64(regs[r1], regs[r2] & 63);
            break;
        }

        /* Extensions & Swaps */
        case INDEX_op_ext8s_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(int8_t)regs[rs];
            break;
        }
        case INDEX_op_ext8s_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)(int8_t)regs[rs];
            break;
        }
        case INDEX_op_ext16s_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)(int16_t)regs[rs];
            break;
        }
        case INDEX_op_ext16s_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)(int16_t)regs[rs];
            break;
        }
        case INDEX_op_ext32s_i64:
        case INDEX_op_ext_i32_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (int64_t)(int32_t)regs[rs];
            break;
        }
        case INDEX_op_ext8u_i32:
        case INDEX_op_ext8u_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint8_t)regs[rs];
            break;
        }
        case INDEX_op_ext16u_i32:
        case INDEX_op_ext16u_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint16_t)regs[rs];
            break;
        }
        case INDEX_op_ext32u_i64:
        case INDEX_op_extu_i32_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[rs];
            break;
        }
        case INDEX_op_bswap16_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)bswap16((uint16_t)regs[rs]);
            break;
        }
        case INDEX_op_bswap16_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = bswap16((uint16_t)regs[rs]);
            break;
        }
        case INDEX_op_bswap32_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = bswap32((uint32_t)regs[rs]);
            break;
        }
        case INDEX_op_bswap32_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = bswap32((uint32_t)regs[rs]);
            break;
        }
        case INDEX_op_bswap64_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = bswap64(regs[rs]);
            break;
        }

        /* Bit Operations */
        case INDEX_op_clz_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] ? clz32((uint32_t)regs[r1]) : (uint32_t)regs[r2];
            break;
        }
        case INDEX_op_clz_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] ? clz64(regs[r1]) : regs[r2];
            break;
        }
        case INDEX_op_ctz_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = (uint32_t)regs[r1] ? ctz32((uint32_t)regs[r1]) : (uint32_t)regs[r2];
            break;
        }
        case INDEX_op_ctz_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = regs[r1] ? ctz64(regs[r1]) : regs[r2];
            break;
        }
        case INDEX_op_ctpop_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ctpop32((uint32_t)regs[rs]);
            break;
        }
        case INDEX_op_ctpop_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            regs[rd] = ctpop64(regs[rs]);
            break;
        }
        case INDEX_op_deposit_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint32_t mask = (len == 32 ? ~(uint32_t)0 : ((1U << len) - 1)) << pos;
            regs[rd] = ((uint32_t)regs[r1] & ~mask) | (((uint32_t)regs[r2] << pos) & mask);
            break;
        }
        case INDEX_op_deposit_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg r1 = read_r((const uint8_t **)&tb_ptr);
            TCGReg r2 = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint64_t mask = (len == 64 ? ~(uint64_t)0 : ((1ULL << len) - 1)) << pos;
            regs[rd] = (regs[r1] & ~mask) | ((regs[r2] << pos) & mask);
            break;
        }
        case INDEX_op_extract_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint32_t mask = len == 32 ? ~(uint32_t)0 : ((1U << len) - 1);
            regs[rd] = ((uint32_t)regs[rs] >> pos) & mask;
            break;
        }
        case INDEX_op_extract_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint64_t mask = len == 64 ? ~(uint64_t)0 : ((1ULL << len) - 1);
            regs[rd] = (regs[rs] >> pos) & mask;
            break;
        }
        case INDEX_op_sextract_i32: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint32_t val = ((uint32_t)regs[rs] >> pos) & (len == 32 ? ~(uint32_t)0 : ((1U << len) - 1));
            if (len < 32 && (val & (1U << (len - 1)))) {
                val |= ~((1U << len) - 1);
            }
            regs[rd] = val;
            break;
        }
        case INDEX_op_sextract_i64: {
            TCGReg rd = read_r((const uint8_t **)&tb_ptr);
            TCGReg rs = read_r((const uint8_t **)&tb_ptr);
            uint8_t pos = read_b((const uint8_t **)&tb_ptr);
            uint8_t len = read_b((const uint8_t **)&tb_ptr);
            uint64_t val = (regs[rs] >> pos) & (len == 64 ? ~(uint64_t)0 : ((1ULL << len) - 1));
            if (len < 64 && (val & (1ULL << (len - 1)))) {
                val |= ~((1ULL << len) - 1);
            }
            regs[rd] = val;
            break;
        }

        /* Helper Calls & Barriers */
        case INDEX_op_call: {
            uint64_t func_ptr = read_l((const uint8_t **)&tb_ptr);
            typedef uint64_t (*tcg_target_call_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
            tcg_target_call_fn fn = (tcg_target_call_fn)(uintptr_t)func_ptr;
            regs[TCG_REG_R1] = fn(regs[TCG_REG_R1], regs[TCG_REG_R2], regs[TCG_REG_R3],
                                  regs[TCG_REG_R4], regs[TCG_REG_R5], regs[TCG_REG_R6], regs[TCG_REG_R7]);
            break;
        }
        case INDEX_op_mb: {
            (void)read_l((const uint8_t **)&tb_ptr);
            break;
        }

        default:
            fprintf(stderr, "TCI: Unhandled opcode %d\n", opc);
            g_assert_not_reached();
        }
    }
}
