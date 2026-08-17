/*
 * Tiny Code Generator for QEMU — TCI interpreter target implementation
 *
 * Ported for Unicorn engine TCI mode.
 * Emits bytecode stream into code_gen_buffer for tcg_qemu_tb_exec interpreter.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "tcg/tcg.h"

static const TCGTargetOpDef *tcg_target_op_def(TCGOpcode op)
{
    static const TCGTargetOpDef r1 = { .args_ct_str = { "r" } };
    static const TCGTargetOpDef r2 = { .args_ct_str = { "r", "r" } };
    static const TCGTargetOpDef r3 = { .args_ct_str = { "r", "r", "r" } };
    static const TCGTargetOpDef r4 = { .args_ct_str = { "r", "r", "r", "r" } };
    static const TCGTargetOpDef r5 = { .args_ct_str = { "r", "r", "r", "r", "r" } };
    static const TCGTargetOpDef r6 = { .args_ct_str = { "r", "r", "r", "r", "r", "r" } };

    switch (op) {
    case INDEX_op_goto_tb:
    case INDEX_op_br:
    case INDEX_op_exit_tb:
    case INDEX_op_goto_ptr:
        return &r1;

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld_i32:
    case INDEX_op_ld8u_i64:
    case INDEX_op_ld8s_i64:
    case INDEX_op_ld16u_i64:
    case INDEX_op_ld16s_i64:
    case INDEX_op_ld32u_i64:
    case INDEX_op_ld32s_i64:
    case INDEX_op_ld_i64:
    case INDEX_op_st8_i32:
    case INDEX_op_st16_i32:
    case INDEX_op_st_i32:
    case INDEX_op_st8_i64:
    case INDEX_op_st16_i64:
    case INDEX_op_st32_i64:
    case INDEX_op_st_i64:
    case INDEX_op_mov_i32:
    case INDEX_op_mov_i64:
    case INDEX_op_ext8s_i32:
    case INDEX_op_ext16s_i32:
    case INDEX_op_ext8u_i32:
    case INDEX_op_ext16u_i32:
    case INDEX_op_ext8s_i64:
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext32s_i64:
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext32u_i64:
    case INDEX_op_ext_i32_i64:
    case INDEX_op_extu_i32_i64:
    case INDEX_op_bswap16_i32:
    case INDEX_op_bswap32_i32:
    case INDEX_op_bswap16_i64:
    case INDEX_op_bswap32_i64:
    case INDEX_op_bswap64_i64:
    case INDEX_op_not_i32:
    case INDEX_op_neg_i32:
    case INDEX_op_not_i64:
    case INDEX_op_neg_i64:
    case INDEX_op_ctpop_i32:
    case INDEX_op_ctpop_i64:
        return &r2;

    case INDEX_op_add_i32:
    case INDEX_op_sub_i32:
    case INDEX_op_mul_i32:
    case INDEX_op_and_i32:
    case INDEX_op_or_i32:
    case INDEX_op_xor_i32:
    case INDEX_op_andc_i32:
    case INDEX_op_orc_i32:
    case INDEX_op_eqv_i32:
    case INDEX_op_nand_i32:
    case INDEX_op_nor_i32:
    case INDEX_op_shl_i32:
    case INDEX_op_shr_i32:
    case INDEX_op_sar_i32:
    case INDEX_op_rotl_i32:
    case INDEX_op_rotr_i32:
    case INDEX_op_clz_i32:
    case INDEX_op_ctz_i32:
    case INDEX_op_add_i64:
    case INDEX_op_sub_i64:
    case INDEX_op_mul_i64:
    case INDEX_op_and_i64:
    case INDEX_op_or_i64:
    case INDEX_op_xor_i64:
    case INDEX_op_andc_i64:
    case INDEX_op_orc_i64:
    case INDEX_op_eqv_i64:
    case INDEX_op_nand_i64:
    case INDEX_op_nor_i64:
    case INDEX_op_shl_i64:
    case INDEX_op_shr_i64:
    case INDEX_op_sar_i64:
    case INDEX_op_rotl_i64:
    case INDEX_op_rotr_i64:
    case INDEX_op_clz_i64:
    case INDEX_op_ctz_i64:
    case INDEX_op_div_i32:
    case INDEX_op_divu_i32:
    case INDEX_op_rem_i32:
    case INDEX_op_remu_i32:
    case INDEX_op_div_i64:
    case INDEX_op_divu_i64:
    case INDEX_op_rem_i64:
    case INDEX_op_remu_i64:
    case INDEX_op_qemu_ld_i32:
    case INDEX_op_qemu_ld_i64:
    case INDEX_op_qemu_st_i32:
    case INDEX_op_qemu_st_i64:
        return &r3;

    case INDEX_op_mulu2_i32:
    case INDEX_op_muls2_i32:
    case INDEX_op_mulu2_i64:
    case INDEX_op_muls2_i64:
    case INDEX_op_extract_i32:
    case INDEX_op_extract_i64:
    case INDEX_op_sextract_i32:
    case INDEX_op_sextract_i64:
    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
        return &r4;

    case INDEX_op_deposit_i32:
    case INDEX_op_deposit_i64:
    case INDEX_op_setcond2_i32:
        return &r5;

    case INDEX_op_movcond_i32:
    case INDEX_op_movcond_i64:
    case INDEX_op_add2_i32:
    case INDEX_op_sub2_i32:
    case INDEX_op_add2_i64:
    case INDEX_op_sub2_i64:
    case INDEX_op_brcond2_i32:
        return &r6;

    default:
        return &r6;
    }
}

static const char *target_parse_constraint(TCGArgConstraint *ct,
                                           const char *ct_str,
                                           TCGType type)
{
    switch (*ct_str++) {
    case 'r':
    case 'l':
        ct->ct |= TCG_CT_REG;
        ct->u.regs = 0xffff;
        break;
    case 'i':
        ct->ct |= TCG_CT_CONST;
        break;
    default:
        return NULL;
    }
    return ct_str;
}

static int tcg_target_const_match(tcg_target_long val, TCGType type,
                                  const TCGArgConstraint *arg_ct)
{
    if (arg_ct->ct & TCG_CT_CONST) {
        return 1;
    }
    return 0;
}

static const int tcg_target_reg_alloc_order[] = {
    TCG_REG_R1,  TCG_REG_R2,  TCG_REG_R3,  TCG_REG_R4,
    TCG_REG_R5,  TCG_REG_R6,  TCG_REG_R7,  TCG_REG_R8,
    TCG_REG_R9,  TCG_REG_R10, TCG_REG_R11, TCG_REG_R12,
    TCG_REG_R13, TCG_REG_R14
};

static const int tcg_target_call_iarg_regs[] = {
    TCG_REG_R1, TCG_REG_R2, TCG_REG_R3, TCG_REG_R4,
    TCG_REG_R5, TCG_REG_R6, TCG_REG_R7
};

static const int tcg_target_call_oarg_regs[] = {
    TCG_REG_R1
};

static inline void tcg_out_b(TCGContext *s, uint8_t v)
{
    *s->code_ptr++ = v;
}

static inline void tcg_out_l(TCGContext *s, uint64_t v)
{
    memcpy(s->code_ptr, &v, sizeof(v));
    s->code_ptr += sizeof(v);
}

static inline void tcg_out_r(TCGContext *s, TCGReg r)
{
    tcg_out_b(s, (uint8_t)r);
}

static bool patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    value += addend;
    memcpy(code_ptr, &value, sizeof(value));
    return true;
}

static bool tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    if (ret != arg) {
        tcg_out_b(s, type == TCG_TYPE_I32 ? INDEX_op_mov_i32 : INDEX_op_mov_i64);
        tcg_out_r(s, ret);
        tcg_out_r(s, arg);
    }
    return true;
}

static inline void tcg_out_movi(TCGContext *s, TCGType type,
                                TCGReg ret, tcg_target_long arg)
{
    tcg_out_b(s, type == TCG_TYPE_I32 ? INDEX_op_movi_i32 : INDEX_op_movi_i64);
    tcg_out_r(s, ret);
    tcg_out_l(s, (uint64_t)arg);
}

static inline void tcg_out_ld(TCGContext *s, TCGType type, TCGReg ret,
                              TCGReg arg1, intptr_t arg2)
{
    tcg_out_b(s, type == TCG_TYPE_I32 ? INDEX_op_ld_i32 : INDEX_op_ld_i64);
    tcg_out_r(s, ret);
    tcg_out_r(s, arg1);
    tcg_out_l(s, (uint64_t)arg2);
}

static inline void tcg_out_st(TCGContext *s, TCGType type, TCGReg arg,
                              TCGReg arg1, intptr_t arg2)
{
    tcg_out_b(s, type == TCG_TYPE_I32 ? INDEX_op_st_i32 : INDEX_op_st_i64);
    tcg_out_r(s, arg);
    tcg_out_r(s, arg1);
    tcg_out_l(s, (uint64_t)arg2);
}

static bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                               TCGReg base, intptr_t ofs)
{
    return false;
}

static inline void tcg_out_call(TCGContext *s, tcg_insn_unit *func_addr)
{
    tcg_out_b(s, INDEX_op_call);
    tcg_out_l(s, (uintptr_t)func_addr);
}

static void tcg_out_op(TCGContext *s, TCGOpcode opc,
                       const TCGArg *args, const int *const_args)
{
    tcg_out_b(s, opc);
    switch (opc) {
    case INDEX_op_exit_tb:
        tcg_out_l(s, args[0]);
        break;
    case INDEX_op_goto_tb:
        if (s->tb_jmp_insn_offset) {
            s->tb_jmp_insn_offset[args[0]] = tcg_current_code_size(s);
        } else {
            s->tb_jmp_target_addr[args[0]] = (uintptr_t)s->code_ptr;
        }
        tcg_out_l(s, 0);
        break;
    case INDEX_op_br:
        tcg_out_reloc(s, s->code_ptr, 0, arg_label(args[0]), 0);
        tcg_out_l(s, 0);
        break;

    case INDEX_op_ld8u_i32:
    case INDEX_op_ld8s_i32:
    case INDEX_op_ld16u_i32:
    case INDEX_op_ld16s_i32:
    case INDEX_op_ld_i32:
    case INDEX_op_ld8u_i64:
    case INDEX_op_ld8s_i64:
    case INDEX_op_ld16u_i64:
    case INDEX_op_ld16s_i64:
    case INDEX_op_ld32u_i64:
    case INDEX_op_ld32s_i64:
    case INDEX_op_ld_i64:
    case INDEX_op_st8_i32:
    case INDEX_op_st16_i32:
    case INDEX_op_st_i32:
    case INDEX_op_st8_i64:
    case INDEX_op_st16_i64:
    case INDEX_op_st32_i64:
    case INDEX_op_st_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_l(s, args[2]);
        break;

    case INDEX_op_add_i32:
    case INDEX_op_sub_i32:
    case INDEX_op_mul_i32:
    case INDEX_op_and_i32:
    case INDEX_op_or_i32:
    case INDEX_op_xor_i32:
    case INDEX_op_andc_i32:
    case INDEX_op_orc_i32:
    case INDEX_op_eqv_i32:
    case INDEX_op_nand_i32:
    case INDEX_op_nor_i32:
    case INDEX_op_shl_i32:
    case INDEX_op_shr_i32:
    case INDEX_op_sar_i32:
    case INDEX_op_rotl_i32:
    case INDEX_op_rotr_i32:
    case INDEX_op_clz_i32:
    case INDEX_op_ctz_i32:
    case INDEX_op_add_i64:
    case INDEX_op_sub_i64:
    case INDEX_op_mul_i64:
    case INDEX_op_and_i64:
    case INDEX_op_or_i64:
    case INDEX_op_xor_i64:
    case INDEX_op_andc_i64:
    case INDEX_op_orc_i64:
    case INDEX_op_eqv_i64:
    case INDEX_op_nand_i64:
    case INDEX_op_nor_i64:
    case INDEX_op_shl_i64:
    case INDEX_op_shr_i64:
    case INDEX_op_sar_i64:
    case INDEX_op_rotl_i64:
    case INDEX_op_rotr_i64:
    case INDEX_op_clz_i64:
    case INDEX_op_ctz_i64:
    case INDEX_op_div_i32:
    case INDEX_op_divu_i32:
    case INDEX_op_rem_i32:
    case INDEX_op_remu_i32:
    case INDEX_op_div_i64:
    case INDEX_op_divu_i64:
    case INDEX_op_rem_i64:
    case INDEX_op_remu_i64:
    case INDEX_op_muluh_i32:
    case INDEX_op_mulsh_i32:
    case INDEX_op_muluh_i64:
    case INDEX_op_mulsh_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        break;

    case INDEX_op_setcond_i32:
    case INDEX_op_setcond_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        tcg_out_b(s, args[3]); // cond
        break;

    case INDEX_op_deposit_i32:
    case INDEX_op_deposit_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        tcg_out_b(s, args[3]);
        tcg_out_b(s, args[4]);
        break;

    case INDEX_op_movcond_i32:
    case INDEX_op_movcond_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        tcg_out_r(s, args[3]);
        tcg_out_r(s, args[4]);
        tcg_out_b(s, args[5]);
        break;

    case INDEX_op_add2_i32:
    case INDEX_op_sub2_i32:
    case INDEX_op_add2_i64:
    case INDEX_op_sub2_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        tcg_out_r(s, args[3]);
        tcg_out_r(s, args[4]);
        tcg_out_r(s, args[5]);
        break;

    case INDEX_op_mulu2_i32:
    case INDEX_op_muls2_i32:
    case INDEX_op_mulu2_i64:
    case INDEX_op_muls2_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_r(s, args[2]);
        tcg_out_r(s, args[3]);
        break;

    case INDEX_op_qemu_ld_i32:
    case INDEX_op_qemu_ld_i64:
    case INDEX_op_qemu_st_i32:
    case INDEX_op_qemu_st_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_l(s, args[2]); // oi
        break;

    case INDEX_op_ext8s_i32:
    case INDEX_op_ext16s_i32:
    case INDEX_op_ext8u_i32:
    case INDEX_op_ext16u_i32:
    case INDEX_op_ext8s_i64:
    case INDEX_op_ext16s_i64:
    case INDEX_op_ext32s_i64:
    case INDEX_op_ext8u_i64:
    case INDEX_op_ext16u_i64:
    case INDEX_op_ext32u_i64:
    case INDEX_op_ext_i32_i64:
    case INDEX_op_extu_i32_i64:
    case INDEX_op_bswap16_i32:
    case INDEX_op_bswap32_i32:
    case INDEX_op_bswap16_i64:
    case INDEX_op_bswap32_i64:
    case INDEX_op_bswap64_i64:
    case INDEX_op_not_i32:
    case INDEX_op_neg_i32:
    case INDEX_op_not_i64:
    case INDEX_op_neg_i64:
    case INDEX_op_ctpop_i32:
    case INDEX_op_ctpop_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        break;

    case INDEX_op_brcond_i32:
    case INDEX_op_brcond_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_b(s, args[2]);
        tcg_out_reloc(s, s->code_ptr, 0, arg_label(args[3]), 0);
        tcg_out_l(s, 0);
        break;

    case INDEX_op_extract_i32:
    case INDEX_op_extract_i64:
    case INDEX_op_sextract_i32:
    case INDEX_op_sextract_i64:
        tcg_out_r(s, args[0]);
        tcg_out_r(s, args[1]);
        tcg_out_b(s, args[2]);
        tcg_out_b(s, args[3]);
        break;

    case INDEX_op_goto_ptr:
        tcg_out_r(s, args[0]);
        break;

    case INDEX_op_mb:
        tcg_out_l(s, args[0]);
        break;

    default:
        fprintf(stderr, "TCI codegen: unhandled opc %d\n", opc);
        tcg_abort();
    }
}

static void tcg_target_init(TCGContext *s)
{
    s->tcg_target_available_regs[TCG_TYPE_I32] = 0xffff;
    s->tcg_target_available_regs[TCG_TYPE_I64] = 0xffff;

    s->tcg_target_call_clobber_regs = 0;
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R1);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R2);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R3);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R4);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R5);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R6);
    tcg_regset_set_reg(s->tcg_target_call_clobber_regs, TCG_REG_R7);

    s->reserved_regs = 0;
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_R0);
    tcg_regset_set_reg(s->reserved_regs, TCG_REG_R15);
}

static void tcg_target_qemu_prologue(TCGContext *s)
{
    tcg_set_frame(s, TCG_REG_R15, 0, CPU_TEMP_BUF_NLONGS * sizeof(long));
}
