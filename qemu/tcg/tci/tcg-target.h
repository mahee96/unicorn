/*
 * Tiny Code Generator for QEMU — TCI interpreter target
 *
 * Ported from QEMU 5.x for use in Unicorn on iOS (no-JIT / W^X).
 * All TCG ops are executed by a C software interpreter; no native
 * machine code is ever written to the code_gen_buffer.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef TCG_TCI_TARGET_H
#define TCG_TCI_TARGET_H

/*
 * HAVE_TCG_QEMU_TB_EXEC tells tcg.h to expose
 *   uintptr_t tcg_qemu_tb_exec(CPUArchState *env, uint8_t *tb_ptr);
 * as a real C function (defined in tci.c) rather than using the
 * macro that casts code_gen_prologue to a function pointer and calls
 * it as native machine code.
 */
#define HAVE_TCG_QEMU_TB_EXEC

/*
 * Bytecode unit size = 1 byte.  All multi-byte fields are emitted /
 * read with memcpy so alignment is never required.
 */
#define TCG_TARGET_INSN_UNIT_SIZE        1
#define TCG_TARGET_TLB_DISPLACEMENT_BITS 32

#undef TCG_TARGET_STACK_GROWSUP

/*
 * 16 virtual 64-bit registers.
 *   R0  = env pointer (TCG_AREG0)
 *   R1–R7  = call argument / caller-saved scratch
 *   R8–R14 = callee-saved general purpose
 *   R15 = virtual frame / stack pointer
 */
typedef enum {
    TCG_REG_R0  = 0,
    TCG_REG_R1  = 1,
    TCG_REG_R2  = 2,
    TCG_REG_R3  = 3,
    TCG_REG_R4  = 4,
    TCG_REG_R5  = 5,
    TCG_REG_R6  = 6,
    TCG_REG_R7  = 7,
    TCG_REG_R8  = 8,
    TCG_REG_R9  = 9,
    TCG_REG_R10 = 10,
    TCG_REG_R11 = 11,
    TCG_REG_R12 = 12,
    TCG_REG_R13 = 13,
    TCG_REG_R14 = 14,
    TCG_REG_R15 = 15,
} TCGReg;

#define TCG_TARGET_NB_REGS 16
#define TCG_AREG0          TCG_REG_R0

/* Calling convention */
#define TCG_REG_CALL_STACK           TCG_REG_R15
#define TCG_TARGET_STACK_ALIGN       16
#define TCG_TARGET_CALL_ALIGN_ARGS   1
#define TCG_TARGET_CALL_STACK_OFFSET 0

/* ---- 32-bit optional ops: all supported by software interpreter ---- */
#define TCG_TARGET_HAS_div_i32       1
#define TCG_TARGET_HAS_rem_i32       1
#define TCG_TARGET_HAS_ext8s_i32     1
#define TCG_TARGET_HAS_ext16s_i32    1
#define TCG_TARGET_HAS_ext8u_i32     1
#define TCG_TARGET_HAS_ext16u_i32    1
#define TCG_TARGET_HAS_bswap16_i32   1
#define TCG_TARGET_HAS_bswap32_i32   1
#define TCG_TARGET_HAS_not_i32       1
#define TCG_TARGET_HAS_neg_i32       1
#define TCG_TARGET_HAS_rot_i32       1
#define TCG_TARGET_HAS_andc_i32      1
#define TCG_TARGET_HAS_orc_i32       1
#define TCG_TARGET_HAS_eqv_i32       1
#define TCG_TARGET_HAS_nand_i32      1
#define TCG_TARGET_HAS_nor_i32       1
#define TCG_TARGET_HAS_clz_i32       1
#define TCG_TARGET_HAS_ctz_i32       1
#define TCG_TARGET_HAS_ctpop_i32     1
#define TCG_TARGET_HAS_deposit_i32   1
#define TCG_TARGET_HAS_extract_i32   1
#define TCG_TARGET_HAS_sextract_i32  1
#define TCG_TARGET_HAS_extract2_i32  0
#define TCG_TARGET_HAS_movcond_i32   1
#define TCG_TARGET_HAS_add2_i32      1
#define TCG_TARGET_HAS_sub2_i32      1
#define TCG_TARGET_HAS_mulu2_i32     1
#define TCG_TARGET_HAS_muls2_i32     1
#define TCG_TARGET_HAS_muluh_i32     1
#define TCG_TARGET_HAS_mulsh_i32     1
#define TCG_TARGET_HAS_extrl_i64_i32 0
#define TCG_TARGET_HAS_extrh_i64_i32 0
#define TCG_TARGET_HAS_div2_i32      0
#define TCG_TARGET_HAS_goto_ptr      1

/* ---- 64-bit optional ops ---- */
#define TCG_TARGET_HAS_div_i64       1
#define TCG_TARGET_HAS_rem_i64       1
#define TCG_TARGET_HAS_ext8s_i64     1
#define TCG_TARGET_HAS_ext16s_i64    1
#define TCG_TARGET_HAS_ext32s_i64    1
#define TCG_TARGET_HAS_ext8u_i64     1
#define TCG_TARGET_HAS_ext16u_i64    1
#define TCG_TARGET_HAS_ext32u_i64    1
#define TCG_TARGET_HAS_bswap16_i64   1
#define TCG_TARGET_HAS_bswap32_i64   1
#define TCG_TARGET_HAS_bswap64_i64   1
#define TCG_TARGET_HAS_not_i64       1
#define TCG_TARGET_HAS_neg_i64       1
#define TCG_TARGET_HAS_rot_i64       1
#define TCG_TARGET_HAS_andc_i64      1
#define TCG_TARGET_HAS_orc_i64       1
#define TCG_TARGET_HAS_eqv_i64       1
#define TCG_TARGET_HAS_nand_i64      1
#define TCG_TARGET_HAS_nor_i64       1
#define TCG_TARGET_HAS_clz_i64       1
#define TCG_TARGET_HAS_ctz_i64       1
#define TCG_TARGET_HAS_ctpop_i64     1
#define TCG_TARGET_HAS_deposit_i64   1
#define TCG_TARGET_HAS_extract_i64   1
#define TCG_TARGET_HAS_sextract_i64  1
#define TCG_TARGET_HAS_extract2_i64  0
#define TCG_TARGET_HAS_movcond_i64   1
#define TCG_TARGET_HAS_add2_i64      1
#define TCG_TARGET_HAS_sub2_i64      1
#define TCG_TARGET_HAS_mulu2_i64     1
#define TCG_TARGET_HAS_muls2_i64     1
#define TCG_TARGET_HAS_muluh_i64     1
#define TCG_TARGET_HAS_mulsh_i64     1
#define TCG_TARGET_HAS_direct_jump   0   /* TB slots patched via jmp_target_addr */
#define TCG_TARGET_HAS_div2_i64      0

/* No host vector registers in TCI — handled by tcg.h fallback */

/* Memory model: emit barriers as needed (softmmu handles ordering) */
#define TCG_TARGET_DEFAULT_MO        (TCG_MO_ALL)
#define TCG_TARGET_HAS_MEMORY_BSWAP  0

/*
 * No i-cache flush needed — TCI never writes executable machine code.
 * The bytecode buffer is plain data.
 */
static inline void flush_icache_range(uintptr_t start, uintptr_t stop)
{
    (void)start;
    (void)stop;
}

/*
 * TB jump-target patching: writes the target bytecode pointer into
 * the 8-byte goto_tb slot.  Defined in tci.c.
 */
void tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_addr,
                              uintptr_t addr);

#endif /* TCG_TCI_TARGET_H */
