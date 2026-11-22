/* Generated automatically by the program `genflags'
   from the machine description file `md'.  */

#ifndef GCC_INSN_FLAGS_H
#define GCC_INSN_FLAGS_H

#define HAVE_fmaxhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fmaxsf3 (CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_fmaxdf3 (CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_fminhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fminsf3 (CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_fmindf3 (CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_lfloorhfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceilhfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrinthfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lfloorunshfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceilunshfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrintunshfsi2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lfloorsfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceilsfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrintsfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lfloorunssfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceilunssfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrintunssfsi2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lfloordfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceildfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrintdfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lfloorunsdfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lceilunsdfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_lrintunsdfsi2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_floorhf2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_ceilhf2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_btrunchf2 (CSKY_ISA_FEATURE(fpv3_hf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_floorsf2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_ceilsf2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_btruncsf2 (CSKY_ISA_FEATURE(fpv3_sf) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_floordf2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_ceildf2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_btruncdf2 (CSKY_ISA_FEATURE(fpv3_df) \
   && (flag_fp_int_builtin_inexact || !flag_trapping_math))
#define HAVE_csky_setfcrsi (CSKY_ISA_FEATURE(fcr))
#define HAVE_csky_getfcrsi (CSKY_ISA_FEATURE(fcr))
#define HAVE_csky_insfcrsi (CSKY_ISA_FEATURE(fcr))
#define HAVE_mulsidi3 (TARGET_DSP)
#define HAVE_umulsidi3 (TARGET_DSP)
#define HAVE_maddsidi4 (TARGET_DSP)
#define HAVE_umaddsidi4 (TARGET_DSP)
#define HAVE_msubsidi4 (TARGET_DSP)
#define HAVE_umsubsidi4 (TARGET_DSP)
#define HAVE_movt (CSKY_ISA_FEATURE (E2))
#define HAVE_movf (CSKY_ISA_FEATURE (E2))
#define HAVE_mvc (CSKY_ISA_FEATURE (E2))
#define HAVE_mvcv 1
#define HAVE_abssi2 (CSKY_ISA_FEATURE (2E3))
#define HAVE_extvsi (CSKY_ISA_FEATURE (2E3))
#define HAVE_insvsi (CSKY_ISA_FEATURE (2E3))
#define HAVE_smart_bseti (CSKY_ISA_FEATURE (E1))
#define HAVE_fast_bseti (CSKY_ISA_FEATURE (E2))
#define HAVE_smart_bclri (CSKY_ISA_FEATURE (E1))
#define HAVE_fast_bclri (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_ashlsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_ck801_lshrsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_smart_addsi3 (TARGET_MINI_REGISTERS && CSKY_ISA_FEATURE (E2) \
  && operands[0] != stack_pointer_rtx \
  && operands[1] != stack_pointer_rtx)
#define HAVE_fast_addsi3 (!TARGET_MINI_REGISTERS && CSKY_ISA_FEATURE (E2))
#define HAVE_smart_subsi3 (TARGET_MINI_REGISTERS && CSKY_ISA_FEATURE (E2) \
   && operands[0] != stack_pointer_rtx \
   && operands[1] != stack_pointer_rtx)
