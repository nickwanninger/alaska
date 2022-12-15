/* Unicorn Engine */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2015-2017 */
/* This file is released under LGPL2.
   See COPYING.LGPL2 in root directory for more details
*/

#ifndef UNICORN_PPC_H
#define UNICORN_PPC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4201)
#endif

//> PPC CPU
typedef enum uc_cpu_ppc {
    UC_CPU_PPC32_401 = 0,
    UC_CPU_PPC32_401A1,
    UC_CPU_PPC32_401B2,
    UC_CPU_PPC32_401C2,
    UC_CPU_PPC32_401D2,
    UC_CPU_PPC32_401E2,
    UC_CPU_PPC32_401F2,
    UC_CPU_PPC32_401G2,
    UC_CPU_PPC32_IOP480,
    UC_CPU_PPC32_COBRA,
    UC_CPU_PPC32_403GA,
    UC_CPU_PPC32_403GB,
    UC_CPU_PPC32_403GC,
    UC_CPU_PPC32_403GCX,
    UC_CPU_PPC32_405D2,
    UC_CPU_PPC32_405D4,
    UC_CPU_PPC32_405CRA,
    UC_CPU_PPC32_405CRB,
    UC_CPU_PPC32_405CRC,
    UC_CPU_PPC32_405EP,
    UC_CPU_PPC32_405EZ,
    UC_CPU_PPC32_405GPA,
    UC_CPU_PPC32_405GPB,
    UC_CPU_PPC32_405GPC,
    UC_CPU_PPC32_405GPD,
    UC_CPU_PPC32_405GPR,
    UC_CPU_PPC32_405LP,
    UC_CPU_PPC32_NPE405H,
    UC_CPU_PPC32_NPE405H2,
    UC_CPU_PPC32_NPE405L,
    UC_CPU_PPC32_NPE4GS3,
    UC_CPU_PPC32_STB03,
    UC_CPU_PPC32_STB04,
    UC_CPU_PPC32_STB25,
    UC_CPU_PPC32_X2VP4,
    UC_CPU_PPC32_X2VP20,
    UC_CPU_PPC32_440_XILINX,
    UC_CPU_PPC32_440_XILINX_W_DFPU,
    UC_CPU_PPC32_440EPA,
    UC_CPU_PPC32_440EPB,
    UC_CPU_PPC32_440EPX,
    UC_CPU_PPC32_460EXB,
    UC_CPU_PPC32_G2,
    UC_CPU_PPC32_G2H4,
    UC_CPU_PPC32_G2GP,
    UC_CPU_PPC32_G2LS,
    UC_CPU_PPC32_G2HIP3,
    UC_CPU_PPC32_G2HIP4,
    UC_CPU_PPC32_MPC603,
    UC_CPU_PPC32_G2LE,
    UC_CPU_PPC32_G2LEGP,
    UC_CPU_PPC32_G2LELS,
    UC_CPU_PPC32_G2LEGP1,
    UC_CPU_PPC32_G2LEGP3,
    UC_CPU_PPC32_MPC5200_V10,
    UC_CPU_PPC32_MPC5200_V11,
    UC_CPU_PPC32_MPC5200_V12,
    UC_CPU_PPC32_MPC5200B_V20,
    UC_CPU_PPC32_MPC5200B_V21,
    UC_CPU_PPC32_E200Z5,
    UC_CPU_PPC32_E200Z6,
    UC_CPU_PPC32_E300C1,
    UC_CPU_PPC32_E300C2,
    UC_CPU_PPC32_E300C3,
    UC_CPU_PPC32_E300C4,
    UC_CPU_PPC32_MPC8343,
    UC_CPU_PPC32_MPC8343A,
    UC_CPU_PPC32_MPC8343E,
    UC_CPU_PPC32_MPC8343EA,
    UC_CPU_PPC32_MPC8347T,
    UC_CPU_PPC32_MPC8347P,
    UC_CPU_PPC32_MPC8347AT,
    UC_CPU_PPC32_MPC8347AP,
    UC_CPU_PPC32_MPC8347ET,
    UC_CPU_PPC32_MPC8347EP,
    UC_CPU_PPC32_MPC8347EAT,
    UC_CPU_PPC32_MPC8347EAP,
    UC_CPU_PPC32_MPC8349,
    UC_CPU_PPC32_MPC8349A,
    UC_CPU_PPC32_MPC8349E,
    UC_CPU_PPC32_MPC8349EA,
    UC_CPU_PPC32_MPC8377,
    UC_CPU_PPC32_MPC8377E,
    UC_CPU_PPC32_MPC8378,
    UC_CPU_PPC32_MPC8378E,
    UC_CPU_PPC32_MPC8379,
    UC_CPU_PPC32_MPC8379E,
    UC_CPU_PPC32_E500_V10,
    UC_CPU_PPC32_E500_V20,
    UC_CPU_PPC32_E500V2_V10,
    UC_CPU_PPC32_E500V2_V20,
    UC_CPU_PPC32_E500V2_V21,
    UC_CPU_PPC32_E500V2_V22,
    UC_CPU_PPC32_E500V2_V30,
    UC_CPU_PPC32_E500MC,
    UC_CPU_PPC32_MPC8533_V10,
    UC_CPU_PPC32_MPC8533_V11,
    UC_CPU_PPC32_MPC8533E_V10,
    UC_CPU_PPC32_MPC8533E_V11,
    UC_CPU_PPC32_MPC8540_V10,
    UC_CPU_PPC32_MPC8540_V20,
    UC_CPU_PPC32_MPC8540_V21,
    UC_CPU_PPC32_MPC8541_V10,
    UC_CPU_PPC32_MPC8541_V11,
    UC_CPU_PPC32_MPC8541E_V10,
    UC_CPU_PPC32_MPC8541E_V11,
    UC_CPU_PPC32_MPC8543_V10,
    UC_CPU_PPC32_MPC8543_V11,
    UC_CPU_PPC32_MPC8543_V20,
    UC_CPU_PPC32_MPC8543_V21,
    UC_CPU_PPC32_MPC8543E_V10,
    UC_CPU_PPC32_MPC8543E_V11,
    UC_CPU_PPC32_MPC8543E_V20,
    UC_CPU_PPC32_MPC8543E_V21,
    UC_CPU_PPC32_MPC8544_V10,
    UC_CPU_PPC32_MPC8544_V11,
    UC_CPU_PPC32_MPC8544E_V10,
    UC_CPU_PPC32_MPC8544E_V11,
    UC_CPU_PPC32_MPC8545_V20,
    UC_CPU_PPC32_MPC8545_V21,
    UC_CPU_PPC32_MPC8545E_V20,
    UC_CPU_PPC32_MPC8545E_V21,
    UC_CPU_PPC32_MPC8547E_V20,
    UC_CPU_PPC32_MPC8547E_V21,
    UC_CPU_PPC32_MPC8548_V10,
    UC_CPU_PPC32_MPC8548_V11,
    UC_CPU_PPC32_MPC8548_V20,
    UC_CPU_PPC32_MPC8548_V21,
    UC_CPU_PPC32_MPC8548E_V10,
    UC_CPU_PPC32_MPC8548E_V11,
    UC_CPU_PPC32_MPC8548E_V20,
    UC_CPU_PPC32_MPC8548E_V21,
    UC_CPU_PPC32_MPC8555_V10,
    UC_CPU_PPC32_MPC8555_V11,
    UC_CPU_PPC32_MPC8555E_V10,
    UC_CPU_PPC32_MPC8555E_V11,
    UC_CPU_PPC32_MPC8560_V10,
    UC_CPU_PPC32_MPC8560_V20,
    UC_CPU_PPC32_MPC8560_V21,
    UC_CPU_PPC32_MPC8567,
    UC_CPU_PPC32_MPC8567E,
    UC_CPU_PPC32_MPC8568,
    UC_CPU_PPC32_MPC8568E,
    UC_CPU_PPC32_MPC8572,
    UC_CPU_PPC32_MPC8572E,
    UC_CPU_PPC32_E600,
    UC_CPU_PPC32_MPC8610,
    UC_CPU_PPC32_MPC8641,
    UC_CPU_PPC32_MPC8641D,
    UC_CPU_PPC32_601_V0,
    UC_CPU_PPC32_601_V1,
    UC_CPU_PPC32_601_V2,
    UC_CPU_PPC32_602,
    UC_CPU_PPC32_603,
    UC_CPU_PPC32_603E_V1_1,
    UC_CPU_PPC32_603E_V1_2,
    UC_CPU_PPC32_603E_V1_3,
    UC_CPU_PPC32_603E_V1_4,
    UC_CPU_PPC32_603E_V2_2,
    UC_CPU_PPC32_603E_V3,
    UC_CPU_PPC32_603E_V4,
    UC_CPU_PPC32_603E_V4_1,
    UC_CPU_PPC32_603E7,
    UC_CPU_PPC32_603E7T,
    UC_CPU_PPC32_603E7V,
    UC_CPU_PPC32_603E7V1,
    UC_CPU_PPC32_603E7V2,
    UC_CPU_PPC32_603P,
    UC_CPU_PPC32_604,
    UC_CPU_PPC32_604E_V1_0,
    UC_CPU_PPC32_604E_V2_2,
    UC_CPU_PPC32_604E_V2_4,
    UC_CPU_PPC32_604R,
    UC_CPU_PPC32_740_V1_0,
    UC_CPU_PPC32_750_V1_0,
    UC_CPU_PPC32_740_V2_0,
    UC_CPU_PPC32_750_V2_0,
    UC_CPU_PPC32_740_V2_1,
    UC_CPU_PPC32_750_V2_1,
    UC_CPU_PPC32_740_V2_2,
    UC_CPU_PPC32_750_V2_2,
    UC_CPU_PPC32_740_V3_0,
    UC_CPU_PPC32_750_V3_0,
    UC_CPU_PPC32_740_V3_1,
    UC_CPU_PPC32_750_V3_1,
    UC_CPU_PPC32_740E,
    UC_CPU_PPC32_750E,
    UC_CPU_PPC32_740P,
    UC_CPU_PPC32_750P,
    UC_CPU_PPC32_750CL_V1_0,
    UC_CPU_PPC32_750CL_V2_0,
    UC_CPU_PPC32_750CX_V1_0,
    UC_CPU_PPC32_750CX_V2_0,
    UC_CPU_PPC32_750CX_V2_1,
    UC_CPU_PPC32_750CX_V2_2,
    UC_CPU_PPC32_750CXE_V2_1,
    UC_CPU_PPC32_750CXE_V2_2,
    UC_CPU_PPC32_750CXE_V2_3,
    UC_CPU_PPC32_750CXE_V2_4,
    UC_CPU_PPC32_750CXE_V2_4B,
    UC_CPU_PPC32_750CXE_V3_0,
    UC_CPU_PPC32_750CXE_V3_1,
    UC_CPU_PPC32_750CXE_V3_1B,
    UC_CPU_PPC32_750CXR,
    UC_CPU_PPC32_750FL,
    UC_CPU_PPC32_750FX_V1_0,
    UC_CPU_PPC32_750FX_V2_0,
    UC_CPU_PPC32_750FX_V2_1,
    UC_CPU_PPC32_750FX_V2_2,
    UC_CPU_PPC32_750FX_V2_3,
    UC_CPU_PPC32_750GL,
    UC_CPU_PPC32_750GX_V1_0,
    UC_CPU_PPC32_750GX_V1_1,
    UC_CPU_PPC32_750GX_V1_2,
    UC_CPU_PPC32_750L_V2_0,
    UC_CPU_PPC32_750L_V2_1,
    UC_CPU_PPC32_750L_V2_2,
    UC_CPU_PPC32_750L_V3_0,
    UC_CPU_PPC32_750L_V3_2,
    UC_CPU_PPC32_745_V1_0,
    UC_CPU_PPC32_755_V1_0,
    UC_CPU_PPC32_745_V1_1,
    UC_CPU_PPC32_755_V1_1,
    UC_CPU_PPC32_745_V2_0,
    UC_CPU_PPC32_755_V2_0,
    UC_CPU_PPC32_745_V2_1,
    UC_CPU_PPC32_755_V2_1,
    UC_CPU_PPC32_745_V2_2,
    UC_CPU_PPC32_755_V2_2,
    UC_CPU_PPC32_745_V2_3,
    UC_CPU_PPC32_755_V2_3,
    UC_CPU_PPC32_745_V2_4,
    UC_CPU_PPC32_755_V2_4,
    UC_CPU_PPC32_745_V2_5,
    UC_CPU_PPC32_755_V2_5,
    UC_CPU_PPC32_745_V2_6,
    UC_CPU_PPC32_755_V2_6,
    UC_CPU_PPC32_745_V2_7,
    UC_CPU_PPC32_755_V2_7,
    UC_CPU_PPC32_745_V2_8,
    UC_CPU_PPC32_755_V2_8,
    UC_CPU_PPC32_7400_V1_0,
    UC_CPU_PPC32_7400_V1_1,
    UC_CPU_PPC32_7400_V2_0,
    UC_CPU_PPC32_7400_V2_1,
    UC_CPU_PPC32_7400_V2_2,
    UC_CPU_PPC32_7400_V2_6,
    UC_CPU_PPC32_7400_V2_7,
    UC_CPU_PPC32_7400_V2_8,
    UC_CPU_PPC32_7400_V2_9,
    UC_CPU_PPC32_7410_V1_0,
    UC_CPU_PPC32_7410_V1_1,
    UC_CPU_PPC32_7410_V1_2,
    UC_CPU_PPC32_7410_V1_3,
    UC_CPU_PPC32_7410_V1_4,
    UC_CPU_PPC32_7448_V1_0,
    UC_CPU_PPC32_7448_V1_1,
    UC_CPU_PPC32_7448_V2_0,
    UC_CPU_PPC32_7448_V2_1,
    UC_CPU_PPC32_7450_V1_0,
    UC_CPU_PPC32_7450_V1_1,
    UC_CPU_PPC32_7450_V1_2,
    UC_CPU_PPC32_7450_V2_0,
    UC_CPU_PPC32_7450_V2_1,
    UC_CPU_PPC32_7441_V2_1,
    UC_CPU_PPC32_7441_V2_3,
    UC_CPU_PPC32_7451_V2_3,
    UC_CPU_PPC32_7441_V2_10,
    UC_CPU_PPC32_7451_V2_10,
    UC_CPU_PPC32_7445_V1_0,
    UC_CPU_PPC32_7455_V1_0,
    UC_CPU_PPC32_7445_V2_1,
    UC_CPU_PPC32_7455_V2_1,
    UC_CPU_PPC32_7445_V3_2,
    UC_CPU_PPC32_7455_V3_2,
    UC_CPU_PPC32_7445_V3_3,
    UC_CPU_PPC32_7455_V3_3,
    UC_CPU_PPC32_7445_V3_4,
    UC_CPU_PPC32_7455_V3_4,
    UC_CPU_PPC32_7447_V1_0,
    UC_CPU_PPC32_7457_V1_0,
    UC_CPU_PPC32_7447_V1_1,
    UC_CPU_PPC32_7457_V1_1,
    UC_CPU_PPC32_7457_V1_2,
    UC_CPU_PPC32_7447A_V1_0,
    UC_CPU_PPC32_7457A_V1_0,
    UC_CPU_PPC32_7447A_V1_1,
    UC_CPU_PPC32_7457A_V1_1,
    UC_CPU_PPC32_7447A_V1_2,
    UC_CPU_PPC32_7457A_V1_2,

    UC_CPU_PPC32_ENDING
} uc_cpu_ppc;

//> PPC64 CPU
typedef enum uc_cpu_ppc64 {
    UC_CPU_PPC64_E5500 = 0,
    UC_CPU_PPC64_E6500,
    UC_CPU_PPC64_970_V2_2,
    UC_CPU_PPC64_970FX_V1_0,
    UC_CPU_PPC64_970FX_V2_0,
    UC_CPU_PPC64_970FX_V2_1,
    UC_CPU_PPC64_970FX_V3_0,
    UC_CPU_PPC64_970FX_V3_1,
    UC_CPU_PPC64_970MP_V1_0,
    UC_CPU_PPC64_970MP_V1_1,
    UC_CPU_PPC64_POWER5_V2_1,
    UC_CPU_PPC64_POWER7_V2_3,
    UC_CPU_PPC64_POWER7_V2_1,
    UC_CPU_PPC64_POWER8E_V2_1,
    UC_CPU_PPC64_POWER8_V2_0,
    UC_CPU_PPC64_POWER8NVL_V1_0,
    UC_CPU_PPC64_POWER9_V1_0,
    UC_CPU_PPC64_POWER9_V2_0,
    UC_CPU_PPC64_POWER10_V1_0,

    UC_CPU_PPC64_ENDING
} uc_cpu_ppc64;

//> PPC registers
typedef enum uc_ppc_reg {
    UC_PPC_REG_INVALID = 0,
    //> General purpose registers
    UC_PPC_REG_PC,

    UC_PPC_REG_0,
    UC_PPC_REG_1,
    UC_PPC_REG_2,
    UC_PPC_REG_3,
    UC_PPC_REG_4,
    UC_PPC_REG_5,
    UC_PPC_REG_6,
    UC_PPC_REG_7,
    UC_PPC_REG_8,
    UC_PPC_REG_9,
    UC_PPC_REG_10,
    UC_PPC_REG_11,
    UC_PPC_REG_12,
    UC_PPC_REG_13,
    UC_PPC_REG_14,
    UC_PPC_REG_15,
    UC_PPC_REG_16,
    UC_PPC_REG_17,
    UC_PPC_REG_18,
    UC_PPC_REG_19,
    UC_PPC_REG_20,
    UC_PPC_REG_21,
    UC_PPC_REG_22,
    UC_PPC_REG_23,
    UC_PPC_REG_24,
    UC_PPC_REG_25,
    UC_PPC_REG_26,
    UC_PPC_REG_27,
    UC_PPC_REG_28,
    UC_PPC_REG_29,
    UC_PPC_REG_30,
    UC_PPC_REG_31,

    UC_PPC_REG_CR0,
    UC_PPC_REG_CR1,
    UC_PPC_REG_CR2,
    UC_PPC_REG_CR3,
    UC_PPC_REG_CR4,
    UC_PPC_REG_CR5,
    UC_PPC_REG_CR6,
    UC_PPC_REG_CR7,

    UC_PPC_REG_FPR0,
    UC_PPC_REG_FPR1,
    UC_PPC_REG_FPR2,
    UC_PPC_REG_FPR3,
    UC_PPC_REG_FPR4,
    UC_PPC_REG_FPR5,
    UC_PPC_REG_FPR6,
    UC_PPC_REG_FPR7,
    UC_PPC_REG_FPR8,
    UC_PPC_REG_FPR9,
    UC_PPC_REG_FPR10,
    UC_PPC_REG_FPR11,
    UC_PPC_REG_FPR12,
    UC_PPC_REG_FPR13,
    UC_PPC_REG_FPR14,
    UC_PPC_REG_FPR15,
    UC_PPC_REG_FPR16,
    UC_PPC_REG_FPR17,
    UC_PPC_REG_FPR18,
    UC_PPC_REG_FPR19,
    UC_PPC_REG_FPR20,
    UC_PPC_REG_FPR21,
    UC_PPC_REG_FPR22,
    UC_PPC_REG_FPR23,
    UC_PPC_REG_FPR24,
    UC_PPC_REG_FPR25,
    UC_PPC_REG_FPR26,
    UC_PPC_REG_FPR27,
    UC_PPC_REG_FPR28,
    UC_PPC_REG_FPR29,
    UC_PPC_REG_FPR30,
    UC_PPC_REG_FPR31,

    UC_PPC_REG_LR,
    UC_PPC_REG_XER,
    UC_PPC_REG_CTR,
    UC_PPC_REG_MSR,
    UC_PPC_REG_FPSCR,
    UC_PPC_REG_CR,

    UC_PPC_REG_ENDING, // <-- mark the end of the list or registers
} uc_ppc_reg;

#ifdef __cplusplus
}
#endif

#endif