#define HAVE_fast_subsi3 (!TARGET_MINI_REGISTERS && CSKY_ISA_FEATURE (E2))
#define HAVE_cskyv2_addc (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_addc (CSKY_ISA_FEATURE (E1))
#define HAVE_cskyv2_subc (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_subc (CSKY_ISA_FEATURE (E1))
#define HAVE_mulhisi3 (CSKY_ISA_FEATURE (2E3))
#define HAVE_cskyv2_addcc (CSKY_ISA_FEATURE (E2))
#define HAVE_cskyv2_addcc_invert (CSKY_ISA_FEATURE (E2))
#define HAVE_cskyv2_extzv (CSKY_ISA_FEATURE (2E3))
#define HAVE_zero_extendhisi2 1
#define HAVE_zero_extendqisi2 1
#define HAVE_zero_extendqihi2 1
#define HAVE_clzsi2 (CSKY_ISA_FEATURE (E2))
#define HAVE_cskyv2_one_cmplsi2 (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_one_cmplsi2 (CSKY_ISA_FEATURE (E1))
#define HAVE_extendhisi2 1
#define HAVE_extendqisi2 1
#define HAVE_extendqihi2 1
#define HAVE_cskyv2_andsi3 (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_andsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_cskyv2_andnsi3 (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_andnsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_cskyv2_iorsi3 (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_iorsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_cskyv2_xorsi3 (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_xorsi3 (CSKY_ISA_FEATURE (E1))
#define HAVE_divsi3 (TARGET_DIV)
#define HAVE_udivsi3 (TARGET_DIV)
#define HAVE_indirect_jump 1
#define HAVE_csky_jbt (CSKY_ISA_FEATURE (2E3))
#define HAVE_csky_jbf (CSKY_ISA_FEATURE (2E3))
#define HAVE_ck802_jbt (CSKY_ISA_FEATURE (E2))
#define HAVE_ck802_jbf (CSKY_ISA_FEATURE (E2))
#define HAVE_ck801_jbt (CSKY_ISA_FEATURE (E1))
#define HAVE_ck801_jbf (CSKY_ISA_FEATURE (E1))
#define HAVE_simple_return (reload_completed)
#define HAVE_csky_eh_return 1
#define HAVE_smart_cmpnesi_i (TARGET_MINI_REGISTERS)
#define HAVE_fast_cmpnesi_i (CSKY_ISA_FEATURE (E2))
#define HAVE_cmpltsi_r 1
#define HAVE_cmpgeusi_r 1
#define HAVE_blockage 1
#define HAVE_prologue_get_pc ((GET_CODE (operands[0]) == UNSPEC) \
   && (XINT (operands[0], 1) == UNSPEC_PIC_SYMBOL_GOTPC_GRS))
#define HAVE_nop 1
#define HAVE_trap 1
#define HAVE_align_4 1
#define HAVE_csky_constpool_label 1
#define HAVE_consttable_4 1
#define HAVE_consttable_8 1
#define HAVE_csky_casesi_dispatch 1
#define HAVE_bswapsi2 (CSKY_ISA_FEATURE (E2))
#define HAVE_movhf (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_movsf (CSKY_ISA_FEATURE(fpv2_sf) \
   || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_movdf (CSKY_ISA_FEATURE(fpv2_df) \
   || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_mulhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_mulsf3 (CSKY_ISA_FEATURE(fpv2_sf) \
  || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_muldf3 (CSKY_ISA_FEATURE(fpv2_df) \
  || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_fmahf4 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fmasf4 (CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_fmadf4 (CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_addhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_addsf3 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_adddf3 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_subhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_subsf3 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_subdf3 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_abshf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_abssf2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_absdf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_neghf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_negsf2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_negdf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_sqrthf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_sqrtsf2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_sqrtdf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_divsf3 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_divdf3 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_divhf3 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_cbranchsf4 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_cbranchdf4 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_cbranchhf4 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_cstoresf4 (CSKY_ISA_FEATURE (fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_cstoredf4 (CSKY_ISA_FEATURE (fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_cstorehf4 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_extendhfsf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_truncsfhf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_extendsfdf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_truncdfsf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_floatsihf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_floatunssihf2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_floatsisf2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_floatunssisf2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_floatsidf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_floatunssidf2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_floathihf2 (CSKY_ISA_FEATURE(fpv3_hi) && CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_floatunshihf2 (CSKY_ISA_FEATURE(fpv3_hi) && CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fix_trunchfsi2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fixuns_trunchfsi2 (CSKY_ISA_FEATURE(fpv3_hf))
#define HAVE_fix_truncsfsi2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_fixuns_truncsfsi2 (CSKY_ISA_FEATURE(fpv2_sf) || CSKY_ISA_FEATURE(fpv3_sf))
#define HAVE_fix_truncdfsi2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_fixuns_truncdfsi2 (CSKY_ISA_FEATURE(fpv2_df) || CSKY_ISA_FEATURE(fpv3_df))
#define HAVE_movsi 1
#define HAVE_movhi 1
#define HAVE_movqi 1
#define HAVE_movdi 1
#define HAVE_movsicc (CSKY_ISA_FEATURE (E2))
#define HAVE_cstoresi4 1
#define HAVE_bseti 1
#define HAVE_bclri 1
#define HAVE_ashlsi3 1
#define HAVE_ashrsi3 1
#define HAVE_lshrsi3 1
#define HAVE_rotlsi3 1
#define HAVE_addsi3 1
#define HAVE_adddi3 1
#define HAVE_subsi3 1
#define HAVE_subdi3 1
#define HAVE_mulsi3 1
#define HAVE_addsicc (CSKY_ISA_FEATURE (E2))
#define HAVE_extzvsi 1
#define HAVE_one_cmplsi2 1
#define HAVE_extendsidi2 1
#define HAVE_andsi3 1
#define HAVE_anddi3 1
#define HAVE_iorsi3 1
#define HAVE_iordi3 1
#define HAVE_xorsi3 1
#define HAVE_xordi3 1
#define HAVE_load_multiple (TARGET_MULTIPLE_STLD)
#define HAVE_store_multiple (TARGET_MULTIPLE_STLD)
#define HAVE_tablejump 1
#define HAVE_jump 1
#define HAVE_cbranchsi4 1
#define HAVE_eh_return 1
#define HAVE_call 1
#define HAVE_call_value 1
#define HAVE_untyped_call 1
#define HAVE_prologue 1
#define HAVE_epilogue 1
#define HAVE_builtin_setjmp_receiver (flag_pic)
#define HAVE_casesi (TARGET_CASESI)
#define HAVE_csky_casesi_internal 1
extern rtx        gen_fmaxhf3                 (rtx, rtx, rtx);
extern rtx        gen_fmaxsf3                 (rtx, rtx, rtx);
extern rtx        gen_fmaxdf3                 (rtx, rtx, rtx);
extern rtx        gen_fminhf3                 (rtx, rtx, rtx);
extern rtx        gen_fminsf3                 (rtx, rtx, rtx);
extern rtx        gen_fmindf3                 (rtx, rtx, rtx);
extern rtx        gen_lfloorhfsi2             (rtx, rtx);
extern rtx        gen_lceilhfsi2              (rtx, rtx);
extern rtx        gen_lrinthfsi2              (rtx, rtx);
extern rtx        gen_lfloorunshfsi2          (rtx, rtx);
extern rtx        gen_lceilunshfsi2           (rtx, rtx);
extern rtx        gen_lrintunshfsi2           (rtx, rtx);
extern rtx        gen_lfloorsfsi2             (rtx, rtx);
extern rtx        gen_lceilsfsi2              (rtx, rtx);
extern rtx        gen_lrintsfsi2              (rtx, rtx);
extern rtx        gen_lfloorunssfsi2          (rtx, rtx);
extern rtx        gen_lceilunssfsi2           (rtx, rtx);
extern rtx        gen_lrintunssfsi2           (rtx, rtx);
extern rtx        gen_lfloordfsi2             (rtx, rtx);
extern rtx        gen_lceildfsi2              (rtx, rtx);
extern rtx        gen_lrintdfsi2              (rtx, rtx);
extern rtx        gen_lfloorunsdfsi2          (rtx, rtx);
extern rtx        gen_lceilunsdfsi2           (rtx, rtx);
extern rtx        gen_lrintunsdfsi2           (rtx, rtx);
extern rtx        gen_floorhf2                (rtx, rtx);
extern rtx        gen_ceilhf2                 (rtx, rtx);
extern rtx        gen_btrunchf2               (rtx, rtx);
extern rtx        gen_floorsf2                (rtx, rtx);
extern rtx        gen_ceilsf2                 (rtx, rtx);
extern rtx        gen_btruncsf2               (rtx, rtx);
extern rtx        gen_floordf2                (rtx, rtx);
extern rtx        gen_ceildf2                 (rtx, rtx);
extern rtx        gen_btruncdf2               (rtx, rtx);
extern rtx        gen_csky_setfcrsi           (rtx);
extern rtx        gen_csky_getfcrsi           (rtx);
extern rtx        gen_csky_insfcrsi           (rtx, rtx, rtx);
extern rtx        gen_mulsidi3                (rtx, rtx, rtx);
extern rtx        gen_umulsidi3               (rtx, rtx, rtx);
extern rtx        gen_maddsidi4               (rtx, rtx, rtx, rtx);
extern rtx        gen_umaddsidi4              (rtx, rtx, rtx, rtx);
extern rtx        gen_msubsidi4               (rtx, rtx, rtx, rtx);
extern rtx        gen_umsubsidi4              (rtx, rtx, rtx, rtx);
extern rtx        gen_movt                    (rtx, rtx, rtx);
extern rtx        gen_movf                    (rtx, rtx, rtx);
extern rtx        gen_mvc                     (rtx);
extern rtx        gen_mvcv                    (rtx);
extern rtx        gen_abssi2                  (rtx, rtx);
extern rtx        gen_extvsi                  (rtx, rtx, rtx, rtx);
extern rtx        gen_insvsi                  (rtx, rtx, rtx, rtx);
extern rtx        gen_smart_bseti             (rtx, rtx, rtx);
extern rtx        gen_fast_bseti              (rtx, rtx, rtx);
extern rtx        gen_smart_bclri             (rtx, rtx, rtx);
extern rtx        gen_fast_bclri              (rtx, rtx, rtx);
extern rtx        gen_ck801_ashlsi3           (rtx, rtx, rtx);
extern rtx        gen_ck801_lshrsi3           (rtx, rtx, rtx);
extern rtx        gen_smart_addsi3            (rtx, rtx, rtx);
extern rtx        gen_fast_addsi3             (rtx, rtx, rtx);
extern rtx        gen_smart_subsi3            (rtx, rtx, rtx);
extern rtx        gen_fast_subsi3             (rtx, rtx, rtx);
extern rtx        gen_cskyv2_addc             (rtx, rtx, rtx);
extern rtx        gen_ck801_addc              (rtx, rtx, rtx);
extern rtx        gen_cskyv2_subc             (rtx, rtx, rtx);
extern rtx        gen_ck801_subc              (rtx, rtx, rtx);
extern rtx        gen_mulhisi3                (rtx, rtx, rtx);
extern rtx        gen_cskyv2_addcc            (rtx, rtx, rtx);
extern rtx        gen_cskyv2_addcc_invert     (rtx, rtx, rtx);
extern rtx        gen_cskyv2_extzv            (rtx, rtx, rtx, rtx);
extern rtx        gen_zero_extendhisi2        (rtx, rtx);
extern rtx        gen_zero_extendqisi2        (rtx, rtx);
extern rtx        gen_zero_extendqihi2        (rtx, rtx);
extern rtx        gen_clzsi2                  (rtx, rtx);
extern rtx        gen_cskyv2_one_cmplsi2      (rtx, rtx);
extern rtx        gen_ck801_one_cmplsi2       (rtx, rtx);
extern rtx        gen_extendhisi2             (rtx, rtx);
extern rtx        gen_extendqisi2             (rtx, rtx);
extern rtx        gen_extendqihi2             (rtx, rtx);
extern rtx        gen_cskyv2_andsi3           (rtx, rtx, rtx);
extern rtx        gen_ck801_andsi3            (rtx, rtx, rtx);
extern rtx        gen_cskyv2_andnsi3          (rtx, rtx, rtx);
extern rtx        gen_ck801_andnsi3           (rtx, rtx, rtx);
extern rtx        gen_cskyv2_iorsi3           (rtx, rtx, rtx);
extern rtx        gen_ck801_iorsi3            (rtx, rtx, rtx);
extern rtx        gen_cskyv2_xorsi3           (rtx, rtx, rtx);
extern rtx        gen_ck801_xorsi3            (rtx, rtx, rtx);
extern rtx        gen_divsi3                  (rtx, rtx, rtx);
extern rtx        gen_udivsi3                 (rtx, rtx, rtx);
extern rtx        gen_indirect_jump           (rtx);
extern rtx        gen_csky_jbt                (rtx);
extern rtx        gen_csky_jbf                (rtx);
extern rtx        gen_ck802_jbt               (rtx);
extern rtx        gen_ck802_jbf               (rtx);
extern rtx        gen_ck801_jbt               (rtx);
extern rtx        gen_ck801_jbf               (rtx);
extern rtx        gen_simple_return           (void);
extern rtx        gen_csky_eh_return          (rtx);
extern rtx        gen_smart_cmpnesi_i         (rtx, rtx);
extern rtx        gen_fast_cmpnesi_i          (rtx, rtx);
extern rtx        gen_cmpltsi_r               (rtx, rtx);
extern rtx        gen_cmpgeusi_r              (rtx, rtx);
extern rtx        gen_blockage                (void);
extern rtx        gen_prologue_get_pc         (rtx);
extern rtx        gen_nop                     (void);
extern rtx        gen_trap                    (void);
extern rtx        gen_align_4                 (void);
extern rtx        gen_csky_constpool_label    (rtx);
extern rtx        gen_consttable_4            (rtx);
extern rtx        gen_consttable_8            (rtx);
extern rtx        gen_csky_casesi_dispatch    (rtx);
extern rtx        gen_bswapsi2                (rtx, rtx);
extern rtx        gen_movhf                   (rtx, rtx);
extern rtx        gen_movsf                   (rtx, rtx);
extern rtx        gen_movdf                   (rtx, rtx);
extern rtx        gen_mulhf3                  (rtx, rtx, rtx);
extern rtx        gen_mulsf3                  (rtx, rtx, rtx);
extern rtx        gen_muldf3                  (rtx, rtx, rtx);
extern rtx        gen_fmahf4                  (rtx, rtx, rtx, rtx);
extern rtx        gen_fmasf4                  (rtx, rtx, rtx, rtx);
extern rtx        gen_fmadf4                  (rtx, rtx, rtx, rtx);
extern rtx        gen_addhf3                  (rtx, rtx, rtx);
extern rtx        gen_addsf3                  (rtx, rtx, rtx);
extern rtx        gen_adddf3                  (rtx, rtx, rtx);
extern rtx        gen_subhf3                  (rtx, rtx, rtx);
extern rtx        gen_subsf3                  (rtx, rtx, rtx);
extern rtx        gen_subdf3                  (rtx, rtx, rtx);
extern rtx        gen_abshf2                  (rtx, rtx);
extern rtx        gen_abssf2                  (rtx, rtx);
extern rtx        gen_absdf2                  (rtx, rtx);
extern rtx        gen_neghf2                  (rtx, rtx);
extern rtx        gen_negsf2                  (rtx, rtx);
extern rtx        gen_negdf2                  (rtx, rtx);
extern rtx        gen_sqrthf2                 (rtx, rtx);
extern rtx        gen_sqrtsf2                 (rtx, rtx);
extern rtx        gen_sqrtdf2                 (rtx, rtx);
extern rtx        gen_divsf3                  (rtx, rtx, rtx);
extern rtx        gen_divdf3                  (rtx, rtx, rtx);
extern rtx        gen_divhf3                  (rtx, rtx, rtx);
extern rtx        gen_cbranchsf4              (rtx, rtx, rtx, rtx);
extern rtx        gen_cbranchdf4              (rtx, rtx, rtx, rtx);
extern rtx        gen_cbranchhf4              (rtx, rtx, rtx, rtx);
extern rtx        gen_cstoresf4               (rtx, rtx, rtx, rtx);
extern rtx        gen_cstoredf4               (rtx, rtx, rtx, rtx);
extern rtx        gen_cstorehf4               (rtx, rtx, rtx, rtx);
extern rtx        gen_extendhfsf2             (rtx, rtx);
extern rtx        gen_truncsfhf2              (rtx, rtx);
extern rtx        gen_extendsfdf2             (rtx, rtx);
extern rtx        gen_truncdfsf2              (rtx, rtx);
extern rtx        gen_floatsihf2              (rtx, rtx);
extern rtx        gen_floatunssihf2           (rtx, rtx);
extern rtx        gen_floatsisf2              (rtx, rtx);
extern rtx        gen_floatunssisf2           (rtx, rtx);
extern rtx        gen_floatsidf2              (rtx, rtx);
extern rtx        gen_floatunssidf2           (rtx, rtx);
extern rtx        gen_floathihf2              (rtx, rtx);
extern rtx        gen_floatunshihf2           (rtx, rtx);
extern rtx        gen_fix_trunchfsi2          (rtx, rtx);
extern rtx        gen_fixuns_trunchfsi2       (rtx, rtx);
extern rtx        gen_fix_truncsfsi2          (rtx, rtx);
extern rtx        gen_fixuns_truncsfsi2       (rtx, rtx);
extern rtx        gen_fix_truncdfsi2          (rtx, rtx);
extern rtx        gen_fixuns_truncdfsi2       (rtx, rtx);
extern rtx        gen_movsi                   (rtx, rtx);
extern rtx        gen_movhi                   (rtx, rtx);
extern rtx        gen_movqi                   (rtx, rtx);
extern rtx        gen_movdi                   (rtx, rtx);
extern rtx        gen_movsicc                 (rtx, rtx, rtx, rtx);
extern rtx        gen_cstoresi4               (rtx, rtx, rtx, rtx);
extern rtx        gen_bseti                   (rtx, rtx, rtx);
extern rtx        gen_bclri                   (rtx, rtx, rtx);
extern rtx        gen_ashlsi3                 (rtx, rtx, rtx);
extern rtx        gen_ashrsi3                 (rtx, rtx, rtx);
extern rtx        gen_lshrsi3                 (rtx, rtx, rtx);
extern rtx        gen_rotlsi3                 (rtx, rtx, rtx);
extern rtx        gen_addsi3                  (rtx, rtx, rtx);
extern rtx        gen_adddi3                  (rtx, rtx, rtx);
extern rtx        gen_subsi3                  (rtx, rtx, rtx);
extern rtx        gen_subdi3                  (rtx, rtx, rtx);
extern rtx        gen_mulsi3                  (rtx, rtx, rtx);
extern rtx        gen_addsicc                 (rtx, rtx, rtx, rtx);
extern rtx        gen_extzvsi                 (rtx, rtx, rtx, rtx);
extern rtx        gen_one_cmplsi2             (rtx, rtx);
extern rtx        gen_extendsidi2             (rtx, rtx);
extern rtx        gen_andsi3                  (rtx, rtx, rtx);
extern rtx        gen_anddi3                  (rtx, rtx, rtx);
extern rtx        gen_iorsi3                  (rtx, rtx, rtx);
extern rtx        gen_iordi3                  (rtx, rtx, rtx);
extern rtx        gen_xorsi3                  (rtx, rtx, rtx);
extern rtx        gen_xordi3                  (rtx, rtx, rtx);
extern rtx        gen_load_multiple           (rtx, rtx, rtx);
extern rtx        gen_store_multiple          (rtx, rtx, rtx);
extern rtx        gen_tablejump               (rtx, rtx);
extern rtx        gen_jump                    (rtx);
extern rtx        gen_cbranchsi4              (rtx, rtx, rtx, rtx);
extern rtx        gen_eh_return               (rtx);
extern rtx        gen_call                    (rtx, rtx);
extern rtx        gen_call_value              (rtx, rtx, rtx);
extern rtx        gen_untyped_call            (rtx, rtx, rtx);
extern rtx        gen_prologue                (void);
extern rtx        gen_epilogue                (void);
extern rtx        gen_builtin_setjmp_receiver (rtx);
extern rtx        gen_casesi                  (rtx, rtx, rtx, rtx, rtx);
extern rtx        gen_csky_casesi_internal    (rtx, rtx, rtx, rtx);

#endif /* GCC_INSN_FLAGS_H */
