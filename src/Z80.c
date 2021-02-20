/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.c                         **/
/**                                                         **/
/** This file contains implementation for Z80 CPU. Don't    **/
/** forget to provide RdZ80(), WrZ80(), InZ80(), OutZ80(),  **/
/** LoopZ80(), and PatchZ80() functions to accomodate the   **/
/** emulated machine's architecture.                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

#include "Coleco.h"
#include "Z80.h"
#include "Tables.h"
#include <stdio.h>

/** INLINE ***************************************************/
/** Different compilers inline C functions differently.     **/
/*************************************************************/
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE static
#endif

extern Z80 CPU;
/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/
#ifdef COLEM
extern byte *RAM;
INLINE byte RdZ80(word A) { return(RAM[A]); }
#endif
#ifdef MG
extern byte *Page[];
INLINE byte RdZ80(word A) { return(Page[A>>13][A&0x1FFF]); }
#endif
#ifdef FMSX
extern byte *RAM[],PSL[],SSLReg;
INLINE byte RdZ80(word A)
{
  if(A!=0xFFFF) return(RAM[A>>13][A&0x1FFF]);
  else return((PSL[3]==3)? ~SSLReg:RAM[7][0x1FFF]);
}
#endif

#define S(Fl)        CPU.AF.B.l|=Fl
#define R(Fl)        CPU.AF.B.l&=~(Fl)
#define FLAGS(Rg,Fl) CPU.AF.B.l=Fl|ZSTable[Rg]

#define M_RLC(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|CPU.AF.B.l;CPU.AF.B.l|=PZSTable[Rg]
#define M_RRC(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg=(Rg>>1)|(CPU.AF.B.l<<7);CPU.AF.B.l|=PZSTable[Rg]
#define M_RL(Rg)       \
  if(Rg&0x80)          \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg<<1)|(CPU.AF.B.l&C_FLAG); \
    CPU.AF.B.l=PZSTable[Rg];        \
  }
#define M_RR(Rg)       \
  if(Rg&0x01)          \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg>>1)|(CPU.AF.B.l<<7);     \
    CPU.AF.B.l=PZSTable[Rg];        \
  }
  
#define M_SLA(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg<<=1;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRA(Rg)      \
  CPU.AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);CPU.AF.B.l|=PZSTable[Rg]

#define M_SLL(Rg)      \
  CPU.AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;CPU.AF.B.l|=PZSTable[Rg]
#define M_SRL(Rg)      \
  CPU.AF.B.l=Rg&0x01;Rg>>=1;CPU.AF.B.l|=PZSTable[Rg]

#define M_BIT(Bit,Rg)  \
  CPU.AF.B.l=(CPU.AF.B.l&~(N_FLAG|Z_FLAG))|H_FLAG|(Rg&(1<<Bit)? 0:Z_FLAG)

#define M_SET(Bit,Rg) Rg|=1<<Bit
#define M_RES(Bit,Rg) Rg&=~(1<<Bit)

#define M_POP(Rg)      \
  CPU.Rg.B.l=RdZ80(CPU.SP.W++);CPU.Rg.B.h=RdZ80(CPU.SP.W++)
#define M_PUSH(Rg)     \
  WrZ80(--CPU.SP.W,CPU.Rg.B.h);WrZ80(--CPU.SP.W,CPU.Rg.B.l)

#define M_CALL         \
  J.B.l=RdZ80(CPU.PC.W++);J.B.h=RdZ80(CPU.PC.W++);         \
  WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l); \
  CPU.PC.W=J.W

#define M_JP  J.B.l=RdZ80(CPU.PC.W++);J.B.h=RdZ80(CPU.PC.W);CPU.PC.W=J.W
#define M_JR  CPU.PC.W+=(offset)RdZ80(CPU.PC.W)+1
#define M_RET CPU.PC.B.l=RdZ80(CPU.SP.W++);CPU.PC.B.h=RdZ80(CPU.SP.W++)

#define M_RST(Ad)      \
  WrZ80(--CPU.SP.W,CPU.PC.B.h);WrZ80(--CPU.SP.W,CPU.PC.B.l);CPU.PC.W=Ad

#define M_LDWORD(Rg)   \
  CPU.Rg.B.l=RdZ80(CPU.PC.W++);CPU.Rg.B.h=RdZ80(CPU.PC.W++)

#define M_ADD(Rg)      \
  J.W=CPU.AF.B.h+Rg;     \
  CPU.AF.B.l=            \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|                        \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);               \
  CPU.AF.B.h=J.B.l       

#define M_SUB(Rg)      \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);                     \
  CPU.AF.B.h=J.B.l

#define M_ADC(Rg)      \
  J.W=CPU.AF.B.h+Rg+(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    (~(CPU.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|              \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_SBC(Rg)      \
  J.W=CPU.AF.B.h-Rg-(CPU.AF.B.l&C_FLAG); \
  CPU.AF.B.l=                           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG);     \
  CPU.AF.B.h=J.B.l

#define M_CP(Rg)       \
  J.W=CPU.AF.B.h-Rg;    \
  CPU.AF.B.l=           \
    ((CPU.AF.B.h^Rg)&(CPU.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((CPU.AF.B.h^Rg^J.B.l)&H_FLAG)

#define M_AND(Rg) CPU.AF.B.h&=Rg;CPU.AF.B.l=H_FLAG|PZSTable[CPU.AF.B.h]
#define M_OR(Rg)  CPU.AF.B.h|=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]
#define M_XOR(Rg) CPU.AF.B.h^=Rg;CPU.AF.B.l=PZSTable[CPU.AF.B.h]
#define M_IN(Rg)  Rg=InZ80(CPU.BC.B.l);CPU.AF.B.l=PZSTable[Rg]|(CPU.AF.B.l&C_FLAG)

#define M_INC(Rg)       \
  Rg++;                 \
  CPU.AF.B.l=            \
    (CPU.AF.B.l&C_FLAG)|ZSTable[Rg]|           \
    (Rg==0x80? V_FLAG:0)|(Rg&0x0F? 0:H_FLAG)

#define M_DEC(Rg)       \
  Rg--;                 \
  CPU.AF.B.l=            \
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[Rg]| \
    (Rg==0x7F? V_FLAG:0)|((Rg&0x0F)==0x0F? H_FLAG:0)

#define M_ADDW(Rg1,Rg2) \
  J.W=(CPU.Rg1.W+CPU.Rg2.W)&0xFFFF;                        \
  CPU.AF.B.l=                                             \
    (CPU.AF.B.l&~(H_FLAG|N_FLAG|C_FLAG))|                 \
    ((CPU.Rg1.W^CPU.Rg2.W^J.W)&0x1000? H_FLAG:0)|          \
    (((long)CPU.Rg1.W+(long)CPU.Rg2.W)&0x10000? C_FLAG:0); \
  CPU.Rg1.W=J.W

#define M_ADCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W+CPU.Rg.W+I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    (((long)CPU.HL.W+(long)CPU.Rg.W+(long)I)&0x10000? C_FLAG:0)| \
    (~(CPU.HL.W^CPU.Rg.W)&(CPU.Rg.W^J.W)&0x8000? V_FLAG:0)|       \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W
   
#define M_SBCW(Rg)      \
  I=CPU.AF.B.l&C_FLAG;J.W=(CPU.HL.W-CPU.Rg.W-I)&0xFFFF;           \
  CPU.AF.B.l=                                                   \
    N_FLAG|                                                    \
    (((long)CPU.HL.W-(long)CPU.Rg.W-(long)I)&0x10000? C_FLAG:0)| \
    ((CPU.HL.W^CPU.Rg.W)&(CPU.HL.W^J.W)&0x8000? V_FLAG:0)|        \
    ((CPU.HL.W^CPU.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  CPU.HL.W=J.W

enum Codes
{
  NOP,LD_BC_WORD,LD_xBC_A,INC_BC,INC_B,DEC_B,LD_B_BYTE,RLCA,
  EX_AF_AF,ADD_HL_BC,LD_A_xBC,DEC_BC,INC_C,DEC_C,LD_C_BYTE,RRCA,
  DJNZ,LD_DE_WORD,LD_xDE_A,INC_DE,INC_D,DEC_D,LD_D_BYTE,RLA,
  JR,ADD_HL_DE,LD_A_xDE,DEC_DE,INC_E,DEC_E,LD_E_BYTE,RRA,
  JR_NZ,LD_HL_WORD,LD_xWORD_HL,INC_HL,INC_H,DEC_H,LD_H_BYTE,DAA,
  JR_Z,ADD_HL_HL,LD_HL_xWORD,DEC_HL,INC_L,DEC_L,LD_L_BYTE,CPL,
  JR_NC,LD_SP_WORD,LD_xWORD_A,INC_SP,INC_xHL,DEC_xHL,LD_xHL_BYTE,SCF,
  JR_C,ADD_HL_SP,LD_A_xWORD,DEC_SP,INC_A,DEC_A,LD_A_BYTE,CCF,
  LD_B_B,LD_B_C,LD_B_D,LD_B_E,LD_B_H,LD_B_L,LD_B_xHL,LD_B_A,
  LD_C_B,LD_C_C,LD_C_D,LD_C_E,LD_C_H,LD_C_L,LD_C_xHL,LD_C_A,
  LD_D_B,LD_D_C,LD_D_D,LD_D_E,LD_D_H,LD_D_L,LD_D_xHL,LD_D_A,
  LD_E_B,LD_E_C,LD_E_D,LD_E_E,LD_E_H,LD_E_L,LD_E_xHL,LD_E_A,
  LD_H_B,LD_H_C,LD_H_D,LD_H_E,LD_H_H,LD_H_L,LD_H_xHL,LD_H_A,
  LD_L_B,LD_L_C,LD_L_D,LD_L_E,LD_L_H,LD_L_L,LD_L_xHL,LD_L_A,
  LD_xHL_B,LD_xHL_C,LD_xHL_D,LD_xHL_E,LD_xHL_H,LD_xHL_L,HALT,LD_xHL_A,
  LD_A_B,LD_A_C,LD_A_D,LD_A_E,LD_A_H,LD_A_L,LD_A_xHL,LD_A_A,
  ADD_B,ADD_C,ADD_D,ADD_E,ADD_H,ADD_L,ADD_xHL,ADD_A,
  ADC_B,ADC_C,ADC_D,ADC_E,ADC_H,ADC_L,ADC_xHL,ADC_A,
  SUB_B,SUB_C,SUB_D,SUB_E,SUB_H,SUB_L,SUB_xHL,SUB_A,
  SBC_B,SBC_C,SBC_D,SBC_E,SBC_H,SBC_L,SBC_xHL,SBC_A,
  AND_B,AND_C,AND_D,AND_E,AND_H,AND_L,AND_xHL,AND_A,
  XOR_B,XOR_C,XOR_D,XOR_E,XOR_H,XOR_L,XOR_xHL,XOR_A,
  OR_B,OR_C,OR_D,OR_E,OR_H,OR_L,OR_xHL,OR_A,
  CP_B,CP_C,CP_D,CP_E,CP_H,CP_L,CP_xHL,CP_A,
  RET_NZ,POP_BC,JP_NZ,JP,CALL_NZ,PUSH_BC,ADD_BYTE,RST00,
  RET_Z,RET,JP_Z,PFX_CB,CALL_Z,CALL,ADC_BYTE,RST08,
  RET_NC,POP_DE,JP_NC,OUTA,CALL_NC,PUSH_DE,SUB_BYTE,RST10,
  RET_C,EXX,JP_C,INA,CALL_C,PFX_DD,SBC_BYTE,RST18,
  RET_PO,POP_HL,JP_PO,EX_HL_xSP,CALL_PO,PUSH_HL,AND_BYTE,RST20,
  RET_PE,LD_PC_HL,JP_PE,EX_DE_HL,CALL_PE,PFX_ED,XOR_BYTE,RST28,
  RET_P,POP_AF,JP_P,DI,CALL_P,PUSH_AF,OR_BYTE,RST30,
  RET_M,LD_SP_HL,JP_M,EI,CALL_M,PFX_FD,CP_BYTE,RST38
};

enum CodesCB
{
  RLC_B,RLC_C,RLC_D,RLC_E,RLC_H,RLC_L,RLC_xHL,RLC_A,
  RRC_B,RRC_C,RRC_D,RRC_E,RRC_H,RRC_L,RRC_xHL,RRC_A,
  RL_B,RL_C,RL_D,RL_E,RL_H,RL_L,RL_xHL,RL_A,
  RR_B,RR_C,RR_D,RR_E,RR_H,RR_L,RR_xHL,RR_A,
  SLA_B,SLA_C,SLA_D,SLA_E,SLA_H,SLA_L,SLA_xHL,SLA_A,
  SRA_B,SRA_C,SRA_D,SRA_E,SRA_H,SRA_L,SRA_xHL,SRA_A,
  SLL_B,SLL_C,SLL_D,SLL_E,SLL_H,SLL_L,SLL_xHL,SLL_A,
  SRL_B,SRL_C,SRL_D,SRL_E,SRL_H,SRL_L,SRL_xHL,SRL_A,
  BIT0_B,BIT0_C,BIT0_D,BIT0_E,BIT0_H,BIT0_L,BIT0_xHL,BIT0_A,
  BIT1_B,BIT1_C,BIT1_D,BIT1_E,BIT1_H,BIT1_L,BIT1_xHL,BIT1_A,
  BIT2_B,BIT2_C,BIT2_D,BIT2_E,BIT2_H,BIT2_L,BIT2_xHL,BIT2_A,
  BIT3_B,BIT3_C,BIT3_D,BIT3_E,BIT3_H,BIT3_L,BIT3_xHL,BIT3_A,
  BIT4_B,BIT4_C,BIT4_D,BIT4_E,BIT4_H,BIT4_L,BIT4_xHL,BIT4_A,
  BIT5_B,BIT5_C,BIT5_D,BIT5_E,BIT5_H,BIT5_L,BIT5_xHL,BIT5_A,
  BIT6_B,BIT6_C,BIT6_D,BIT6_E,BIT6_H,BIT6_L,BIT6_xHL,BIT6_A,
  BIT7_B,BIT7_C,BIT7_D,BIT7_E,BIT7_H,BIT7_L,BIT7_xHL,BIT7_A,
  RES0_B,RES0_C,RES0_D,RES0_E,RES0_H,RES0_L,RES0_xHL,RES0_A,
  RES1_B,RES1_C,RES1_D,RES1_E,RES1_H,RES1_L,RES1_xHL,RES1_A,
  RES2_B,RES2_C,RES2_D,RES2_E,RES2_H,RES2_L,RES2_xHL,RES2_A,
  RES3_B,RES3_C,RES3_D,RES3_E,RES3_H,RES3_L,RES3_xHL,RES3_A,
  RES4_B,RES4_C,RES4_D,RES4_E,RES4_H,RES4_L,RES4_xHL,RES4_A,
  RES5_B,RES5_C,RES5_D,RES5_E,RES5_H,RES5_L,RES5_xHL,RES5_A,
  RES6_B,RES6_C,RES6_D,RES6_E,RES6_H,RES6_L,RES6_xHL,RES6_A,
  RES7_B,RES7_C,RES7_D,RES7_E,RES7_H,RES7_L,RES7_xHL,RES7_A,  
  SET0_B,SET0_C,SET0_D,SET0_E,SET0_H,SET0_L,SET0_xHL,SET0_A,
  SET1_B,SET1_C,SET1_D,SET1_E,SET1_H,SET1_L,SET1_xHL,SET1_A,
  SET2_B,SET2_C,SET2_D,SET2_E,SET2_H,SET2_L,SET2_xHL,SET2_A,
  SET3_B,SET3_C,SET3_D,SET3_E,SET3_H,SET3_L,SET3_xHL,SET3_A,
  SET4_B,SET4_C,SET4_D,SET4_E,SET4_H,SET4_L,SET4_xHL,SET4_A,
  SET5_B,SET5_C,SET5_D,SET5_E,SET5_H,SET5_L,SET5_xHL,SET5_A,
  SET6_B,SET6_C,SET6_D,SET6_E,SET6_H,SET6_L,SET6_xHL,SET6_A,
  SET7_B,SET7_C,SET7_D,SET7_E,SET7_H,SET7_L,SET7_xHL,SET7_A
};
  
enum CodesED
{
  DB_00,DB_01,DB_02,DB_03,DB_04,DB_05,DB_06,DB_07,
  DB_08,DB_09,DB_0A,DB_0B,DB_0C,DB_0D,DB_0E,DB_0F,
  DB_10,DB_11,DB_12,DB_13,DB_14,DB_15,DB_16,DB_17,
  DB_18,DB_19,DB_1A,DB_1B,DB_1C,DB_1D,DB_1E,DB_1F,
  DB_20,DB_21,DB_22,DB_23,DB_24,DB_25,DB_26,DB_27,
  DB_28,DB_29,DB_2A,DB_2B,DB_2C,DB_2D,DB_2E,DB_2F,
  DB_30,DB_31,DB_32,DB_33,DB_34,DB_35,DB_36,DB_37,
  DB_38,DB_39,DB_3A,DB_3B,DB_3C,DB_3D,DB_3E,DB_3F,
  IN_B_xC,OUT_xC_B,SBC_HL_BC,LD_xWORDe_BC,NEG,RETN,IM_0,LD_I_A,
  IN_C_xC,OUT_xC_C,ADC_HL_BC,LD_BC_xWORDe,DB_4C,RETI,DB_,LD_R_A,
  IN_D_xC,OUT_xC_D,SBC_HL_DE,LD_xWORDe_DE,DB_54,DB_55,IM_1,LD_A_I,
  IN_E_xC,OUT_xC_E,ADC_HL_DE,LD_DE_xWORDe,DB_5C,DB_5D,IM_2,LD_A_R,
  IN_H_xC,OUT_xC_H,SBC_HL_HL,LD_xWORDe_HL,DB_64,DB_65,DB_66,RRD,
  IN_L_xC,OUT_xC_L,ADC_HL_HL,LD_HL_xWORDe,DB_6C,DB_6D,DB_6E,RLD,
  IN_F_xC,DB_71,SBC_HL_SP,LD_xWORDe_SP,DB_74,DB_75,DB_76,DB_77,
  IN_A_xC,OUT_xC_A,ADC_HL_SP,LD_SP_xWORDe,DB_7C,DB_7D,DB_7E,DB_7F,
  DB_80,DB_81,DB_82,DB_83,DB_84,DB_85,DB_86,DB_87,
  DB_88,DB_89,DB_8A,DB_8B,DB_8C,DB_8D,DB_8E,DB_8F,
  DB_90,DB_91,DB_92,DB_93,DB_94,DB_95,DB_96,DB_97,
  DB_98,DB_99,DB_9A,DB_9B,DB_9C,DB_9D,DB_9E,DB_9F,
  LDI,CPI,INI,OUTI,DB_A4,DB_A5,DB_A6,DB_A7,
  LDD,CPD,IND,OUTD,DB_AC,DB_AD,DB_AE,DB_AF,
  LDIR,CPIR,INIR,OTIR,DB_B4,DB_B5,DB_B6,DB_B7,
  LDDR,CPDR,INDR,OTDR,DB_BC,DB_BD,DB_BE,DB_BF,
  DB_C0,DB_C1,DB_C2,DB_C3,DB_C4,DB_C5,DB_C6,DB_C7,
  DB_C8,DB_C9,DB_CA,DB_CB,DB_CC,DB_CD,DB_CE,DB_CF,
  DB_D0,DB_D1,DB_D2,DB_D3,DB_D4,DB_D5,DB_D6,DB_D7,
  DB_D8,DB_D9,DB_DA,DB_DB,DB_DC,DB_DD,DB_DE,DB_DF,
  DB_E0,DB_E1,DB_E2,DB_E3,DB_E4,DB_E5,DB_E6,DB_E7,
  DB_E8,DB_E9,DB_EA,DB_EB,DB_EC,DB_ED,DB_EE,DB_EF,
  DB_F0,DB_F1,DB_F2,DB_F3,DB_F4,DB_F5,DB_F6,DB_F7,
  DB_F8,DB_F9,DB_FA,DB_FB,DB_FC,DB_FD,DB_FE,DB_FF
};

static void CodesCB()
{
  __label__
  RLC_B,RLC_C,RLC_D,RLC_E,RLC_H,RLC_L,RLC_xHL,RLC_A,
  RRC_B,RRC_C,RRC_D,RRC_E,RRC_H,RRC_L,RRC_xHL,RRC_A,
  RL_B,RL_C,RL_D,RL_E,RL_H,RL_L,RL_xHL,RL_A,
  RR_B,RR_C,RR_D,RR_E,RR_H,RR_L,RR_xHL,RR_A,
  SLA_B,SLA_C,SLA_D,SLA_E,SLA_H,SLA_L,SLA_xHL,SLA_A,
  SRA_B,SRA_C,SRA_D,SRA_E,SRA_H,SRA_L,SRA_xHL,SRA_A,
  SLL_B,SLL_C,SLL_D,SLL_E,SLL_H,SLL_L,SLL_xHL,SLL_A,
  SRL_B,SRL_C,SRL_D,SRL_E,SRL_H,SRL_L,SRL_xHL,SRL_A,
  BIT0_B,BIT0_C,BIT0_D,BIT0_E,BIT0_H,BIT0_L,BIT0_xHL,BIT0_A,
  BIT1_B,BIT1_C,BIT1_D,BIT1_E,BIT1_H,BIT1_L,BIT1_xHL,BIT1_A,
  BIT2_B,BIT2_C,BIT2_D,BIT2_E,BIT2_H,BIT2_L,BIT2_xHL,BIT2_A,
  BIT3_B,BIT3_C,BIT3_D,BIT3_E,BIT3_H,BIT3_L,BIT3_xHL,BIT3_A,
  BIT4_B,BIT4_C,BIT4_D,BIT4_E,BIT4_H,BIT4_L,BIT4_xHL,BIT4_A,
  BIT5_B,BIT5_C,BIT5_D,BIT5_E,BIT5_H,BIT5_L,BIT5_xHL,BIT5_A,
  BIT6_B,BIT6_C,BIT6_D,BIT6_E,BIT6_H,BIT6_L,BIT6_xHL,BIT6_A,
  BIT7_B,BIT7_C,BIT7_D,BIT7_E,BIT7_H,BIT7_L,BIT7_xHL,BIT7_A,
  RES0_B,RES0_C,RES0_D,RES0_E,RES0_H,RES0_L,RES0_xHL,RES0_A,
  RES1_B,RES1_C,RES1_D,RES1_E,RES1_H,RES1_L,RES1_xHL,RES1_A,
  RES2_B,RES2_C,RES2_D,RES2_E,RES2_H,RES2_L,RES2_xHL,RES2_A,
  RES3_B,RES3_C,RES3_D,RES3_E,RES3_H,RES3_L,RES3_xHL,RES3_A,
  RES4_B,RES4_C,RES4_D,RES4_E,RES4_H,RES4_L,RES4_xHL,RES4_A,
  RES5_B,RES5_C,RES5_D,RES5_E,RES5_H,RES5_L,RES5_xHL,RES5_A,
  RES6_B,RES6_C,RES6_D,RES6_E,RES6_H,RES6_L,RES6_xHL,RES6_A,
  RES7_B,RES7_C,RES7_D,RES7_E,RES7_H,RES7_L,RES7_xHL,RES7_A,  
  SET0_B,SET0_C,SET0_D,SET0_E,SET0_H,SET0_L,SET0_xHL,SET0_A,
  SET1_B,SET1_C,SET1_D,SET1_E,SET1_H,SET1_L,SET1_xHL,SET1_A,
  SET2_B,SET2_C,SET2_D,SET2_E,SET2_H,SET2_L,SET2_xHL,SET2_A,
  SET3_B,SET3_C,SET3_D,SET3_E,SET3_H,SET3_L,SET3_xHL,SET3_A,
  SET4_B,SET4_C,SET4_D,SET4_E,SET4_H,SET4_L,SET4_xHL,SET4_A,
  SET5_B,SET5_C,SET5_D,SET5_E,SET5_H,SET5_L,SET5_xHL,SET5_A,
  SET6_B,SET6_C,SET6_D,SET6_E,SET6_H,SET6_L,SET6_xHL,SET6_A,
  SET7_B,SET7_C,SET7_D,SET7_E,SET7_H,SET7_L,SET7_xHL,SET7_A;

    static const void* const a_jump_table[256] = { &&
  RLC_B,&&RLC_C,&&RLC_D,&&RLC_E,&&RLC_H,&&RLC_L,&&RLC_xHL,&&RLC_A,&&
  RRC_B,&&RRC_C,&&RRC_D,&&RRC_E,&&RRC_H,&&RRC_L,&&RRC_xHL,&&RRC_A,&&
  RL_B,&&RL_C,&&RL_D,&&RL_E,&&RL_H,&&RL_L,&&RL_xHL,&&RL_A,&&
  RR_B,&&RR_C,&&RR_D,&&RR_E,&&RR_H,&&RR_L,&&RR_xHL,&&RR_A,&&
  SLA_B,&&SLA_C,&&SLA_D,&&SLA_E,&&SLA_H,&&SLA_L,&&SLA_xHL,&&SLA_A,&&
  SRA_B,&&SRA_C,&&SRA_D,&&SRA_E,&&SRA_H,&&SRA_L,&&SRA_xHL,&&SRA_A,&&
  SLL_B,&&SLL_C,&&SLL_D,&&SLL_E,&&SLL_H,&&SLL_L,&&SLL_xHL,&&SLL_A,&&
  SRL_B,&&SRL_C,&&SRL_D,&&SRL_E,&&SRL_H,&&SRL_L,&&SRL_xHL,&&SRL_A,&&
  BIT0_B,&&BIT0_C,&&BIT0_D,&&BIT0_E,&&BIT0_H,&&BIT0_L,&&BIT0_xHL,&&BIT0_A,&&
  BIT1_B,&&BIT1_C,&&BIT1_D,&&BIT1_E,&&BIT1_H,&&BIT1_L,&&BIT1_xHL,&&BIT1_A,&&
  BIT2_B,&&BIT2_C,&&BIT2_D,&&BIT2_E,&&BIT2_H,&&BIT2_L,&&BIT2_xHL,&&BIT2_A,&&
  BIT3_B,&&BIT3_C,&&BIT3_D,&&BIT3_E,&&BIT3_H,&&BIT3_L,&&BIT3_xHL,&&BIT3_A,&&
  BIT4_B,&&BIT4_C,&&BIT4_D,&&BIT4_E,&&BIT4_H,&&BIT4_L,&&BIT4_xHL,&&BIT4_A,&&
  BIT5_B,&&BIT5_C,&&BIT5_D,&&BIT5_E,&&BIT5_H,&&BIT5_L,&&BIT5_xHL,&&BIT5_A,&&
  BIT6_B,&&BIT6_C,&&BIT6_D,&&BIT6_E,&&BIT6_H,&&BIT6_L,&&BIT6_xHL,&&BIT6_A,&&
  BIT7_B,&&BIT7_C,&&BIT7_D,&&BIT7_E,&&BIT7_H,&&BIT7_L,&&BIT7_xHL,&&BIT7_A,&&
  RES0_B,&&RES0_C,&&RES0_D,&&RES0_E,&&RES0_H,&&RES0_L,&&RES0_xHL,&&RES0_A,&&
  RES1_B,&&RES1_C,&&RES1_D,&&RES1_E,&&RES1_H,&&RES1_L,&&RES1_xHL,&&RES1_A,&&
  RES2_B,&&RES2_C,&&RES2_D,&&RES2_E,&&RES2_H,&&RES2_L,&&RES2_xHL,&&RES2_A,&&
  RES3_B,&&RES3_C,&&RES3_D,&&RES3_E,&&RES3_H,&&RES3_L,&&RES3_xHL,&&RES3_A,&&
  RES4_B,&&RES4_C,&&RES4_D,&&RES4_E,&&RES4_H,&&RES4_L,&&RES4_xHL,&&RES4_A,&&
  RES5_B,&&RES5_C,&&RES5_D,&&RES5_E,&&RES5_H,&&RES5_L,&&RES5_xHL,&&RES5_A,&&
  RES6_B,&&RES6_C,&&RES6_D,&&RES6_E,&&RES6_H,&&RES6_L,&&RES6_xHL,&&RES6_A,&&
  RES7_B,&&RES7_C,&&RES7_D,&&RES7_E,&&RES7_H,&&RES7_L,&&RES7_xHL,&&RES7_A,&&  
  SET0_B,&&SET0_C,&&SET0_D,&&SET0_E,&&SET0_H,&&SET0_L,&&SET0_xHL,&&SET0_A,&&
  SET1_B,&&SET1_C,&&SET1_D,&&SET1_E,&&SET1_H,&&SET1_L,&&SET1_xHL,&&SET1_A,&&
  SET2_B,&&SET2_C,&&SET2_D,&&SET2_E,&&SET2_H,&&SET2_L,&&SET2_xHL,&&SET2_A,&&
  SET3_B,&&SET3_C,&&SET3_D,&&SET3_E,&&SET3_H,&&SET3_L,&&SET3_xHL,&&SET3_A,&&
  SET4_B,&&SET4_C,&&SET4_D,&&SET4_E,&&SET4_H,&&SET4_L,&&SET4_xHL,&&SET4_A,&&
  SET5_B,&&SET5_C,&&SET5_D,&&SET5_E,&&SET5_H,&&SET5_L,&&SET5_xHL,&&SET5_A,&&
  SET6_B,&&SET6_C,&&SET6_D,&&SET6_E,&&SET6_H,&&SET6_L,&&SET6_xHL,&&SET6_A,&&
  SET7_B,&&SET7_C,&&SET7_D,&&SET7_E,&&SET7_H,&&SET7_L,&&SET7_xHL,&&SET7_A 
  };

  byte I;

  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesCB[I];
	goto *a_jump_table[I];
  //switch(I) {
     RLC_B: M_RLC(CPU.BC.B.h);return;   RLC_C: M_RLC(CPU.BC.B.l);return;
     RLC_D: M_RLC(CPU.DE.B.h);return;   RLC_E: M_RLC(CPU.DE.B.l);return;
     RLC_H: M_RLC(CPU.HL.B.h);return;   RLC_L: M_RLC(CPU.HL.B.l);return;
     RLC_xHL: I=RdZ80(CPU.HL.W);M_RLC(I);WrZ80(CPU.HL.W,I);return;
     RLC_A: M_RLC(CPU.AF.B.h);return;

     RRC_B: M_RRC(CPU.BC.B.h);return;   RRC_C: M_RRC(CPU.BC.B.l);return;
     RRC_D: M_RRC(CPU.DE.B.h);return;   RRC_E: M_RRC(CPU.DE.B.l);return;
     RRC_H: M_RRC(CPU.HL.B.h);return;   RRC_L: M_RRC(CPU.HL.B.l);return;
     RRC_xHL: I=RdZ80(CPU.HL.W);M_RRC(I);WrZ80(CPU.HL.W,I);return;
     RRC_A: M_RRC(CPU.AF.B.h);return;

     RL_B: M_RL(CPU.BC.B.h);return;   RL_C: M_RL(CPU.BC.B.l);return;
     RL_D: M_RL(CPU.DE.B.h);return;   RL_E: M_RL(CPU.DE.B.l);return;
     RL_H: M_RL(CPU.HL.B.h);return;   RL_L: M_RL(CPU.HL.B.l);return;
     RL_xHL: I=RdZ80(CPU.HL.W);M_RL(I);WrZ80(CPU.HL.W,I);return;
     RL_A: M_RL(CPU.AF.B.h);return;

     RR_B: M_RR(CPU.BC.B.h);return;   RR_C: M_RR(CPU.BC.B.l);return;
     RR_D: M_RR(CPU.DE.B.h);return;   RR_E: M_RR(CPU.DE.B.l);return;
     RR_H: M_RR(CPU.HL.B.h);return;   RR_L: M_RR(CPU.HL.B.l);return;
     RR_xHL: I=RdZ80(CPU.HL.W);M_RR(I);WrZ80(CPU.HL.W,I);return;
     RR_A: M_RR(CPU.AF.B.h);return;

     SLA_B: M_SLA(CPU.BC.B.h);return;   SLA_C: M_SLA(CPU.BC.B.l);return;
     SLA_D: M_SLA(CPU.DE.B.h);return;   SLA_E: M_SLA(CPU.DE.B.l);return;
     SLA_H: M_SLA(CPU.HL.B.h);return;   SLA_L: M_SLA(CPU.HL.B.l);return;
     SLA_xHL: I=RdZ80(CPU.HL.W);M_SLA(I);WrZ80(CPU.HL.W,I);return;
     SLA_A: M_SLA(CPU.AF.B.h);return;

     SRA_B: M_SRA(CPU.BC.B.h);return;   SRA_C: M_SRA(CPU.BC.B.l);return;
     SRA_D: M_SRA(CPU.DE.B.h);return;   SRA_E: M_SRA(CPU.DE.B.l);return;
     SRA_H: M_SRA(CPU.HL.B.h);return;   SRA_L: M_SRA(CPU.HL.B.l);return;
     SRA_xHL: I=RdZ80(CPU.HL.W);M_SRA(I);WrZ80(CPU.HL.W,I);return;
     SRA_A: M_SRA(CPU.AF.B.h);return;

     SLL_B: M_SLL(CPU.BC.B.h);return;   SLL_C: M_SLL(CPU.BC.B.l);return;
     SLL_D: M_SLL(CPU.DE.B.h);return;   SLL_E: M_SLL(CPU.DE.B.l);return;
     SLL_H: M_SLL(CPU.HL.B.h);return;   SLL_L: M_SLL(CPU.HL.B.l);return;
     SLL_xHL: I=RdZ80(CPU.HL.W);M_SLL(I);WrZ80(CPU.HL.W,I);return;
     SLL_A: M_SLL(CPU.AF.B.h);return;

     SRL_B: M_SRL(CPU.BC.B.h);return;   SRL_C: M_SRL(CPU.BC.B.l);return;
     SRL_D: M_SRL(CPU.DE.B.h);return;   SRL_E: M_SRL(CPU.DE.B.l);return;
     SRL_H: M_SRL(CPU.HL.B.h);return;   SRL_L: M_SRL(CPU.HL.B.l);return;
     SRL_xHL: I=RdZ80(CPU.HL.W);M_SRL(I);WrZ80(CPU.HL.W,I);return;
     SRL_A: M_SRL(CPU.AF.B.h);return;
    
     BIT0_B: M_BIT(0,CPU.BC.B.h);return;   BIT0_C: M_BIT(0,CPU.BC.B.l);return;
     BIT0_D: M_BIT(0,CPU.DE.B.h);return;   BIT0_E: M_BIT(0,CPU.DE.B.l);return;
     BIT0_H: M_BIT(0,CPU.HL.B.h);return;   BIT0_L: M_BIT(0,CPU.HL.B.l);return;
     BIT0_xHL: I=RdZ80(CPU.HL.W);M_BIT(0,I);return;
     BIT0_A: M_BIT(0,CPU.AF.B.h);return;

     BIT1_B: M_BIT(1,CPU.BC.B.h);return;   BIT1_C: M_BIT(1,CPU.BC.B.l);return;
     BIT1_D: M_BIT(1,CPU.DE.B.h);return;   BIT1_E: M_BIT(1,CPU.DE.B.l);return;
     BIT1_H: M_BIT(1,CPU.HL.B.h);return;   BIT1_L: M_BIT(1,CPU.HL.B.l);return;
     BIT1_xHL: I=RdZ80(CPU.HL.W);M_BIT(1,I);return;
     BIT1_A: M_BIT(1,CPU.AF.B.h);return;

     BIT2_B: M_BIT(2,CPU.BC.B.h);return;   BIT2_C: M_BIT(2,CPU.BC.B.l);return;
     BIT2_D: M_BIT(2,CPU.DE.B.h);return;   BIT2_E: M_BIT(2,CPU.DE.B.l);return;
     BIT2_H: M_BIT(2,CPU.HL.B.h);return;   BIT2_L: M_BIT(2,CPU.HL.B.l);return;
     BIT2_xHL: I=RdZ80(CPU.HL.W);M_BIT(2,I);return;
     BIT2_A: M_BIT(2,CPU.AF.B.h);return;

     BIT3_B: M_BIT(3,CPU.BC.B.h);return;   BIT3_C: M_BIT(3,CPU.BC.B.l);return;
     BIT3_D: M_BIT(3,CPU.DE.B.h);return;   BIT3_E: M_BIT(3,CPU.DE.B.l);return;
     BIT3_H: M_BIT(3,CPU.HL.B.h);return;   BIT3_L: M_BIT(3,CPU.HL.B.l);return;
     BIT3_xHL: I=RdZ80(CPU.HL.W);M_BIT(3,I);return;
     BIT3_A: M_BIT(3,CPU.AF.B.h);return;

     BIT4_B: M_BIT(4,CPU.BC.B.h);return;   BIT4_C: M_BIT(4,CPU.BC.B.l);return;
     BIT4_D: M_BIT(4,CPU.DE.B.h);return;   BIT4_E: M_BIT(4,CPU.DE.B.l);return;
     BIT4_H: M_BIT(4,CPU.HL.B.h);return;   BIT4_L: M_BIT(4,CPU.HL.B.l);return;
     BIT4_xHL: I=RdZ80(CPU.HL.W);M_BIT(4,I);return;
     BIT4_A: M_BIT(4,CPU.AF.B.h);return;

     BIT5_B: M_BIT(5,CPU.BC.B.h);return;   BIT5_C: M_BIT(5,CPU.BC.B.l);return;
     BIT5_D: M_BIT(5,CPU.DE.B.h);return;   BIT5_E: M_BIT(5,CPU.DE.B.l);return;
     BIT5_H: M_BIT(5,CPU.HL.B.h);return;   BIT5_L: M_BIT(5,CPU.HL.B.l);return;
     BIT5_xHL: I=RdZ80(CPU.HL.W);M_BIT(5,I);return;
     BIT5_A: M_BIT(5,CPU.AF.B.h);return;

     BIT6_B: M_BIT(6,CPU.BC.B.h);return;   BIT6_C: M_BIT(6,CPU.BC.B.l);return;
     BIT6_D: M_BIT(6,CPU.DE.B.h);return;   BIT6_E: M_BIT(6,CPU.DE.B.l);return;
     BIT6_H: M_BIT(6,CPU.HL.B.h);return;   BIT6_L: M_BIT(6,CPU.HL.B.l);return;
     BIT6_xHL: I=RdZ80(CPU.HL.W);M_BIT(6,I);return;
     BIT6_A: M_BIT(6,CPU.AF.B.h);return;

     BIT7_B: M_BIT(7,CPU.BC.B.h);return;   BIT7_C: M_BIT(7,CPU.BC.B.l);return;
     BIT7_D: M_BIT(7,CPU.DE.B.h);return;   BIT7_E: M_BIT(7,CPU.DE.B.l);return;
     BIT7_H: M_BIT(7,CPU.HL.B.h);return;   BIT7_L: M_BIT(7,CPU.HL.B.l);return;
     BIT7_xHL: I=RdZ80(CPU.HL.W);M_BIT(7,I);return;
     BIT7_A: M_BIT(7,CPU.AF.B.h);return;

     RES0_B: M_RES(0,CPU.BC.B.h);return;   RES0_C: M_RES(0,CPU.BC.B.l);return;
     RES0_D: M_RES(0,CPU.DE.B.h);return;   RES0_E: M_RES(0,CPU.DE.B.l);return;
     RES0_H: M_RES(0,CPU.HL.B.h);return;   RES0_L: M_RES(0,CPU.HL.B.l);return;
     RES0_xHL: I=RdZ80(CPU.HL.W);M_RES(0,I);WrZ80(CPU.HL.W,I);return;
     RES0_A: M_RES(0,CPU.AF.B.h);return;

     RES1_B: M_RES(1,CPU.BC.B.h);return;   RES1_C: M_RES(1,CPU.BC.B.l);return;
     RES1_D: M_RES(1,CPU.DE.B.h);return;   RES1_E: M_RES(1,CPU.DE.B.l);return;
     RES1_H: M_RES(1,CPU.HL.B.h);return;   RES1_L: M_RES(1,CPU.HL.B.l);return;
     RES1_xHL: I=RdZ80(CPU.HL.W);M_RES(1,I);WrZ80(CPU.HL.W,I);return;
     RES1_A: M_RES(1,CPU.AF.B.h);return;

     RES2_B: M_RES(2,CPU.BC.B.h);return;   RES2_C: M_RES(2,CPU.BC.B.l);return;
     RES2_D: M_RES(2,CPU.DE.B.h);return;   RES2_E: M_RES(2,CPU.DE.B.l);return;
     RES2_H: M_RES(2,CPU.HL.B.h);return;   RES2_L: M_RES(2,CPU.HL.B.l);return;
     RES2_xHL: I=RdZ80(CPU.HL.W);M_RES(2,I);WrZ80(CPU.HL.W,I);return;
     RES2_A: M_RES(2,CPU.AF.B.h);return;

     RES3_B: M_RES(3,CPU.BC.B.h);return;   RES3_C: M_RES(3,CPU.BC.B.l);return;
     RES3_D: M_RES(3,CPU.DE.B.h);return;   RES3_E: M_RES(3,CPU.DE.B.l);return;
     RES3_H: M_RES(3,CPU.HL.B.h);return;   RES3_L: M_RES(3,CPU.HL.B.l);return;
     RES3_xHL: I=RdZ80(CPU.HL.W);M_RES(3,I);WrZ80(CPU.HL.W,I);return;
     RES3_A: M_RES(3,CPU.AF.B.h);return;

     RES4_B: M_RES(4,CPU.BC.B.h);return;   RES4_C: M_RES(4,CPU.BC.B.l);return;
     RES4_D: M_RES(4,CPU.DE.B.h);return;   RES4_E: M_RES(4,CPU.DE.B.l);return;
     RES4_H: M_RES(4,CPU.HL.B.h);return;   RES4_L: M_RES(4,CPU.HL.B.l);return;
     RES4_xHL: I=RdZ80(CPU.HL.W);M_RES(4,I);WrZ80(CPU.HL.W,I);return;
     RES4_A: M_RES(4,CPU.AF.B.h);return;

     RES5_B: M_RES(5,CPU.BC.B.h);return;   RES5_C: M_RES(5,CPU.BC.B.l);return;
     RES5_D: M_RES(5,CPU.DE.B.h);return;   RES5_E: M_RES(5,CPU.DE.B.l);return;
     RES5_H: M_RES(5,CPU.HL.B.h);return;   RES5_L: M_RES(5,CPU.HL.B.l);return;
     RES5_xHL: I=RdZ80(CPU.HL.W);M_RES(5,I);WrZ80(CPU.HL.W,I);return;
     RES5_A: M_RES(5,CPU.AF.B.h);return;

     RES6_B: M_RES(6,CPU.BC.B.h);return;   RES6_C: M_RES(6,CPU.BC.B.l);return;
     RES6_D: M_RES(6,CPU.DE.B.h);return;   RES6_E: M_RES(6,CPU.DE.B.l);return;
     RES6_H: M_RES(6,CPU.HL.B.h);return;   RES6_L: M_RES(6,CPU.HL.B.l);return;
     RES6_xHL: I=RdZ80(CPU.HL.W);M_RES(6,I);WrZ80(CPU.HL.W,I);return;
     RES6_A: M_RES(6,CPU.AF.B.h);return;

     RES7_B: M_RES(7,CPU.BC.B.h);return;   RES7_C: M_RES(7,CPU.BC.B.l);return;
     RES7_D: M_RES(7,CPU.DE.B.h);return;   RES7_E: M_RES(7,CPU.DE.B.l);return;
     RES7_H: M_RES(7,CPU.HL.B.h);return;   RES7_L: M_RES(7,CPU.HL.B.l);return;
     RES7_xHL: I=RdZ80(CPU.HL.W);M_RES(7,I);WrZ80(CPU.HL.W,I);return;
     RES7_A: M_RES(7,CPU.AF.B.h);return;

     SET0_B: M_SET(0,CPU.BC.B.h);return;   SET0_C: M_SET(0,CPU.BC.B.l);return;
     SET0_D: M_SET(0,CPU.DE.B.h);return;   SET0_E: M_SET(0,CPU.DE.B.l);return;
     SET0_H: M_SET(0,CPU.HL.B.h);return;   SET0_L: M_SET(0,CPU.HL.B.l);return;
     SET0_xHL: I=RdZ80(CPU.HL.W);M_SET(0,I);WrZ80(CPU.HL.W,I);return;
     SET0_A: M_SET(0,CPU.AF.B.h);return;

     SET1_B: M_SET(1,CPU.BC.B.h);return;   SET1_C: M_SET(1,CPU.BC.B.l);return;
     SET1_D: M_SET(1,CPU.DE.B.h);return;   SET1_E: M_SET(1,CPU.DE.B.l);return;
     SET1_H: M_SET(1,CPU.HL.B.h);return;   SET1_L: M_SET(1,CPU.HL.B.l);return;
     SET1_xHL: I=RdZ80(CPU.HL.W);M_SET(1,I);WrZ80(CPU.HL.W,I);return;
     SET1_A: M_SET(1,CPU.AF.B.h);return;

     SET2_B: M_SET(2,CPU.BC.B.h);return;   SET2_C: M_SET(2,CPU.BC.B.l);return;
     SET2_D: M_SET(2,CPU.DE.B.h);return;   SET2_E: M_SET(2,CPU.DE.B.l);return;
     SET2_H: M_SET(2,CPU.HL.B.h);return;   SET2_L: M_SET(2,CPU.HL.B.l);return;
     SET2_xHL: I=RdZ80(CPU.HL.W);M_SET(2,I);WrZ80(CPU.HL.W,I);return;
     SET2_A: M_SET(2,CPU.AF.B.h);return;

     SET3_B: M_SET(3,CPU.BC.B.h);return;   SET3_C: M_SET(3,CPU.BC.B.l);return;
     SET3_D: M_SET(3,CPU.DE.B.h);return;   SET3_E: M_SET(3,CPU.DE.B.l);return;
     SET3_H: M_SET(3,CPU.HL.B.h);return;   SET3_L: M_SET(3,CPU.HL.B.l);return;
     SET3_xHL: I=RdZ80(CPU.HL.W);M_SET(3,I);WrZ80(CPU.HL.W,I);return;
     SET3_A: M_SET(3,CPU.AF.B.h);return;

     SET4_B: M_SET(4,CPU.BC.B.h);return;   SET4_C: M_SET(4,CPU.BC.B.l);return;
     SET4_D: M_SET(4,CPU.DE.B.h);return;   SET4_E: M_SET(4,CPU.DE.B.l);return;
     SET4_H: M_SET(4,CPU.HL.B.h);return;   SET4_L: M_SET(4,CPU.HL.B.l);return;
     SET4_xHL: I=RdZ80(CPU.HL.W);M_SET(4,I);WrZ80(CPU.HL.W,I);return;
     SET4_A: M_SET(4,CPU.AF.B.h);return;

     SET5_B: M_SET(5,CPU.BC.B.h);return;   SET5_C: M_SET(5,CPU.BC.B.l);return;
     SET5_D: M_SET(5,CPU.DE.B.h);return;   SET5_E: M_SET(5,CPU.DE.B.l);return;
     SET5_H: M_SET(5,CPU.HL.B.h);return;   SET5_L: M_SET(5,CPU.HL.B.l);return;
     SET5_xHL: I=RdZ80(CPU.HL.W);M_SET(5,I);WrZ80(CPU.HL.W,I);return;
     SET5_A: M_SET(5,CPU.AF.B.h);return;

     SET6_B: M_SET(6,CPU.BC.B.h);return;   SET6_C: M_SET(6,CPU.BC.B.l);return;
     SET6_D: M_SET(6,CPU.DE.B.h);return;   SET6_E: M_SET(6,CPU.DE.B.l);return;
     SET6_H: M_SET(6,CPU.HL.B.h);return;   SET6_L: M_SET(6,CPU.HL.B.l);return;
     SET6_xHL: I=RdZ80(CPU.HL.W);M_SET(6,I);WrZ80(CPU.HL.W,I);return;
     SET6_A: M_SET(6,CPU.AF.B.h);return;

     SET7_B: M_SET(7,CPU.BC.B.h);return;   SET7_C: M_SET(7,CPU.BC.B.l);return;
     SET7_D: M_SET(7,CPU.DE.B.h);return;   SET7_E: M_SET(7,CPU.DE.B.l);return;
     SET7_H: M_SET(7,CPU.HL.B.h);return;   SET7_L: M_SET(7,CPU.HL.B.l);return;
     SET7_xHL: I=RdZ80(CPU.HL.W);M_SET(7,I);WrZ80(CPU.HL.W,I);return;
     SET7_A: M_SET(7,CPU.AF.B.h);return;
  //}
}

static void CodesDDCB()
{
  pair J;
  byte I;

#define XX IX    
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesXXCB[I];
  switch(I) {
#include "CodesXCB.h"
  }
#undef XX
}

static void CodesFDCB()
{
  register pair J;
  register byte I;

#define XX IY
  J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesXXCB[I];
  switch(I)
  {
#include "CodesXCB.h"
  }
#undef XX
}

static void CodesED()
{
  __label__
  DB_00,DB_01,DB_02,DB_03,DB_04,DB_05,DB_06,DB_07,
  DB_08,DB_09,DB_0A,DB_0B,DB_0C,DB_0D,DB_0E,DB_0F,
  DB_10,DB_11,DB_12,DB_13,DB_14,DB_15,DB_16,DB_17,
  DB_18,DB_19,DB_1A,DB_1B,DB_1C,DB_1D,DB_1E,DB_1F,
  DB_20,DB_21,DB_22,DB_23,DB_24,DB_25,DB_26,DB_27,
  DB_28,DB_29,DB_2A,DB_2B,DB_2C,DB_2D,DB_2E,DB_2F,
  DB_30,DB_31,DB_32,DB_33,DB_34,DB_35,DB_36,DB_37,
  DB_38,DB_39,DB_3A,DB_3B,DB_3C,DB_3D,DB_3E,DB_3F,
  IN_B_xC,OUT_xC_B,SBC_HL_BC,LD_xWORDe_BC,NEG,RETN,IM_0,LD_I_A,
  IN_C_xC,OUT_xC_C,ADC_HL_BC,LD_BC_xWORDe,DB_4C,RETI,DB_,LD_R_A,
  IN_D_xC,OUT_xC_D,SBC_HL_DE,LD_xWORDe_DE,DB_54,DB_55,IM_1,LD_A_I,
  IN_E_xC,OUT_xC_E,ADC_HL_DE,LD_DE_xWORDe,DB_5C,DB_5D,IM_2,LD_A_R,
  IN_H_xC,OUT_xC_H,SBC_HL_HL,LD_xWORDe_HL,DB_64,DB_65,DB_66,RRD,
  IN_L_xC,OUT_xC_L,ADC_HL_HL,LD_HL_xWORDe,DB_6C,DB_6D,DB_6E,RLD,
  IN_F_xC,DB_71,SBC_HL_SP,LD_xWORDe_SP,DB_74,DB_75,DB_76,DB_77,
  IN_A_xC,OUT_xC_A,ADC_HL_SP,LD_SP_xWORDe,DB_7C,DB_7D,DB_7E,DB_7F,
  DB_80,DB_81,DB_82,DB_83,DB_84,DB_85,DB_86,DB_87,
  DB_88,DB_89,DB_8A,DB_8B,DB_8C,DB_8D,DB_8E,DB_8F,
  DB_90,DB_91,DB_92,DB_93,DB_94,DB_95,DB_96,DB_97,
  DB_98,DB_99,DB_9A,DB_9B,DB_9C,DB_9D,DB_9E,DB_9F,
  LDI,CPI,INI,OUTI,DB_A4,DB_A5,DB_A6,DB_A7,
  LDD,CPD,IND,OUTD,DB_AC,DB_AD,DB_AE,DB_AF,
  LDIR,CPIR,INIR,OTIR,DB_B4,DB_B5,DB_B6,DB_B7,
  LDDR,CPDR,INDR,OTDR,DB_BC,DB_BD,DB_BE,DB_BF,
  DB_C0,DB_C1,DB_C2,DB_C3,DB_C4,DB_C5,DB_C6,DB_C7,
  DB_C8,DB_C9,DB_CA,DB_CB,DB_CC,DB_CD,DB_CE,DB_CF,
  DB_D0,DB_D1,DB_D2,DB_D3,DB_D4,DB_D5,DB_D6,DB_D7,
  DB_D8,DB_D9,DB_DA,DB_DB,DB_DC,DB_DD,DB_DE,DB_DF,
  DB_E0,DB_E1,DB_E2,DB_E3,DB_E4,DB_E5,DB_E6,DB_E7,
  DB_E8,DB_E9,DB_EA,DB_EB,DB_EC,DB_ED,DB_EE,DB_EF,
  DB_F0,DB_F1,DB_F2,DB_F3,DB_F4,DB_F5,DB_F6,DB_F7,
  DB_F8,DB_F9,DB_FA,DB_FB,DB_FC,DB_FD,DB_FE,DB_FF;

    static const void* const a_jump_table[256] = { &&
  DB_00,&&DB_01,&&DB_02,&&DB_03,&&DB_04,&&DB_05,&&DB_06,&&DB_07,&&
  DB_08,&&DB_09,&&DB_0A,&&DB_0B,&&DB_0C,&&DB_0D,&&DB_0E,&&DB_0F,&&
  DB_10,&&DB_11,&&DB_12,&&DB_13,&&DB_14,&&DB_15,&&DB_16,&&DB_17,&&
  DB_18,&&DB_19,&&DB_1A,&&DB_1B,&&DB_1C,&&DB_1D,&&DB_1E,&&DB_1F,&&
  DB_20,&&DB_21,&&DB_22,&&DB_23,&&DB_24,&&DB_25,&&DB_26,&&DB_27,&&
  DB_28,&&DB_29,&&DB_2A,&&DB_2B,&&DB_2C,&&DB_2D,&&DB_2E,&&DB_2F,&&
  DB_30,&&DB_31,&&DB_32,&&DB_33,&&DB_34,&&DB_35,&&DB_36,&&DB_37,&&
  DB_38,&&DB_39,&&DB_3A,&&DB_3B,&&DB_3C,&&DB_3D,&&DB_3E,&&DB_3F,&&
  IN_B_xC,&&OUT_xC_B,&&SBC_HL_BC,&&LD_xWORDe_BC,&&NEG,&&RETN,&&IM_0,&&LD_I_A,&&
  IN_C_xC,&&OUT_xC_C,&&ADC_HL_BC,&&LD_BC_xWORDe,&&DB_4C,&&RETI,&&DB_,&&LD_R_A,&&
  IN_D_xC,&&OUT_xC_D,&&SBC_HL_DE,&&LD_xWORDe_DE,&&DB_54,&&DB_55,&&IM_1,&&LD_A_I,&&
  IN_E_xC,&&OUT_xC_E,&&ADC_HL_DE,&&LD_DE_xWORDe,&&DB_5C,&&DB_5D,&&IM_2,&&LD_A_R,&&
  IN_H_xC,&&OUT_xC_H,&&SBC_HL_HL,&&LD_xWORDe_HL,&&DB_64,&&DB_65,&&DB_66,&&RRD,&&
  IN_L_xC,&&OUT_xC_L,&&ADC_HL_HL,&&LD_HL_xWORDe,&&DB_6C,&&DB_6D,&&DB_6E,&&RLD,&&
  IN_F_xC,&&DB_71,&&SBC_HL_SP,&&LD_xWORDe_SP,&&DB_74,&&DB_75,&&DB_76,&&DB_77,&&
  IN_A_xC,&&OUT_xC_A,&&ADC_HL_SP,&&LD_SP_xWORDe,&&DB_7C,&&DB_7D,&&DB_7E,&&DB_7F,&&
  DB_80,&&DB_81,&&DB_82,&&DB_83,&&DB_84,&&DB_85,&&DB_86,&&DB_87,&&
  DB_88,&&DB_89,&&DB_8A,&&DB_8B,&&DB_8C,&&DB_8D,&&DB_8E,&&DB_8F,&&
  DB_90,&&DB_91,&&DB_92,&&DB_93,&&DB_94,&&DB_95,&&DB_96,&&DB_97,&&
  DB_98,&&DB_99,&&DB_9A,&&DB_9B,&&DB_9C,&&DB_9D,&&DB_9E,&&DB_9F,&&
  LDI,&&CPI,&&INI,&&OUTI,&&DB_A4,&&DB_A5,&&DB_A6,&&DB_A7,&&
  LDD,&&CPD,&&IND,&&OUTD,&&DB_AC,&&DB_AD,&&DB_AE,&&DB_AF,&&
  LDIR,&&CPIR,&&INIR,&&OTIR,&&DB_B4,&&DB_B5,&&DB_B6,&&DB_B7,&&
  LDDR,&&CPDR,&&INDR,&&OTDR,&&DB_BC,&&DB_BD,&&DB_BE,&&DB_BF,&&
  DB_C0,&&DB_C1,&&DB_C2,&&DB_C3,&&DB_C4,&&DB_C5,&&DB_C6,&&DB_C7,&&
  DB_C8,&&DB_C9,&&DB_CA,&&DB_CB,&&DB_CC,&&DB_CD,&&DB_CE,&&DB_CF,&&
  DB_D0,&&DB_D1,&&DB_D2,&&DB_D3,&&DB_D4,&&DB_D5,&&DB_D6,&&DB_D7,&&
  DB_D8,&&DB_D9,&&DB_DA,&&DB_DB,&&DB_DC,&&DB_DD,&&DB_DE,&&DB_DF,&&
  DB_E0,&&DB_E1,&&DB_E2,&&DB_E3,&&DB_E4,&&DB_E5,&&DB_E6,&&DB_E7,&&
  DB_E8,&&DB_E9,&&DB_EA,&&DB_EB,&&DB_EC,&&DB_ED,&&DB_EE,&&DB_EF,&&
  DB_F0,&&DB_F1,&&DB_F2,&&DB_F3,&&DB_F4,&&DB_F5,&&DB_F6,&&DB_F7,&&
  DB_F8,&&DB_F9,&&DB_FA,&&DB_FB,&&DB_FC,&&DB_FD,&&DB_FE,&&DB_FF 
  };

  byte I;
  pair J;

  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesED[I];
	goto *a_jump_table[I];
  //switch(I) {
DB_FE:     PatchZ80();return;

ADC_HL_BC: M_ADCW(BC);return;
ADC_HL_DE: M_ADCW(DE);return;
ADC_HL_HL: M_ADCW(HL);return;
ADC_HL_SP: M_ADCW(SP);return;

SBC_HL_BC: M_SBCW(BC);return;
SBC_HL_DE: M_SBCW(DE);return;
SBC_HL_HL: M_SBCW(HL);return;
SBC_HL_SP: M_SBCW(SP);return;

LD_xWORDe_HL:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.HL.B.l);
  WrZ80(J.W,CPU.HL.B.h);
  return;
LD_xWORDe_DE:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.DE.B.l);
  WrZ80(J.W,CPU.DE.B.h);
  return;
LD_xWORDe_BC:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.BC.B.l);
  WrZ80(J.W,CPU.BC.B.h);
  return;
LD_xWORDe_SP:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.SP.B.l);
  WrZ80(J.W,CPU.SP.B.h);
  return;

LD_HL_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.HL.B.l=RdZ80(J.W++);
  CPU.HL.B.h=RdZ80(J.W);
  return;
LD_DE_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.DE.B.l=RdZ80(J.W++);
  CPU.DE.B.h=RdZ80(J.W);
  return;
LD_BC_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.BC.B.l=RdZ80(J.W++);
  CPU.BC.B.h=RdZ80(J.W);
  return;
LD_SP_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.SP.B.l=RdZ80(J.W++);
  CPU.SP.B.h=RdZ80(J.W);
  return;

RRD:
  I=RdZ80(CPU.HL.W);
  J.B.l=(I>>4)|(CPU.AF.B.h<<4);
  WrZ80(CPU.HL.W,J.B.l);
  CPU.AF.B.h=(I&0x0F)|(CPU.AF.B.h&0xF0);
  CPU.AF.B.l=PZSTable[CPU.AF.B.h]|(CPU.AF.B.l&C_FLAG);
  return;
RLD:
  I=RdZ80(CPU.HL.W);
  J.B.l=(I<<4)|(CPU.AF.B.h&0x0F);
  WrZ80(CPU.HL.W,J.B.l);
  CPU.AF.B.h=(I>>4)|(CPU.AF.B.h&0xF0);
  CPU.AF.B.l=PZSTable[CPU.AF.B.h]|(CPU.AF.B.l&C_FLAG);
  return;

LD_A_I:
  CPU.AF.B.h=CPU.I;
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|(CPU.IFF&1? P_FLAG:0)|ZSTable[CPU.AF.B.h];
  return;

LD_A_R:
  CPU.R++;
  CPU.AF.B.h=(byte)(CPU.R-CPU.ICount);
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|(CPU.IFF&1? P_FLAG:0)|ZSTable[CPU.AF.B.h];
  return;

LD_I_A:   CPU.I=CPU.AF.B.h;return;
LD_R_A:   return;

IM_0:     CPU.IFF&=0xF9;return;
IM_1:     CPU.IFF=(CPU.IFF&0xF9)|2;return;
IM_2:     CPU.IFF=(CPU.IFF&0xF9)|4;return;

RETI:     M_RET;return;
RETN:     if(CPU.IFF&0x40) CPU.IFF|=0x01; else CPU.IFF&=0xFE;
               M_RET;return;

NEG:      I=CPU.AF.B.h;CPU.AF.B.h=0;M_SUB(I);return;

IN_B_xC:  M_IN(CPU.BC.B.h);return;
IN_C_xC:  M_IN(CPU.BC.B.l);return;
IN_D_xC:  M_IN(CPU.DE.B.h);return;
IN_E_xC:  M_IN(CPU.DE.B.l);return;
IN_H_xC:  M_IN(CPU.HL.B.h);return;
IN_L_xC:  M_IN(CPU.HL.B.l);return;
IN_A_xC:  M_IN(CPU.AF.B.h);return;
IN_F_xC:  M_IN(J.B.l);return;

OUT_xC_B: OutZ80(CPU.BC.B.l,CPU.BC.B.h);return;
OUT_xC_C: OutZ80(CPU.BC.B.l,CPU.BC.B.l);return;
OUT_xC_D: OutZ80(CPU.BC.B.l,CPU.DE.B.h);return;
OUT_xC_E: OutZ80(CPU.BC.B.l,CPU.DE.B.l);return;
OUT_xC_H: OutZ80(CPU.BC.B.l,CPU.HL.B.h);return;
OUT_xC_L: OutZ80(CPU.BC.B.l,CPU.HL.B.l);return;
OUT_xC_A: OutZ80(CPU.BC.B.l,CPU.AF.B.h);return;

INI:
  WrZ80(CPU.HL.W++,InZ80(CPU.BC.B.l));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  return;

INIR:
  do
  {
    WrZ80(CPU.HL.W++,InZ80(CPU.BC.B.l));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  return;

IND:
  WrZ80(CPU.HL.W--,InZ80(CPU.BC.B.l));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  return;

INDR:
  do
  {
    WrZ80(CPU.HL.W--,InZ80(CPU.BC.B.l));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  return;

OUTI:
  OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W++));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  return;

OTIR:
  do
  {
    OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W++));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  return;

OUTD:
  OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W--));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  return;

OTDR:
  do
  {
    OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W--));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  return;

LDI:
  WrZ80(CPU.DE.W++,RdZ80(CPU.HL.W++));
  CPU.BC.W--;
  CPU.AF.B.l=(CPU.AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(CPU.BC.W? P_FLAG:0);
  return;

LDIR:
  do
  {
    WrZ80(CPU.DE.W++,RdZ80(CPU.HL.W++));
    CPU.BC.W--;CPU.ICount-=21;
  }
  while(CPU.BC.W&&(CPU.ICount>0));
  CPU.AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(CPU.BC.W) { CPU.AF.B.l|=N_FLAG;CPU.PC.W-=2; }
  else CPU.ICount+=5;
  return;

LDD:
  WrZ80(CPU.DE.W--,RdZ80(CPU.HL.W--));
  CPU.BC.W--;
  CPU.AF.B.l=(CPU.AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(CPU.BC.W? P_FLAG:0);
  return;

LDDR:
  do
  {
    WrZ80(CPU.DE.W--,RdZ80(CPU.HL.W--));
    CPU.BC.W--;CPU.ICount-=21;
  }
  while(CPU.BC.W&&(CPU.ICount>0));
  CPU.AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(CPU.BC.W) { CPU.AF.B.l|=N_FLAG;CPU.PC.W-=2; }
  else CPU.ICount+=5;
  return;

CPI:
  I=RdZ80(CPU.HL.W++);
  J.B.l=CPU.AF.B.h-I;
  CPU.BC.W--;
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  return;

CPIR:
  do
  {
    I=RdZ80(CPU.HL.W++);
    J.B.l=CPU.AF.B.h-I;
    CPU.BC.W--;CPU.ICount-=21;
  }  
  while(CPU.BC.W&&J.B.l&&(CPU.ICount>0));
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  if(CPU.BC.W&&J.B.l) CPU.PC.W-=2; else CPU.ICount+=5;
  return;  

CPD:
  I=RdZ80(CPU.HL.W--);
  J.B.l=CPU.AF.B.h-I;
  CPU.BC.W--;
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  return;

CPDR:
  do
  {
    I=RdZ80(CPU.HL.W--);
    J.B.l=CPU.AF.B.h-I;
    CPU.BC.W--;CPU.ICount-=21;
  }
  while(CPU.BC.W&&J.B.l);
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  if(CPU.BC.W&&J.B.l) CPU.PC.W-=2; else CPU.ICount+=5;
  return;
    DB_ED:
      CPU.PC.W--;return;

DB_FF:
DB_FD:
DB_FC:
DB_FB:
DB_FA:
DB_F9:
DB_F8:
DB_F7:
DB_F6:
DB_F5:
DB_F4:
DB_F3:
DB_F2:
DB_F1:
DB_F0:
DB_EF:
DB_EE:
DB_EC:
DB_EB:
DB_EA:
DB_E9:
DB_E8:
DB_E7:
DB_E6:
DB_E5:
DB_E4:
DB_E3:
DB_E2:
DB_E1:
DB_E0:
DB_DF:
DB_DE:
DB_DD:
DB_DC:
DB_DB:
DB_DA:
DB_D9:
DB_D8:
DB_D7:
DB_D6:
DB_D5:
DB_D4:
DB_D3:
DB_D2:
DB_D1:
DB_D0:
DB_CF:
DB_CE:
DB_CD:
DB_CC:
DB_CB:
DB_CA:
DB_C9:
DB_C8:
DB_C7:
DB_C6:
DB_C5:
DB_C4:
DB_C3:
DB_C2:
DB_C1:
DB_C0:
DB_BF:
DB_BE:
DB_BD:
DB_BC:
DB_B7:
DB_B6:
DB_B5:
DB_B4:
DB_AF:
DB_AE:
DB_AD:
DB_AC:
DB_A7:
DB_A6:
DB_A5:
DB_A4:
DB_9F:
DB_9E:
DB_9D:
DB_9C:
DB_9B:
DB_9A:
DB_99:
DB_98:
DB_97:
DB_96:
DB_95:
DB_94:
DB_93:
DB_92:
DB_91:
DB_90:
DB_8F:
DB_8E:
DB_8D:
DB_8C:
DB_8B:
DB_8A:
DB_89:
DB_88:
DB_87:
DB_86:
DB_85:
DB_84:
DB_83:
DB_82:
DB_81:
DB_80:
DB_7F:
DB_7E:
DB_7D:
DB_7C:
DB_77:
DB_76:
DB_75:
DB_74:
DB_71:
DB_6E:
DB_6D:
DB_6C:
DB_66:
DB_65:
DB_64:
DB_5D:
DB_5C:
DB_55:
DB_54:
DB_:
DB_4C:
DB_3F:
DB_3E:
DB_3D:
DB_3C:
DB_3B:
DB_3A:
DB_39:
DB_38:
DB_37:
DB_36:
DB_35:
DB_34:
DB_33:
DB_32:
DB_31:
DB_30:
DB_2F:
DB_2E:
DB_2D:
DB_2C:
DB_2B:
DB_2A:
DB_29:
DB_28:
DB_27:
DB_26:
DB_25:
DB_24:
DB_23:
DB_22:
DB_21:
DB_20:
DB_1F:
DB_1E:
DB_1D:
DB_1C:
DB_1B:
DB_1A:
DB_19:
DB_18:
DB_17:
DB_16:
DB_15:
DB_14:
DB_13:
DB_12:
DB_11:
DB_10:
DB_0F:
DB_0E:
DB_0D:
DB_0C:
DB_0B:
DB_0A:
DB_09:
DB_08:
DB_07:
DB_06:
DB_05:
DB_04:
DB_03:
DB_02:
DB_01:
DB_00:
  return;
  //}
}

static void CodesDD()
{
  __label__
  NOP,LD_BC_WORD,LD_xBC_A,INC_BC,INC_B,DEC_B,LD_B_BYTE,RLCA,
  EX_AF_AF,ADD_HL_BC,LD_A_xBC,DEC_BC,INC_C,DEC_C,LD_C_BYTE,RRCA,
  DJNZ,LD_DE_WORD,LD_xDE_A,INC_DE,INC_D,DEC_D,LD_D_BYTE,RLA,
  JR,ADD_HL_DE,LD_A_xDE,DEC_DE,INC_E,DEC_E,LD_E_BYTE,RRA,
  JR_NZ,LD_HL_WORD,LD_xWORD_HL,INC_HL,INC_H,DEC_H,LD_H_BYTE,DAA,
  JR_Z,ADD_HL_HL,LD_HL_xWORD,DEC_HL,INC_L,DEC_L,LD_L_BYTE,CPL,
  JR_NC,LD_SP_WORD,LD_xWORD_A,INC_SP,INC_xHL,DEC_xHL,LD_xHL_BYTE,SCF,
  JR_C,ADD_HL_SP,LD_A_xWORD,DEC_SP,INC_A,DEC_A,LD_A_BYTE,CCF,
  LD_B_B,LD_B_C,LD_B_D,LD_B_E,LD_B_H,LD_B_L,LD_B_xHL,LD_B_A,
  LD_C_B,LD_C_C,LD_C_D,LD_C_E,LD_C_H,LD_C_L,LD_C_xHL,LD_C_A,
  LD_D_B,LD_D_C,LD_D_D,LD_D_E,LD_D_H,LD_D_L,LD_D_xHL,LD_D_A,
  LD_E_B,LD_E_C,LD_E_D,LD_E_E,LD_E_H,LD_E_L,LD_E_xHL,LD_E_A,
  LD_H_B,LD_H_C,LD_H_D,LD_H_E,LD_H_H,LD_H_L,LD_H_xHL,LD_H_A,
  LD_L_B,LD_L_C,LD_L_D,LD_L_E,LD_L_H,LD_L_L,LD_L_xHL,LD_L_A,
  LD_xHL_B,LD_xHL_C,LD_xHL_D,LD_xHL_E,LD_xHL_H,LD_xHL_L,HALT,LD_xHL_A,
  LD_A_B,LD_A_C,LD_A_D,LD_A_E,LD_A_H,LD_A_L,LD_A_xHL,LD_A_A,
  ADD_B,ADD_C,ADD_D,ADD_E,ADD_H,ADD_L,ADD_xHL,ADD_A,
  ADC_B,ADC_C,ADC_D,ADC_E,ADC_H,ADC_L,ADC_xHL,ADC_A,
  SUB_B,SUB_C,SUB_D,SUB_E,SUB_H,SUB_L,SUB_xHL,SUB_A,
  SBC_B,SBC_C,SBC_D,SBC_E,SBC_H,SBC_L,SBC_xHL,SBC_A,
  AND_B,AND_C,AND_D,AND_E,AND_H,AND_L,AND_xHL,AND_A,
  XOR_B,XOR_C,XOR_D,XOR_E,XOR_H,XOR_L,XOR_xHL,XOR_A,
  OR_B,OR_C,OR_D,OR_E,OR_H,OR_L,OR_xHL,OR_A,
  CP_B,CP_C,CP_D,CP_E,CP_H,CP_L,CP_xHL,CP_A,
  RET_NZ,POP_BC,JP_NZ,JP,CALL_NZ,PUSH_BC,ADD_BYTE,RST00,
  RET_Z,RET,JP_Z,PFX_CB,CALL_Z,CALL,ADC_BYTE,RST08,
  RET_NC,POP_DE,JP_NC,OUTA,CALL_NC,PUSH_DE,SUB_BYTE,RST10,
  RET_C,EXX,JP_C,INA,CALL_C,PFX_DD,SBC_BYTE,RST18,
  RET_PO,POP_HL,JP_PO,EX_HL_xSP,CALL_PO,PUSH_HL,AND_BYTE,RST20,
  RET_PE,LD_PC_HL,JP_PE,EX_DE_HL,CALL_PE,PFX_ED,XOR_BYTE,RST28,
  RET_P,POP_AF,JP_P,DI,CALL_P,PUSH_AF,OR_BYTE,RST30,
  RET_M,LD_SP_HL,JP_M,EI,CALL_M,PFX_FD,CP_BYTE,RST38;

    static const void* const a_jump_table[256] = { &&
  NOP,&&LD_BC_WORD,&&LD_xBC_A,&&INC_BC,&&INC_B,&&DEC_B,&&LD_B_BYTE,&&RLCA,&&
  EX_AF_AF,&&ADD_HL_BC,&&LD_A_xBC,&&DEC_BC,&&INC_C,&&DEC_C,&&LD_C_BYTE,&&RRCA,&&
  DJNZ,&&LD_DE_WORD,&&LD_xDE_A,&&INC_DE,&&INC_D,&&DEC_D,&&LD_D_BYTE,&&RLA,&&
  JR,&&ADD_HL_DE,&&LD_A_xDE,&&DEC_DE,&&INC_E,&&DEC_E,&&LD_E_BYTE,&&RRA,&&
  JR_NZ,&&LD_HL_WORD,&&LD_xWORD_HL,&&INC_HL,&&INC_H,&&DEC_H,&&LD_H_BYTE,&&DAA,&&
  JR_Z,&&ADD_HL_HL,&&LD_HL_xWORD,&&DEC_HL,&&INC_L,&&DEC_L,&&LD_L_BYTE,&&CPL,&&
  JR_NC,&&LD_SP_WORD,&&LD_xWORD_A,&&INC_SP,&&INC_xHL,&&DEC_xHL,&&LD_xHL_BYTE,&&SCF,&&
  JR_C,&&ADD_HL_SP,&&LD_A_xWORD,&&DEC_SP,&&INC_A,&&DEC_A,&&LD_A_BYTE,&&CCF,&&
  LD_B_B,&&LD_B_C,&&LD_B_D,&&LD_B_E,&&LD_B_H,&&LD_B_L,&&LD_B_xHL,&&LD_B_A,&&
  LD_C_B,&&LD_C_C,&&LD_C_D,&&LD_C_E,&&LD_C_H,&&LD_C_L,&&LD_C_xHL,&&LD_C_A,&&
  LD_D_B,&&LD_D_C,&&LD_D_D,&&LD_D_E,&&LD_D_H,&&LD_D_L,&&LD_D_xHL,&&LD_D_A,&&
  LD_E_B,&&LD_E_C,&&LD_E_D,&&LD_E_E,&&LD_E_H,&&LD_E_L,&&LD_E_xHL,&&LD_E_A,&&
  LD_H_B,&&LD_H_C,&&LD_H_D,&&LD_H_E,&&LD_H_H,&&LD_H_L,&&LD_H_xHL,&&LD_H_A,&&
  LD_L_B,&&LD_L_C,&&LD_L_D,&&LD_L_E,&&LD_L_H,&&LD_L_L,&&LD_L_xHL,&&LD_L_A,&&
  LD_xHL_B,&&LD_xHL_C,&&LD_xHL_D,&&LD_xHL_E,&&LD_xHL_H,&&LD_xHL_L,&&HALT,&&LD_xHL_A,&&
  LD_A_B,&&LD_A_C,&&LD_A_D,&&LD_A_E,&&LD_A_H,&&LD_A_L,&&LD_A_xHL,&&LD_A_A,&&
  ADD_B,&&ADD_C,&&ADD_D,&&ADD_E,&&ADD_H,&&ADD_L,&&ADD_xHL,&&ADD_A,&&
  ADC_B,&&ADC_C,&&ADC_D,&&ADC_E,&&ADC_H,&&ADC_L,&&ADC_xHL,&&ADC_A,&&
  SUB_B,&&SUB_C,&&SUB_D,&&SUB_E,&&SUB_H,&&SUB_L,&&SUB_xHL,&&SUB_A,&&
  SBC_B,&&SBC_C,&&SBC_D,&&SBC_E,&&SBC_H,&&SBC_L,&&SBC_xHL,&&SBC_A,&&
  AND_B,&&AND_C,&&AND_D,&&AND_E,&&AND_H,&&AND_L,&&AND_xHL,&&AND_A,&&
  XOR_B,&&XOR_C,&&XOR_D,&&XOR_E,&&XOR_H,&&XOR_L,&&XOR_xHL,&&XOR_A,&&
  OR_B,&&OR_C,&&OR_D,&&OR_E,&&OR_H,&&OR_L,&&OR_xHL,&&OR_A,&&
  CP_B,&&CP_C,&&CP_D,&&CP_E,&&CP_H,&&CP_L,&&CP_xHL,&&CP_A,&&
  RET_NZ,&&POP_BC,&&JP_NZ,&&JP,&&CALL_NZ,&&PUSH_BC,&&ADD_BYTE,&&RST00,&&
  RET_Z,&&RET,&&JP_Z,&&PFX_CB,&&CALL_Z,&&CALL,&&ADC_BYTE,&&RST08,&&
  RET_NC,&&POP_DE,&&JP_NC,&&OUTA,&&CALL_NC,&&PUSH_DE,&&SUB_BYTE,&&RST10,&&
  RET_C,&&EXX,&&JP_C,&&INA,&&CALL_C,&&PFX_DD,&&SBC_BYTE,&&RST18,&&
  RET_PO,&&POP_HL,&&JP_PO,&&EX_HL_xSP,&&CALL_PO,&&PUSH_HL,&&AND_BYTE,&&RST20,&&
  RET_PE,&&LD_PC_HL,&&JP_PE,&&EX_DE_HL,&&CALL_PE,&&PFX_ED,&&XOR_BYTE,&&RST28,&&
  RET_P,&&POP_AF,&&JP_P,&&DI,&&CALL_P,&&PUSH_AF,&&OR_BYTE,&&RST30,&&
  RET_M,&&LD_SP_HL,&&JP_M,&&EI,&&CALL_M,&&PFX_FD,&&CP_BYTE,&&RST38 };

  byte I;
  pair J;

#define XX IX
  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesXX[I];
	goto *a_jump_table[I];
  //switch(I) {
JR_NZ:    if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W++; else { M_JR; } return;
JR_NC:    if(CPU.AF.B.l&C_FLAG)  CPU.PC.W++; else { M_JR; } return;
JR_Z:     if(CPU.AF.B.l&Z_FLAG)  { M_JR; } else CPU.PC.W++; return;
JR_C:     if(CPU.AF.B.l&C_FLAG)  { M_JR; } else CPU.PC.W++; return;

JP_NZ:    if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_NC:    if(CPU.AF.B.l&C_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_PO:    if(CPU.AF.B.l&P_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_P:     if(CPU.AF.B.l&S_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_Z:     if(CPU.AF.B.l&Z_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_C:     if(CPU.AF.B.l&C_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_PE:    if(CPU.AF.B.l&P_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_M:     if(CPU.AF.B.l&S_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;

RET_NZ:   if(!(CPU.AF.B.l&Z_FLAG))  { M_RET; }  return;
RET_NC:   if(!(CPU.AF.B.l&C_FLAG))  { M_RET; }  return;
RET_PO:   if(!(CPU.AF.B.l&P_FLAG))  { M_RET; }  return;
RET_P:    if(!(CPU.AF.B.l&S_FLAG))  { M_RET; }  return;
RET_Z:    if(CPU.AF.B.l&Z_FLAG)     { M_RET; }  return;
RET_C:    if(CPU.AF.B.l&C_FLAG)     { M_RET; }  return;
RET_PE:   if(CPU.AF.B.l&P_FLAG)     { M_RET; }  return;
RET_M:    if(CPU.AF.B.l&S_FLAG)     { M_RET; }  return;

CALL_NZ:  if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_NC:  if(CPU.AF.B.l&C_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_PO:  if(CPU.AF.B.l&P_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_P:   if(CPU.AF.B.l&S_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_Z:   if(CPU.AF.B.l&Z_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_C:   if(CPU.AF.B.l&C_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_PE:  if(CPU.AF.B.l&P_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_M:   if(CPU.AF.B.l&S_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;

ADD_B:    M_ADD(CPU.BC.B.h);return;
ADD_C:    M_ADD(CPU.BC.B.l);return;
ADD_D:    M_ADD(CPU.DE.B.h);return;
ADD_E:    M_ADD(CPU.DE.B.l);return;
ADD_H:    M_ADD(CPU.XX.B.h);return;
ADD_L:    M_ADD(CPU.XX.B.l);return;
ADD_A:    M_ADD(CPU.AF.B.h);return;
ADD_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_ADD(I);return;
ADD_BYTE: I=RdZ80(CPU.PC.W++);M_ADD(I);return;

SUB_B:    M_SUB(CPU.BC.B.h);return;
SUB_C:    M_SUB(CPU.BC.B.l);return;
SUB_D:    M_SUB(CPU.DE.B.h);return;
SUB_E:    M_SUB(CPU.DE.B.l);return;
SUB_H:    M_SUB(CPU.XX.B.h);return;
SUB_L:    M_SUB(CPU.XX.B.l);return;
SUB_A:    CPU.AF.B.h=0;CPU.AF.B.l=N_FLAG|Z_FLAG;return;
SUB_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_SUB(I);return;
SUB_BYTE: I=RdZ80(CPU.PC.W++);M_SUB(I);return;

AND_B:    M_AND(CPU.BC.B.h);return;
AND_C:    M_AND(CPU.BC.B.l);return;
AND_D:    M_AND(CPU.DE.B.h);return;
AND_E:    M_AND(CPU.DE.B.l);return;
AND_H:    M_AND(CPU.XX.B.h);return;
AND_L:    M_AND(CPU.XX.B.l);return;
AND_A:    M_AND(CPU.AF.B.h);return;
AND_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_AND(I);return;
AND_BYTE: I=RdZ80(CPU.PC.W++);M_AND(I);return;

OR_B:     M_OR(CPU.BC.B.h);return;
OR_C:     M_OR(CPU.BC.B.l);return;
OR_D:     M_OR(CPU.DE.B.h);return;
OR_E:     M_OR(CPU.DE.B.l);return;
OR_H:     M_OR(CPU.XX.B.h);return;
OR_L:     M_OR(CPU.XX.B.l);return;
OR_A:     M_OR(CPU.AF.B.h);return;
OR_xHL:   I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_OR(I);return;
OR_BYTE:  I=RdZ80(CPU.PC.W++);M_OR(I);return;

ADC_B:    M_ADC(CPU.BC.B.h);return;
ADC_C:    M_ADC(CPU.BC.B.l);return;
ADC_D:    M_ADC(CPU.DE.B.h);return;
ADC_E:    M_ADC(CPU.DE.B.l);return;
ADC_H:    M_ADC(CPU.XX.B.h);return;
ADC_L:    M_ADC(CPU.XX.B.l);return;
ADC_A:    M_ADC(CPU.AF.B.h);return;
ADC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_ADC(I);return;
ADC_BYTE: I=RdZ80(CPU.PC.W++);M_ADC(I);return;

SBC_B:    M_SBC(CPU.BC.B.h);return;
SBC_C:    M_SBC(CPU.BC.B.l);return;
SBC_D:    M_SBC(CPU.DE.B.h);return;
SBC_E:    M_SBC(CPU.DE.B.l);return;
SBC_H:    M_SBC(CPU.XX.B.h);return;
SBC_L:    M_SBC(CPU.XX.B.l);return;
SBC_A:    M_SBC(CPU.AF.B.h);return;
SBC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_SBC(I);return;
SBC_BYTE: I=RdZ80(CPU.PC.W++);M_SBC(I);return;

XOR_B:    M_XOR(CPU.BC.B.h);return;
XOR_C:    M_XOR(CPU.BC.B.l);return;
XOR_D:    M_XOR(CPU.DE.B.h);return;
XOR_E:    M_XOR(CPU.DE.B.l);return;
XOR_H:    M_XOR(CPU.XX.B.h);return;
XOR_L:    M_XOR(CPU.XX.B.l);return;
XOR_A:    CPU.AF.B.h=0;CPU.AF.B.l=P_FLAG|Z_FLAG;return;
XOR_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_XOR(I);return;
XOR_BYTE: I=RdZ80(CPU.PC.W++);M_XOR(I);return;

CP_B:     M_CP(CPU.BC.B.h);return;
CP_C:     M_CP(CPU.BC.B.l);return;
CP_D:     M_CP(CPU.DE.B.h);return;
CP_E:     M_CP(CPU.DE.B.l);return;
CP_H:     M_CP(CPU.XX.B.h);return;
CP_L:     M_CP(CPU.XX.B.l);return;
CP_A:     CPU.AF.B.l=N_FLAG|Z_FLAG;return;
CP_xHL:   I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_CP(I);return;
CP_BYTE:  I=RdZ80(CPU.PC.W++);M_CP(I);return;
               
LD_BC_WORD: M_LDWORD(BC);return;
LD_DE_WORD: M_LDWORD(DE);return;
LD_HL_WORD: M_LDWORD(XX);return;
LD_SP_WORD: M_LDWORD(SP);return;

LD_PC_HL: CPU.PC.W=CPU.XX.W;return;
LD_SP_HL: CPU.SP.W=CPU.XX.W;return;
LD_A_xBC: CPU.AF.B.h=RdZ80(CPU.BC.W);return;
LD_A_xDE: CPU.AF.B.h=RdZ80(CPU.DE.W);return;

ADD_HL_BC:  M_ADDW(XX,BC);return;
ADD_HL_DE:  M_ADDW(XX,DE);return;
ADD_HL_HL:  M_ADDW(XX,XX);return;
ADD_HL_SP:  M_ADDW(XX,SP);return;

DEC_BC:   CPU.BC.W--;return;
DEC_DE:   CPU.DE.W--;return;
DEC_HL:   CPU.XX.W--;return;
DEC_SP:   CPU.SP.W--;return;

INC_BC:   CPU.BC.W++;return;
INC_DE:   CPU.DE.W++;return;
INC_HL:   CPU.XX.W++;return;
INC_SP:   CPU.SP.W++;return;

DEC_B:    M_DEC(CPU.BC.B.h);return;
DEC_C:    M_DEC(CPU.BC.B.l);return;
DEC_D:    M_DEC(CPU.DE.B.h);return;
DEC_E:    M_DEC(CPU.DE.B.l);return;
DEC_H:    M_DEC(CPU.XX.B.h);return;
DEC_L:    M_DEC(CPU.XX.B.l);return;
DEC_A:    M_DEC(CPU.AF.B.h);return;
DEC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_DEC(I);
               WrZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++),I);
               return;

INC_B:    M_INC(CPU.BC.B.h);return;
INC_C:    M_INC(CPU.BC.B.l);return;
INC_D:    M_INC(CPU.DE.B.h);return;
INC_E:    M_INC(CPU.DE.B.l);return;
INC_H:    M_INC(CPU.XX.B.h);return;
INC_L:    M_INC(CPU.XX.B.l);return;
INC_A:    M_INC(CPU.AF.B.h);return;
INC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_INC(I);
               WrZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++),I);
               return;

RLCA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|I;
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RLA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|(CPU.AF.B.l&C_FLAG);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RRCA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(I? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RRA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(CPU.AF.B.l&C_FLAG? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;

RST00:    M_RST(0x0000);return;
RST08:    M_RST(0x0008);return;
RST10:    M_RST(0x0010);return;
RST18:    M_RST(0x0018);return;
RST20:    M_RST(0x0020);return;
RST28:    M_RST(0x0028);return;
RST30:    M_RST(0x0030);return;
RST38:    M_RST(0x0038);return;

PUSH_BC:  M_PUSH(BC);return;
PUSH_DE:  M_PUSH(DE);return;
PUSH_HL:  M_PUSH(XX);return;
PUSH_AF:  M_PUSH(AF);return;

POP_BC:   M_POP(BC);return;
POP_DE:   M_POP(DE);return;
POP_HL:   M_POP(XX);return;
POP_AF:   M_POP(AF);return;

DJNZ: if(--CPU.BC.B.h) { M_JR; } else CPU.PC.W++;return;
JP:   M_JP;return;
JR:   M_JR;return;
CALL: M_CALL;return;
RET:  M_RET;return;
SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);return;
CPL:  CPU.AF.B.h=~CPU.AF.B.h;S(N_FLAG|H_FLAG);return;
NOP:  return;
OUTA: OutZ80(RdZ80(CPU.PC.W++),CPU.AF.B.h);return;
INA:  CPU.AF.B.h=InZ80(RdZ80(CPU.PC.W++));return;

DI:   
  CPU.IFF&=0xFE;
  return;  
EI:
  CPU.IFF|=0x01;
  if(CPU.IRequest!=INT_NONE)
  {
    CPU.IFF|=0x20;
    CPU.IBackup=CPU.ICount;
    CPU.ICount=1;
  }
  return;

CCF:
  CPU.AF.B.l^=C_FLAG;R(N_FLAG|H_FLAG);
  CPU.AF.B.l|=CPU.AF.B.l&C_FLAG? 0:H_FLAG;
  return;

EXX:
  J.W=CPU.BC.W;CPU.BC.W=CPU.BC1.W;CPU.BC1.W=J.W;
  J.W=CPU.DE.W;CPU.DE.W=CPU.DE1.W;CPU.DE1.W=J.W;
  J.W=CPU.HL.W;CPU.HL.W=CPU.HL1.W;CPU.HL1.W=J.W;
  return;

EX_DE_HL: J.W=CPU.DE.W;CPU.DE.W=CPU.HL.W;CPU.HL.W=J.W;return;
EX_AF_AF: J.W=CPU.AF.W;CPU.AF.W=CPU.AF1.W;CPU.AF1.W=J.W;return;  
  
LD_B_B:   CPU.BC.B.h=CPU.BC.B.h;return;
LD_C_B:   CPU.BC.B.l=CPU.BC.B.h;return;
LD_D_B:   CPU.DE.B.h=CPU.BC.B.h;return;
LD_E_B:   CPU.DE.B.l=CPU.BC.B.h;return;
LD_H_B:   CPU.XX.B.h=CPU.BC.B.h;return;
LD_L_B:   CPU.XX.B.l=CPU.BC.B.h;return;
LD_A_B:   CPU.AF.B.h=CPU.BC.B.h;return;
LD_xHL_B: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.h);return;

LD_B_C:   CPU.BC.B.h=CPU.BC.B.l;return;
LD_C_C:   CPU.BC.B.l=CPU.BC.B.l;return;
LD_D_C:   CPU.DE.B.h=CPU.BC.B.l;return;
LD_E_C:   CPU.DE.B.l=CPU.BC.B.l;return;
LD_H_C:   CPU.XX.B.h=CPU.BC.B.l;return;
LD_L_C:   CPU.XX.B.l=CPU.BC.B.l;return;
LD_A_C:   CPU.AF.B.h=CPU.BC.B.l;return;
LD_xHL_C: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.l);return;

LD_B_D:   CPU.BC.B.h=CPU.DE.B.h;return;
LD_C_D:   CPU.BC.B.l=CPU.DE.B.h;return;
LD_D_D:   CPU.DE.B.h=CPU.DE.B.h;return;
LD_E_D:   CPU.DE.B.l=CPU.DE.B.h;return;
LD_H_D:   CPU.XX.B.h=CPU.DE.B.h;return;
LD_L_D:   CPU.XX.B.l=CPU.DE.B.h;return;
LD_A_D:   CPU.AF.B.h=CPU.DE.B.h;return;
LD_xHL_D: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.h);return;

LD_B_E:   CPU.BC.B.h=CPU.DE.B.l;return;
LD_C_E:   CPU.BC.B.l=CPU.DE.B.l;return;
LD_D_E:   CPU.DE.B.h=CPU.DE.B.l;return;
LD_E_E:   CPU.DE.B.l=CPU.DE.B.l;return;
LD_H_E:   CPU.XX.B.h=CPU.DE.B.l;return;
LD_L_E:   CPU.XX.B.l=CPU.DE.B.l;return;
LD_A_E:   CPU.AF.B.h=CPU.DE.B.l;return;
LD_xHL_E: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.l);return;

LD_B_H:   CPU.BC.B.h=CPU.XX.B.h;return;
LD_C_H:   CPU.BC.B.l=CPU.XX.B.h;return;
LD_D_H:   CPU.DE.B.h=CPU.XX.B.h;return;
LD_E_H:   CPU.DE.B.l=CPU.XX.B.h;return;
LD_H_H:   CPU.XX.B.h=CPU.XX.B.h;return;
LD_L_H:   CPU.XX.B.l=CPU.XX.B.h;return;
LD_A_H:   CPU.AF.B.h=CPU.XX.B.h;return;
LD_xHL_H: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.h);return;

LD_B_L:   CPU.BC.B.h=CPU.XX.B.l;return;
LD_C_L:   CPU.BC.B.l=CPU.XX.B.l;return;
LD_D_L:   CPU.DE.B.h=CPU.XX.B.l;return;
LD_E_L:   CPU.DE.B.l=CPU.XX.B.l;return;
LD_H_L:   CPU.XX.B.h=CPU.XX.B.l;return;
LD_L_L:   CPU.XX.B.l=CPU.XX.B.l;return;
LD_A_L:   CPU.AF.B.h=CPU.XX.B.l;return;
LD_xHL_L: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.l);return;

LD_B_A:   CPU.BC.B.h=CPU.AF.B.h;return;
LD_C_A:   CPU.BC.B.l=CPU.AF.B.h;return;
LD_D_A:   CPU.DE.B.h=CPU.AF.B.h;return;
LD_E_A:   CPU.DE.B.l=CPU.AF.B.h;return;
LD_H_A:   CPU.XX.B.h=CPU.AF.B.h;return;
LD_L_A:   CPU.XX.B.l=CPU.AF.B.h;return;
LD_A_A:   CPU.AF.B.h=CPU.AF.B.h;return;
LD_xHL_A: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.AF.B.h);return;

LD_xBC_A: WrZ80(CPU.BC.W,CPU.AF.B.h);return;
LD_xDE_A: WrZ80(CPU.DE.W,CPU.AF.B.h);return;

LD_B_xHL:    CPU.BC.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_C_xHL:    CPU.BC.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_D_xHL:    CPU.DE.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_E_xHL:    CPU.DE.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_H_xHL:    CPU.HL.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_L_xHL:    CPU.HL.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_A_xHL:    CPU.AF.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;

LD_B_BYTE:   CPU.BC.B.h=RdZ80(CPU.PC.W++);return;
LD_C_BYTE:   CPU.BC.B.l=RdZ80(CPU.PC.W++);return;
LD_D_BYTE:   CPU.DE.B.h=RdZ80(CPU.PC.W++);return;
LD_E_BYTE:   CPU.DE.B.l=RdZ80(CPU.PC.W++);return;
LD_H_BYTE:   CPU.XX.B.h=RdZ80(CPU.PC.W++);return;
LD_L_BYTE:   CPU.XX.B.l=RdZ80(CPU.PC.W++);return;
LD_A_BYTE:   CPU.AF.B.h=RdZ80(CPU.PC.W++);return;
LD_xHL_BYTE: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
                  WrZ80(J.W,RdZ80(CPU.PC.W++));return;

LD_xWORD_HL:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.XX.B.l);
  WrZ80(J.W,CPU.XX.B.h);
  return;

LD_HL_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.XX.B.l=RdZ80(J.W++);
  CPU.XX.B.h=RdZ80(J.W);
  return;

LD_A_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.AF.B.h=RdZ80(J.W);
  return;

LD_xWORD_A:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W,CPU.AF.B.h);
  return;

EX_HL_xSP:
  J.B.l=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W++,CPU.XX.B.l);
  J.B.h=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W--,CPU.XX.B.h);
  CPU.XX.W=J.W;
  return;

DAA:
  J.W=CPU.AF.B.h;
  if(CPU.AF.B.l&C_FLAG) J.W|=256;
  if(CPU.AF.B.l&H_FLAG) J.W|=512;
  if(CPU.AF.B.l&N_FLAG) J.W|=1024;
  CPU.AF.W=DAATable[J.W];
  return;
    PFX_FD:
    PFX_DD:
      CPU.PC.W--;return;
    PFX_CB:
      CodesDDCB(CPU);return;
    HALT:
      CPU.PC.W--;CPU.IFF|=0x80;CPU.ICount=0;return;
PFX_ED: 
   return;
  //}
#undef XX
}

static void CodesFD()
{
  __label__
  NOP,LD_BC_WORD,LD_xBC_A,INC_BC,INC_B,DEC_B,LD_B_BYTE,RLCA,
  EX_AF_AF,ADD_HL_BC,LD_A_xBC,DEC_BC,INC_C,DEC_C,LD_C_BYTE,RRCA,
  DJNZ,LD_DE_WORD,LD_xDE_A,INC_DE,INC_D,DEC_D,LD_D_BYTE,RLA,
  JR,ADD_HL_DE,LD_A_xDE,DEC_DE,INC_E,DEC_E,LD_E_BYTE,RRA,
  JR_NZ,LD_HL_WORD,LD_xWORD_HL,INC_HL,INC_H,DEC_H,LD_H_BYTE,DAA,
  JR_Z,ADD_HL_HL,LD_HL_xWORD,DEC_HL,INC_L,DEC_L,LD_L_BYTE,CPL,
  JR_NC,LD_SP_WORD,LD_xWORD_A,INC_SP,INC_xHL,DEC_xHL,LD_xHL_BYTE,SCF,
  JR_C,ADD_HL_SP,LD_A_xWORD,DEC_SP,INC_A,DEC_A,LD_A_BYTE,CCF,
  LD_B_B,LD_B_C,LD_B_D,LD_B_E,LD_B_H,LD_B_L,LD_B_xHL,LD_B_A,
  LD_C_B,LD_C_C,LD_C_D,LD_C_E,LD_C_H,LD_C_L,LD_C_xHL,LD_C_A,
  LD_D_B,LD_D_C,LD_D_D,LD_D_E,LD_D_H,LD_D_L,LD_D_xHL,LD_D_A,
  LD_E_B,LD_E_C,LD_E_D,LD_E_E,LD_E_H,LD_E_L,LD_E_xHL,LD_E_A,
  LD_H_B,LD_H_C,LD_H_D,LD_H_E,LD_H_H,LD_H_L,LD_H_xHL,LD_H_A,
  LD_L_B,LD_L_C,LD_L_D,LD_L_E,LD_L_H,LD_L_L,LD_L_xHL,LD_L_A,
  LD_xHL_B,LD_xHL_C,LD_xHL_D,LD_xHL_E,LD_xHL_H,LD_xHL_L,HALT,LD_xHL_A,
  LD_A_B,LD_A_C,LD_A_D,LD_A_E,LD_A_H,LD_A_L,LD_A_xHL,LD_A_A,
  ADD_B,ADD_C,ADD_D,ADD_E,ADD_H,ADD_L,ADD_xHL,ADD_A,
  ADC_B,ADC_C,ADC_D,ADC_E,ADC_H,ADC_L,ADC_xHL,ADC_A,
  SUB_B,SUB_C,SUB_D,SUB_E,SUB_H,SUB_L,SUB_xHL,SUB_A,
  SBC_B,SBC_C,SBC_D,SBC_E,SBC_H,SBC_L,SBC_xHL,SBC_A,
  AND_B,AND_C,AND_D,AND_E,AND_H,AND_L,AND_xHL,AND_A,
  XOR_B,XOR_C,XOR_D,XOR_E,XOR_H,XOR_L,XOR_xHL,XOR_A,
  OR_B,OR_C,OR_D,OR_E,OR_H,OR_L,OR_xHL,OR_A,
  CP_B,CP_C,CP_D,CP_E,CP_H,CP_L,CP_xHL,CP_A,
  RET_NZ,POP_BC,JP_NZ,JP,CALL_NZ,PUSH_BC,ADD_BYTE,RST00,
  RET_Z,RET,JP_Z,PFX_CB,CALL_Z,CALL,ADC_BYTE,RST08,
  RET_NC,POP_DE,JP_NC,OUTA,CALL_NC,PUSH_DE,SUB_BYTE,RST10,
  RET_C,EXX,JP_C,INA,CALL_C,PFX_DD,SBC_BYTE,RST18,
  RET_PO,POP_HL,JP_PO,EX_HL_xSP,CALL_PO,PUSH_HL,AND_BYTE,RST20,
  RET_PE,LD_PC_HL,JP_PE,EX_DE_HL,CALL_PE,PFX_ED,XOR_BYTE,RST28,
  RET_P,POP_AF,JP_P,DI,CALL_P,PUSH_AF,OR_BYTE,RST30,
  RET_M,LD_SP_HL,JP_M,EI,CALL_M,PFX_FD,CP_BYTE,RST38;

    static const void* const a_jump_table[256] = { &&
  NOP,&&LD_BC_WORD,&&LD_xBC_A,&&INC_BC,&&INC_B,&&DEC_B,&&LD_B_BYTE,&&RLCA,&&
  EX_AF_AF,&&ADD_HL_BC,&&LD_A_xBC,&&DEC_BC,&&INC_C,&&DEC_C,&&LD_C_BYTE,&&RRCA,&&
  DJNZ,&&LD_DE_WORD,&&LD_xDE_A,&&INC_DE,&&INC_D,&&DEC_D,&&LD_D_BYTE,&&RLA,&&
  JR,&&ADD_HL_DE,&&LD_A_xDE,&&DEC_DE,&&INC_E,&&DEC_E,&&LD_E_BYTE,&&RRA,&&
  JR_NZ,&&LD_HL_WORD,&&LD_xWORD_HL,&&INC_HL,&&INC_H,&&DEC_H,&&LD_H_BYTE,&&DAA,&&
  JR_Z,&&ADD_HL_HL,&&LD_HL_xWORD,&&DEC_HL,&&INC_L,&&DEC_L,&&LD_L_BYTE,&&CPL,&&
  JR_NC,&&LD_SP_WORD,&&LD_xWORD_A,&&INC_SP,&&INC_xHL,&&DEC_xHL,&&LD_xHL_BYTE,&&SCF,&&
  JR_C,&&ADD_HL_SP,&&LD_A_xWORD,&&DEC_SP,&&INC_A,&&DEC_A,&&LD_A_BYTE,&&CCF,&&
  LD_B_B,&&LD_B_C,&&LD_B_D,&&LD_B_E,&&LD_B_H,&&LD_B_L,&&LD_B_xHL,&&LD_B_A,&&
  LD_C_B,&&LD_C_C,&&LD_C_D,&&LD_C_E,&&LD_C_H,&&LD_C_L,&&LD_C_xHL,&&LD_C_A,&&
  LD_D_B,&&LD_D_C,&&LD_D_D,&&LD_D_E,&&LD_D_H,&&LD_D_L,&&LD_D_xHL,&&LD_D_A,&&
  LD_E_B,&&LD_E_C,&&LD_E_D,&&LD_E_E,&&LD_E_H,&&LD_E_L,&&LD_E_xHL,&&LD_E_A,&&
  LD_H_B,&&LD_H_C,&&LD_H_D,&&LD_H_E,&&LD_H_H,&&LD_H_L,&&LD_H_xHL,&&LD_H_A,&&
  LD_L_B,&&LD_L_C,&&LD_L_D,&&LD_L_E,&&LD_L_H,&&LD_L_L,&&LD_L_xHL,&&LD_L_A,&&
  LD_xHL_B,&&LD_xHL_C,&&LD_xHL_D,&&LD_xHL_E,&&LD_xHL_H,&&LD_xHL_L,&&HALT,&&LD_xHL_A,&&
  LD_A_B,&&LD_A_C,&&LD_A_D,&&LD_A_E,&&LD_A_H,&&LD_A_L,&&LD_A_xHL,&&LD_A_A,&&
  ADD_B,&&ADD_C,&&ADD_D,&&ADD_E,&&ADD_H,&&ADD_L,&&ADD_xHL,&&ADD_A,&&
  ADC_B,&&ADC_C,&&ADC_D,&&ADC_E,&&ADC_H,&&ADC_L,&&ADC_xHL,&&ADC_A,&&
  SUB_B,&&SUB_C,&&SUB_D,&&SUB_E,&&SUB_H,&&SUB_L,&&SUB_xHL,&&SUB_A,&&
  SBC_B,&&SBC_C,&&SBC_D,&&SBC_E,&&SBC_H,&&SBC_L,&&SBC_xHL,&&SBC_A,&&
  AND_B,&&AND_C,&&AND_D,&&AND_E,&&AND_H,&&AND_L,&&AND_xHL,&&AND_A,&&
  XOR_B,&&XOR_C,&&XOR_D,&&XOR_E,&&XOR_H,&&XOR_L,&&XOR_xHL,&&XOR_A,&&
  OR_B,&&OR_C,&&OR_D,&&OR_E,&&OR_H,&&OR_L,&&OR_xHL,&&OR_A,&&
  CP_B,&&CP_C,&&CP_D,&&CP_E,&&CP_H,&&CP_L,&&CP_xHL,&&CP_A,&&
  RET_NZ,&&POP_BC,&&JP_NZ,&&JP,&&CALL_NZ,&&PUSH_BC,&&ADD_BYTE,&&RST00,&&
  RET_Z,&&RET,&&JP_Z,&&PFX_CB,&&CALL_Z,&&CALL,&&ADC_BYTE,&&RST08,&&
  RET_NC,&&POP_DE,&&JP_NC,&&OUTA,&&CALL_NC,&&PUSH_DE,&&SUB_BYTE,&&RST10,&&
  RET_C,&&EXX,&&JP_C,&&INA,&&CALL_C,&&PFX_DD,&&SBC_BYTE,&&RST18,&&
  RET_PO,&&POP_HL,&&JP_PO,&&EX_HL_xSP,&&CALL_PO,&&PUSH_HL,&&AND_BYTE,&&RST20,&&
  RET_PE,&&LD_PC_HL,&&JP_PE,&&EX_DE_HL,&&CALL_PE,&&PFX_ED,&&XOR_BYTE,&&RST28,&&
  RET_P,&&POP_AF,&&JP_P,&&DI,&&CALL_P,&&PUSH_AF,&&OR_BYTE,&&RST30,&&
  RET_M,&&LD_SP_HL,&&JP_M,&&EI,&&CALL_M,&&PFX_FD,&&CP_BYTE,&&RST38 };

  byte I;
  pair J;

#define XX IY
  I=RdZ80(CPU.PC.W++);
  CPU.ICount-=CyclesXX[I];
	goto *a_jump_table[I];
  //switch(I) {
JR_NZ:    if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W++; else { M_JR; } return;
JR_NC:    if(CPU.AF.B.l&C_FLAG)  CPU.PC.W++; else { M_JR; } return;
JR_Z:     if(CPU.AF.B.l&Z_FLAG)  { M_JR; } else CPU.PC.W++; return;
JR_C:     if(CPU.AF.B.l&C_FLAG)  { M_JR; } else CPU.PC.W++; return;

JP_NZ:    if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_NC:    if(CPU.AF.B.l&C_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_PO:    if(CPU.AF.B.l&P_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_P:     if(CPU.AF.B.l&S_FLAG)  CPU.PC.W+=2; else { M_JP; }  return;
JP_Z:     if(CPU.AF.B.l&Z_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_C:     if(CPU.AF.B.l&C_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_PE:    if(CPU.AF.B.l&P_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;
JP_M:     if(CPU.AF.B.l&S_FLAG)  { M_JP; }  else CPU.PC.W+=2; return;

RET_NZ:   if(!(CPU.AF.B.l&Z_FLAG))  { M_RET; }  return;
RET_NC:   if(!(CPU.AF.B.l&C_FLAG))  { M_RET; }  return;
RET_PO:   if(!(CPU.AF.B.l&P_FLAG))  { M_RET; }  return;
RET_P:    if(!(CPU.AF.B.l&S_FLAG))  { M_RET; }  return;
RET_Z:    if(CPU.AF.B.l&Z_FLAG)     { M_RET; }  return;
RET_C:    if(CPU.AF.B.l&C_FLAG)     { M_RET; }  return;
RET_PE:   if(CPU.AF.B.l&P_FLAG)     { M_RET; }  return;
RET_M:    if(CPU.AF.B.l&S_FLAG)     { M_RET; }  return;

CALL_NZ:  if(CPU.AF.B.l&Z_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_NC:  if(CPU.AF.B.l&C_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_PO:  if(CPU.AF.B.l&P_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_P:   if(CPU.AF.B.l&S_FLAG)  CPU.PC.W+=2;  else { M_CALL; } return;
CALL_Z:   if(CPU.AF.B.l&Z_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_C:   if(CPU.AF.B.l&C_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_PE:  if(CPU.AF.B.l&P_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;
CALL_M:   if(CPU.AF.B.l&S_FLAG)  { M_CALL; } else CPU.PC.W+=2;  return;

ADD_B:    M_ADD(CPU.BC.B.h);return;
ADD_C:    M_ADD(CPU.BC.B.l);return;
ADD_D:    M_ADD(CPU.DE.B.h);return;
ADD_E:    M_ADD(CPU.DE.B.l);return;
ADD_H:    M_ADD(CPU.XX.B.h);return;
ADD_L:    M_ADD(CPU.XX.B.l);return;
ADD_A:    M_ADD(CPU.AF.B.h);return;
ADD_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_ADD(I);return;
ADD_BYTE: I=RdZ80(CPU.PC.W++);M_ADD(I);return;

SUB_B:    M_SUB(CPU.BC.B.h);return;
SUB_C:    M_SUB(CPU.BC.B.l);return;
SUB_D:    M_SUB(CPU.DE.B.h);return;
SUB_E:    M_SUB(CPU.DE.B.l);return;
SUB_H:    M_SUB(CPU.XX.B.h);return;
SUB_L:    M_SUB(CPU.XX.B.l);return;
SUB_A:    CPU.AF.B.h=0;CPU.AF.B.l=N_FLAG|Z_FLAG;return;
SUB_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_SUB(I);return;
SUB_BYTE: I=RdZ80(CPU.PC.W++);M_SUB(I);return;

AND_B:    M_AND(CPU.BC.B.h);return;
AND_C:    M_AND(CPU.BC.B.l);return;
AND_D:    M_AND(CPU.DE.B.h);return;
AND_E:    M_AND(CPU.DE.B.l);return;
AND_H:    M_AND(CPU.XX.B.h);return;
AND_L:    M_AND(CPU.XX.B.l);return;
AND_A:    M_AND(CPU.AF.B.h);return;
AND_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_AND(I);return;
AND_BYTE: I=RdZ80(CPU.PC.W++);M_AND(I);return;

OR_B:     M_OR(CPU.BC.B.h);return;
OR_C:     M_OR(CPU.BC.B.l);return;
OR_D:     M_OR(CPU.DE.B.h);return;
OR_E:     M_OR(CPU.DE.B.l);return;
OR_H:     M_OR(CPU.XX.B.h);return;
OR_L:     M_OR(CPU.XX.B.l);return;
OR_A:     M_OR(CPU.AF.B.h);return;
OR_xHL:   I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_OR(I);return;
OR_BYTE:  I=RdZ80(CPU.PC.W++);M_OR(I);return;

ADC_B:    M_ADC(CPU.BC.B.h);return;
ADC_C:    M_ADC(CPU.BC.B.l);return;
ADC_D:    M_ADC(CPU.DE.B.h);return;
ADC_E:    M_ADC(CPU.DE.B.l);return;
ADC_H:    M_ADC(CPU.XX.B.h);return;
ADC_L:    M_ADC(CPU.XX.B.l);return;
ADC_A:    M_ADC(CPU.AF.B.h);return;
ADC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_ADC(I);return;
ADC_BYTE: I=RdZ80(CPU.PC.W++);M_ADC(I);return;

SBC_B:    M_SBC(CPU.BC.B.h);return;
SBC_C:    M_SBC(CPU.BC.B.l);return;
SBC_D:    M_SBC(CPU.DE.B.h);return;
SBC_E:    M_SBC(CPU.DE.B.l);return;
SBC_H:    M_SBC(CPU.XX.B.h);return;
SBC_L:    M_SBC(CPU.XX.B.l);return;
SBC_A:    M_SBC(CPU.AF.B.h);return;
SBC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_SBC(I);return;
SBC_BYTE: I=RdZ80(CPU.PC.W++);M_SBC(I);return;

XOR_B:    M_XOR(CPU.BC.B.h);return;
XOR_C:    M_XOR(CPU.BC.B.l);return;
XOR_D:    M_XOR(CPU.DE.B.h);return;
XOR_E:    M_XOR(CPU.DE.B.l);return;
XOR_H:    M_XOR(CPU.XX.B.h);return;
XOR_L:    M_XOR(CPU.XX.B.l);return;
XOR_A:    CPU.AF.B.h=0;CPU.AF.B.l=P_FLAG|Z_FLAG;return;
XOR_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_XOR(I);return;
XOR_BYTE: I=RdZ80(CPU.PC.W++);M_XOR(I);return;

CP_B:     M_CP(CPU.BC.B.h);return;
CP_C:     M_CP(CPU.BC.B.l);return;
CP_D:     M_CP(CPU.DE.B.h);return;
CP_E:     M_CP(CPU.DE.B.l);return;
CP_H:     M_CP(CPU.XX.B.h);return;
CP_L:     M_CP(CPU.XX.B.l);return;
CP_A:     CPU.AF.B.l=N_FLAG|Z_FLAG;return;
CP_xHL:   I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));
               M_CP(I);return;
CP_BYTE:  I=RdZ80(CPU.PC.W++);M_CP(I);return;
               
LD_BC_WORD: M_LDWORD(BC);return;
LD_DE_WORD: M_LDWORD(DE);return;
LD_HL_WORD: M_LDWORD(XX);return;
LD_SP_WORD: M_LDWORD(SP);return;

LD_PC_HL: CPU.PC.W=CPU.XX.W;return;
LD_SP_HL: CPU.SP.W=CPU.XX.W;return;
LD_A_xBC: CPU.AF.B.h=RdZ80(CPU.BC.W);return;
LD_A_xDE: CPU.AF.B.h=RdZ80(CPU.DE.W);return;

ADD_HL_BC:  M_ADDW(XX,BC);return;
ADD_HL_DE:  M_ADDW(XX,DE);return;
ADD_HL_HL:  M_ADDW(XX,XX);return;
ADD_HL_SP:  M_ADDW(XX,SP);return;

DEC_BC:   CPU.BC.W--;return;
DEC_DE:   CPU.DE.W--;return;
DEC_HL:   CPU.XX.W--;return;
DEC_SP:   CPU.SP.W--;return;

INC_BC:   CPU.BC.W++;return;
INC_DE:   CPU.DE.W++;return;
INC_HL:   CPU.XX.W++;return;
INC_SP:   CPU.SP.W++;return;

DEC_B:    M_DEC(CPU.BC.B.h);return;
DEC_C:    M_DEC(CPU.BC.B.l);return;
DEC_D:    M_DEC(CPU.DE.B.h);return;
DEC_E:    M_DEC(CPU.DE.B.l);return;
DEC_H:    M_DEC(CPU.XX.B.h);return;
DEC_L:    M_DEC(CPU.XX.B.l);return;
DEC_A:    M_DEC(CPU.AF.B.h);return;
DEC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_DEC(I);
               WrZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++),I);
               return;

INC_B:    M_INC(CPU.BC.B.h);return;
INC_C:    M_INC(CPU.BC.B.l);return;
INC_D:    M_INC(CPU.DE.B.h);return;
INC_E:    M_INC(CPU.DE.B.l);return;
INC_H:    M_INC(CPU.XX.B.h);return;
INC_L:    M_INC(CPU.XX.B.l);return;
INC_A:    M_INC(CPU.AF.B.h);return;
INC_xHL:  I=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W));M_INC(I);
               WrZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++),I);
               return;

RLCA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|I;
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RLA:
  I=(CPU.AF.B.h&0x80? C_FLAG:0);
  CPU.AF.B.h=(CPU.AF.B.h<<1)|(CPU.AF.B.l&C_FLAG);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RRCA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(I? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;
RRA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(CPU.AF.B.l&C_FLAG? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  return;

RST00:    M_RST(0x0000);return;
RST08:    M_RST(0x0008);return;
RST10:    M_RST(0x0010);return;
RST18:    M_RST(0x0018);return;
RST20:    M_RST(0x0020);return;
RST28:    M_RST(0x0028);return;
RST30:    M_RST(0x0030);return;
RST38:    M_RST(0x0038);return;

PUSH_BC:  M_PUSH(BC);return;
PUSH_DE:  M_PUSH(DE);return;
PUSH_HL:  M_PUSH(XX);return;
PUSH_AF:  M_PUSH(AF);return;

POP_BC:   M_POP(BC);return;
POP_DE:   M_POP(DE);return;
POP_HL:   M_POP(XX);return;
POP_AF:   M_POP(AF);return;

DJNZ: if(--CPU.BC.B.h) { M_JR; } else CPU.PC.W++;return;
JP:   M_JP;return;
JR:   M_JR;return;
CALL: M_CALL;return;
RET:  M_RET;return;
SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);return;
CPL:  CPU.AF.B.h=~CPU.AF.B.h;S(N_FLAG|H_FLAG);return;
NOP:  return;
OUTA: OutZ80(RdZ80(CPU.PC.W++),CPU.AF.B.h);return;
INA:  CPU.AF.B.h=InZ80(RdZ80(CPU.PC.W++));return;

DI:   
  CPU.IFF&=0xFE;
  return;  
EI:
  CPU.IFF|=0x01;
  if(CPU.IRequest!=INT_NONE)
  {
    CPU.IFF|=0x20;
    CPU.IBackup=CPU.ICount;
    CPU.ICount=1;
  }
  return;

CCF:
  CPU.AF.B.l^=C_FLAG;R(N_FLAG|H_FLAG);
  CPU.AF.B.l|=CPU.AF.B.l&C_FLAG? 0:H_FLAG;
  return;

EXX:
  J.W=CPU.BC.W;CPU.BC.W=CPU.BC1.W;CPU.BC1.W=J.W;
  J.W=CPU.DE.W;CPU.DE.W=CPU.DE1.W;CPU.DE1.W=J.W;
  J.W=CPU.HL.W;CPU.HL.W=CPU.HL1.W;CPU.HL1.W=J.W;
  return;

EX_DE_HL: J.W=CPU.DE.W;CPU.DE.W=CPU.HL.W;CPU.HL.W=J.W;return;
EX_AF_AF: J.W=CPU.AF.W;CPU.AF.W=CPU.AF1.W;CPU.AF1.W=J.W;return;  
  
LD_B_B:   CPU.BC.B.h=CPU.BC.B.h;return;
LD_C_B:   CPU.BC.B.l=CPU.BC.B.h;return;
LD_D_B:   CPU.DE.B.h=CPU.BC.B.h;return;
LD_E_B:   CPU.DE.B.l=CPU.BC.B.h;return;
LD_H_B:   CPU.XX.B.h=CPU.BC.B.h;return;
LD_L_B:   CPU.XX.B.l=CPU.BC.B.h;return;
LD_A_B:   CPU.AF.B.h=CPU.BC.B.h;return;
LD_xHL_B: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.h);return;

LD_B_C:   CPU.BC.B.h=CPU.BC.B.l;return;
LD_C_C:   CPU.BC.B.l=CPU.BC.B.l;return;
LD_D_C:   CPU.DE.B.h=CPU.BC.B.l;return;
LD_E_C:   CPU.DE.B.l=CPU.BC.B.l;return;
LD_H_C:   CPU.XX.B.h=CPU.BC.B.l;return;
LD_L_C:   CPU.XX.B.l=CPU.BC.B.l;return;
LD_A_C:   CPU.AF.B.h=CPU.BC.B.l;return;
LD_xHL_C: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.BC.B.l);return;

LD_B_D:   CPU.BC.B.h=CPU.DE.B.h;return;
LD_C_D:   CPU.BC.B.l=CPU.DE.B.h;return;
LD_D_D:   CPU.DE.B.h=CPU.DE.B.h;return;
LD_E_D:   CPU.DE.B.l=CPU.DE.B.h;return;
LD_H_D:   CPU.XX.B.h=CPU.DE.B.h;return;
LD_L_D:   CPU.XX.B.l=CPU.DE.B.h;return;
LD_A_D:   CPU.AF.B.h=CPU.DE.B.h;return;
LD_xHL_D: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.h);return;

LD_B_E:   CPU.BC.B.h=CPU.DE.B.l;return;
LD_C_E:   CPU.BC.B.l=CPU.DE.B.l;return;
LD_D_E:   CPU.DE.B.h=CPU.DE.B.l;return;
LD_E_E:   CPU.DE.B.l=CPU.DE.B.l;return;
LD_H_E:   CPU.XX.B.h=CPU.DE.B.l;return;
LD_L_E:   CPU.XX.B.l=CPU.DE.B.l;return;
LD_A_E:   CPU.AF.B.h=CPU.DE.B.l;return;
LD_xHL_E: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.DE.B.l);return;

LD_B_H:   CPU.BC.B.h=CPU.XX.B.h;return;
LD_C_H:   CPU.BC.B.l=CPU.XX.B.h;return;
LD_D_H:   CPU.DE.B.h=CPU.XX.B.h;return;
LD_E_H:   CPU.DE.B.l=CPU.XX.B.h;return;
LD_H_H:   CPU.XX.B.h=CPU.XX.B.h;return;
LD_L_H:   CPU.XX.B.l=CPU.XX.B.h;return;
LD_A_H:   CPU.AF.B.h=CPU.XX.B.h;return;
LD_xHL_H: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.h);return;

LD_B_L:   CPU.BC.B.h=CPU.XX.B.l;return;
LD_C_L:   CPU.BC.B.l=CPU.XX.B.l;return;
LD_D_L:   CPU.DE.B.h=CPU.XX.B.l;return;
LD_E_L:   CPU.DE.B.l=CPU.XX.B.l;return;
LD_H_L:   CPU.XX.B.h=CPU.XX.B.l;return;
LD_L_L:   CPU.XX.B.l=CPU.XX.B.l;return;
LD_A_L:   CPU.AF.B.h=CPU.XX.B.l;return;
LD_xHL_L: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.HL.B.l);return;

LD_B_A:   CPU.BC.B.h=CPU.AF.B.h;return;
LD_C_A:   CPU.BC.B.l=CPU.AF.B.h;return;
LD_D_A:   CPU.DE.B.h=CPU.AF.B.h;return;
LD_E_A:   CPU.DE.B.l=CPU.AF.B.h;return;
LD_H_A:   CPU.XX.B.h=CPU.AF.B.h;return;
LD_L_A:   CPU.XX.B.l=CPU.AF.B.h;return;
LD_A_A:   CPU.AF.B.h=CPU.AF.B.h;return;
LD_xHL_A: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
               WrZ80(J.W,CPU.AF.B.h);return;

LD_xBC_A: WrZ80(CPU.BC.W,CPU.AF.B.h);return;
LD_xDE_A: WrZ80(CPU.DE.W,CPU.AF.B.h);return;

LD_B_xHL:    CPU.BC.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_C_xHL:    CPU.BC.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_D_xHL:    CPU.DE.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_E_xHL:    CPU.DE.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_H_xHL:    CPU.HL.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_L_xHL:    CPU.HL.B.l=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;
LD_A_xHL:    CPU.AF.B.h=RdZ80(CPU.XX.W+(offset)RdZ80(CPU.PC.W++));return;

LD_B_BYTE:   CPU.BC.B.h=RdZ80(CPU.PC.W++);return;
LD_C_BYTE:   CPU.BC.B.l=RdZ80(CPU.PC.W++);return;
LD_D_BYTE:   CPU.DE.B.h=RdZ80(CPU.PC.W++);return;
LD_E_BYTE:   CPU.DE.B.l=RdZ80(CPU.PC.W++);return;
LD_H_BYTE:   CPU.XX.B.h=RdZ80(CPU.PC.W++);return;
LD_L_BYTE:   CPU.XX.B.l=RdZ80(CPU.PC.W++);return;
LD_A_BYTE:   CPU.AF.B.h=RdZ80(CPU.PC.W++);return;
LD_xHL_BYTE: J.W=CPU.XX.W+(offset)RdZ80(CPU.PC.W++);
                  WrZ80(J.W,RdZ80(CPU.PC.W++));return;

LD_xWORD_HL:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.XX.B.l);
  WrZ80(J.W,CPU.XX.B.h);
  return;

LD_HL_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.XX.B.l=RdZ80(J.W++);
  CPU.XX.B.h=RdZ80(J.W);
  return;

LD_A_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.AF.B.h=RdZ80(J.W);
  return;

LD_xWORD_A:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W,CPU.AF.B.h);
  return;

EX_HL_xSP:
  J.B.l=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W++,CPU.XX.B.l);
  J.B.h=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W--,CPU.XX.B.h);
  CPU.XX.W=J.W;
  return;

DAA:
  J.W=CPU.AF.B.h;
  if(CPU.AF.B.l&C_FLAG) J.W|=256;
  if(CPU.AF.B.l&H_FLAG) J.W|=512;
  if(CPU.AF.B.l&N_FLAG) J.W|=1024;
  CPU.AF.W=DAATable[J.W];
  return;
    PFX_FD:
    PFX_DD:
      CPU.PC.W--;return;
    PFX_CB:
      CodesFDCB();return;
    HALT:
      CPU.PC.W--;CPU.IFF|=0x80;CPU.ICount=0;return;
PFX_ED:
   return;
  //}
#undef XX
}

/** ResetZ80() ***********************************************/
/** This function can be used to reset the register struct  **/
/** before starting execution with Z80(). It sets the       **/
/** registers to their supposed initial values.             **/
/*************************************************************/
void ResetZ80()
{
  CPU.PC.W=0x0000;CPU.SP.W=0xF000;
  CPU.AF.W=CPU.BC.W=CPU.DE.W=CPU.HL.W=0x0000;
  CPU.AF1.W=CPU.BC1.W=CPU.DE1.W=CPU.HL1.W=0x0000;
  CPU.IX.W=CPU.IY.W=0x0000;
  CPU.I=0x00;CPU.IFF=0x00;
  CPU.ICount=CPU.IPeriod;
  CPU.IRequest=INT_NONE;
}

/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
void IntZ80(word Vector)
{
  if((CPU.IFF&0x01)||(Vector==INT_NMI))
  {
    /* Experimental V Shouldn't disable all interrupts? */
    CPU.IFF=(CPU.IFF&0x9E)|((CPU.IFF&0x01)<<6);
    if(CPU.IFF&0x80) { CPU.PC.W++;CPU.IFF&=0x7F; }
    M_PUSH(PC);

    /* Automatically reset IRequest if needed */
    if(CPU.IAutoReset&&(Vector==CPU.IRequest)) CPU.IRequest=INT_NONE;

    if(Vector==INT_NMI) CPU.PC.W=INT_NMI;
    else
      if(CPU.IFF&0x04)
      { 
        Vector=(Vector&0xFF)|((word)(CPU.I)<<8);
        CPU.PC.B.l=RdZ80(Vector++);
        CPU.PC.B.h=RdZ80(Vector);
      }
      else
        if(CPU.IFF&0x02) CPU.PC.W=INT_IRQ;
        else CPU.PC.W=Vector;
  }
}

/** RunZ80() *************************************************/
/** This function will run Z80 code until an LoopZ80() call **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in CPU.    **/
/*************************************************************/

word 
RunZ80()
{
  __label__
  NOP,LD_BC_WORD,LD_xBC_A,INC_BC,INC_B,DEC_B,LD_B_BYTE,RLCA,
  EX_AF_AF,ADD_HL_BC,LD_A_xBC,DEC_BC,INC_C,DEC_C,LD_C_BYTE,RRCA,
  DJNZ,LD_DE_WORD,LD_xDE_A,INC_DE,INC_D,DEC_D,LD_D_BYTE,RLA,
  JR,ADD_HL_DE,LD_A_xDE,DEC_DE,INC_E,DEC_E,LD_E_BYTE,RRA,
  JR_NZ,LD_HL_WORD,LD_xWORD_HL,INC_HL,INC_H,DEC_H,LD_H_BYTE,DAA,
  JR_Z,ADD_HL_HL,LD_HL_xWORD,DEC_HL,INC_L,DEC_L,LD_L_BYTE,CPL,
  JR_NC,LD_SP_WORD,LD_xWORD_A,INC_SP,INC_xHL,DEC_xHL,LD_xHL_BYTE,SCF,
  JR_C,ADD_HL_SP,LD_A_xWORD,DEC_SP,INC_A,DEC_A,LD_A_BYTE,CCF,
  LD_B_B,LD_B_C,LD_B_D,LD_B_E,LD_B_H,LD_B_L,LD_B_xHL,LD_B_A,
  LD_C_B,LD_C_C,LD_C_D,LD_C_E,LD_C_H,LD_C_L,LD_C_xHL,LD_C_A,
  LD_D_B,LD_D_C,LD_D_D,LD_D_E,LD_D_H,LD_D_L,LD_D_xHL,LD_D_A,
  LD_E_B,LD_E_C,LD_E_D,LD_E_E,LD_E_H,LD_E_L,LD_E_xHL,LD_E_A,
  LD_H_B,LD_H_C,LD_H_D,LD_H_E,LD_H_H,LD_H_L,LD_H_xHL,LD_H_A,
  LD_L_B,LD_L_C,LD_L_D,LD_L_E,LD_L_H,LD_L_L,LD_L_xHL,LD_L_A,
  LD_xHL_B,LD_xHL_C,LD_xHL_D,LD_xHL_E,LD_xHL_H,LD_xHL_L,HALT,LD_xHL_A,
  LD_A_B,LD_A_C,LD_A_D,LD_A_E,LD_A_H,LD_A_L,LD_A_xHL,LD_A_A,
  ADD_B,ADD_C,ADD_D,ADD_E,ADD_H,ADD_L,ADD_xHL,ADD_A,
  ADC_B,ADC_C,ADC_D,ADC_E,ADC_H,ADC_L,ADC_xHL,ADC_A,
  SUB_B,SUB_C,SUB_D,SUB_E,SUB_H,SUB_L,SUB_xHL,SUB_A,
  SBC_B,SBC_C,SBC_D,SBC_E,SBC_H,SBC_L,SBC_xHL,SBC_A,
  AND_B,AND_C,AND_D,AND_E,AND_H,AND_L,AND_xHL,AND_A,
  XOR_B,XOR_C,XOR_D,XOR_E,XOR_H,XOR_L,XOR_xHL,XOR_A,
  OR_B,OR_C,OR_D,OR_E,OR_H,OR_L,OR_xHL,OR_A,
  CP_B,CP_C,CP_D,CP_E,CP_H,CP_L,CP_xHL,CP_A,
  RET_NZ,POP_BC,JP_NZ,JP,CALL_NZ,PUSH_BC,ADD_BYTE,RST00,
  RET_Z,RET,JP_Z,PFX_CB,CALL_Z,CALL,ADC_BYTE,RST08,
  RET_NC,POP_DE,JP_NC,OUTA,CALL_NC,PUSH_DE,SUB_BYTE,RST10,
  RET_C,EXX,JP_C,INA,CALL_C,PFX_DD,SBC_BYTE,RST18,
  RET_PO,POP_HL,JP_PO,EX_HL_xSP,CALL_PO,PUSH_HL,AND_BYTE,RST20,
  RET_PE,LD_PC_HL,JP_PE,EX_DE_HL,CALL_PE,PFX_ED,XOR_BYTE,RST28,
  RET_P,POP_AF,JP_P,DI,CALL_P,PUSH_AF,OR_BYTE,RST30,
  RET_M,LD_SP_HL,JP_M,EI,CALL_M,PFX_FD,CP_BYTE,RST38;

    static const void* const a_jump_table[256] = { &&
  NOP,&&LD_BC_WORD,&&LD_xBC_A,&&INC_BC,&&INC_B,&&DEC_B,&&LD_B_BYTE,&&RLCA,&&
  EX_AF_AF,&&ADD_HL_BC,&&LD_A_xBC,&&DEC_BC,&&INC_C,&&DEC_C,&&LD_C_BYTE,&&RRCA,&&
  DJNZ,&&LD_DE_WORD,&&LD_xDE_A,&&INC_DE,&&INC_D,&&DEC_D,&&LD_D_BYTE,&&RLA,&&
  JR,&&ADD_HL_DE,&&LD_A_xDE,&&DEC_DE,&&INC_E,&&DEC_E,&&LD_E_BYTE,&&RRA,&&
  JR_NZ,&&LD_HL_WORD,&&LD_xWORD_HL,&&INC_HL,&&INC_H,&&DEC_H,&&LD_H_BYTE,&&DAA,&&
  JR_Z,&&ADD_HL_HL,&&LD_HL_xWORD,&&DEC_HL,&&INC_L,&&DEC_L,&&LD_L_BYTE,&&CPL,&&
  JR_NC,&&LD_SP_WORD,&&LD_xWORD_A,&&INC_SP,&&INC_xHL,&&DEC_xHL,&&LD_xHL_BYTE,&&SCF,&&
  JR_C,&&ADD_HL_SP,&&LD_A_xWORD,&&DEC_SP,&&INC_A,&&DEC_A,&&LD_A_BYTE,&&CCF,&&
  LD_B_B,&&LD_B_C,&&LD_B_D,&&LD_B_E,&&LD_B_H,&&LD_B_L,&&LD_B_xHL,&&LD_B_A,&&
  LD_C_B,&&LD_C_C,&&LD_C_D,&&LD_C_E,&&LD_C_H,&&LD_C_L,&&LD_C_xHL,&&LD_C_A,&&
  LD_D_B,&&LD_D_C,&&LD_D_D,&&LD_D_E,&&LD_D_H,&&LD_D_L,&&LD_D_xHL,&&LD_D_A,&&
  LD_E_B,&&LD_E_C,&&LD_E_D,&&LD_E_E,&&LD_E_H,&&LD_E_L,&&LD_E_xHL,&&LD_E_A,&&
  LD_H_B,&&LD_H_C,&&LD_H_D,&&LD_H_E,&&LD_H_H,&&LD_H_L,&&LD_H_xHL,&&LD_H_A,&&
  LD_L_B,&&LD_L_C,&&LD_L_D,&&LD_L_E,&&LD_L_H,&&LD_L_L,&&LD_L_xHL,&&LD_L_A,&&
  LD_xHL_B,&&LD_xHL_C,&&LD_xHL_D,&&LD_xHL_E,&&LD_xHL_H,&&LD_xHL_L,&&HALT,&&LD_xHL_A,&&
  LD_A_B,&&LD_A_C,&&LD_A_D,&&LD_A_E,&&LD_A_H,&&LD_A_L,&&LD_A_xHL,&&LD_A_A,&&
  ADD_B,&&ADD_C,&&ADD_D,&&ADD_E,&&ADD_H,&&ADD_L,&&ADD_xHL,&&ADD_A,&&
  ADC_B,&&ADC_C,&&ADC_D,&&ADC_E,&&ADC_H,&&ADC_L,&&ADC_xHL,&&ADC_A,&&
  SUB_B,&&SUB_C,&&SUB_D,&&SUB_E,&&SUB_H,&&SUB_L,&&SUB_xHL,&&SUB_A,&&
  SBC_B,&&SBC_C,&&SBC_D,&&SBC_E,&&SBC_H,&&SBC_L,&&SBC_xHL,&&SBC_A,&&
  AND_B,&&AND_C,&&AND_D,&&AND_E,&&AND_H,&&AND_L,&&AND_xHL,&&AND_A,&&
  XOR_B,&&XOR_C,&&XOR_D,&&XOR_E,&&XOR_H,&&XOR_L,&&XOR_xHL,&&XOR_A,&&
  OR_B,&&OR_C,&&OR_D,&&OR_E,&&OR_H,&&OR_L,&&OR_xHL,&&OR_A,&&
  CP_B,&&CP_C,&&CP_D,&&CP_E,&&CP_H,&&CP_L,&&CP_xHL,&&CP_A,&&
  RET_NZ,&&POP_BC,&&JP_NZ,&&JP,&&CALL_NZ,&&PUSH_BC,&&ADD_BYTE,&&RST00,&&
  RET_Z,&&RET,&&JP_Z,&&PFX_CB,&&CALL_Z,&&CALL,&&ADC_BYTE,&&RST08,&&
  RET_NC,&&POP_DE,&&JP_NC,&&OUTA,&&CALL_NC,&&PUSH_DE,&&SUB_BYTE,&&RST10,&&
  RET_C,&&EXX,&&JP_C,&&INA,&&CALL_C,&&PFX_DD,&&SBC_BYTE,&&RST18,&&
  RET_PO,&&POP_HL,&&JP_PO,&&EX_HL_xSP,&&CALL_PO,&&PUSH_HL,&&AND_BYTE,&&RST20,&&
  RET_PE,&&LD_PC_HL,&&JP_PE,&&EX_DE_HL,&&CALL_PE,&&PFX_ED,&&XOR_BYTE,&&RST28,&&
  RET_P,&&POP_AF,&&JP_P,&&DI,&&CALL_P,&&PUSH_AF,&&OR_BYTE,&&RST30,&&
  RET_M,&&LD_SP_HL,&&JP_M,&&EI,&&CALL_M,&&PFX_FD,&&CP_BYTE,&&RST38 };

  byte I;
  pair J;

  goto z80_loop_beg;

z80_loop_end:

    /* If cycle counter expired... */
    if(CPU.ICount<=0)
    {
      /* If we have come after EI, get address from IRequest */
      /* Otherwise, get it from the loop handler             */
      if(CPU.IFF&0x20)
      {
        J.W=CPU.IRequest;         /* Get pending interrupt    */
        CPU.ICount+=CPU.IBackup-1; /* Restore the ICount       */
        CPU.IFF&=0xDF;            /* Done with AfterEI state  */
      }
      else
      {
        J.W=LoopZ80(&CPU);          /* Call periodic handler    */
        CPU.ICount=CPU.IPeriod;    /* Reset the cycle counter  */
        if(J.W==INT_NONE) I=CPU.IRequest; /* Pending int-rupt */
      }

      if(J.W==INT_QUIT) return(CPU.PC.W); /* Exit if INT_QUIT */
      if(J.W!=INT_NONE) IntZ80(J.W);   /* Int-pt if needed */
    }

z80_loop_beg:

    I=RdZ80(CPU.PC.W++);
    CPU.ICount-=Cycles[I];
	  goto *a_jump_table[I];
    //switch(I) {
 
JR_NZ:   if(CPU.AF.B.l&Z_FLAG) CPU.PC.W++; else { CPU.ICount-=5;M_JR; } goto z80_loop_end;
JR_NC:   if(CPU.AF.B.l&C_FLAG) CPU.PC.W++; else { CPU.ICount-=5;M_JR; } goto z80_loop_end;
JR_Z:    if(CPU.AF.B.l&Z_FLAG) { CPU.ICount-=5;M_JR; } else CPU.PC.W++; goto z80_loop_end;
JR_C:    if(CPU.AF.B.l&C_FLAG) { CPU.ICount-=5;M_JR; } else CPU.PC.W++; goto z80_loop_end;

JP_NZ:   if(CPU.AF.B.l&Z_FLAG) CPU.PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_NC:   if(CPU.AF.B.l&C_FLAG) CPU.PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_PO:   if(CPU.AF.B.l&P_FLAG) CPU.PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_P:    if(CPU.AF.B.l&S_FLAG) CPU.PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_Z:    if(CPU.AF.B.l&Z_FLAG) { M_JP; } else CPU.PC.W+=2; goto z80_loop_end;
JP_C:    if(CPU.AF.B.l&C_FLAG) { M_JP; } else CPU.PC.W+=2; goto z80_loop_end;
JP_PE:   if(CPU.AF.B.l&P_FLAG) { M_JP; } else CPU.PC.W+=2; goto z80_loop_end;
JP_M:    if(CPU.AF.B.l&S_FLAG) { M_JP; } else CPU.PC.W+=2; goto z80_loop_end;

RET_NZ:  if(!(CPU.AF.B.l&Z_FLAG)) { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_NC:  if(!(CPU.AF.B.l&C_FLAG)) { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_PO:  if(!(CPU.AF.B.l&P_FLAG)) { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_P:   if(!(CPU.AF.B.l&S_FLAG)) { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_Z:   if(CPU.AF.B.l&Z_FLAG)    { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_C:   if(CPU.AF.B.l&C_FLAG)    { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_PE:  if(CPU.AF.B.l&P_FLAG)    { CPU.ICount-=6;M_RET; } goto z80_loop_end;
RET_M:   if(CPU.AF.B.l&S_FLAG)    { CPU.ICount-=6;M_RET; } goto z80_loop_end;

CALL_NZ: if(CPU.AF.B.l&Z_FLAG) CPU.PC.W+=2; else { CPU.ICount-=7;M_CALL; } goto z80_loop_end;
CALL_NC: if(CPU.AF.B.l&C_FLAG) CPU.PC.W+=2; else { CPU.ICount-=7;M_CALL; } goto z80_loop_end;
CALL_PO: if(CPU.AF.B.l&P_FLAG) CPU.PC.W+=2; else { CPU.ICount-=7;M_CALL; } goto z80_loop_end;
CALL_P:  if(CPU.AF.B.l&S_FLAG) CPU.PC.W+=2; else { CPU.ICount-=7;M_CALL; } goto z80_loop_end;
CALL_Z:  if(CPU.AF.B.l&Z_FLAG) { CPU.ICount-=7;M_CALL; } else CPU.PC.W+=2; goto z80_loop_end;
CALL_C:  if(CPU.AF.B.l&C_FLAG) { CPU.ICount-=7;M_CALL; } else CPU.PC.W+=2; goto z80_loop_end;
CALL_PE: if(CPU.AF.B.l&P_FLAG) { CPU.ICount-=7;M_CALL; } else CPU.PC.W+=2; goto z80_loop_end;
CALL_M:  if(CPU.AF.B.l&S_FLAG) { CPU.ICount-=7;M_CALL; } else CPU.PC.W+=2; goto z80_loop_end;

ADD_B:    M_ADD(CPU.BC.B.h);goto z80_loop_end;
ADD_C:    M_ADD(CPU.BC.B.l);goto z80_loop_end;
ADD_D:    M_ADD(CPU.DE.B.h);goto z80_loop_end;
ADD_E:    M_ADD(CPU.DE.B.l);goto z80_loop_end;
ADD_H:    M_ADD(CPU.HL.B.h);goto z80_loop_end;
ADD_L:    M_ADD(CPU.HL.B.l);goto z80_loop_end;
ADD_A:    M_ADD(CPU.AF.B.h);goto z80_loop_end;
ADD_xHL:  I=RdZ80(CPU.HL.W);M_ADD(I);goto z80_loop_end;
ADD_BYTE: I=RdZ80(CPU.PC.W++);M_ADD(I);goto z80_loop_end;

SUB_B:    M_SUB(CPU.BC.B.h);goto z80_loop_end;
SUB_C:    M_SUB(CPU.BC.B.l);goto z80_loop_end;
SUB_D:    M_SUB(CPU.DE.B.h);goto z80_loop_end;
SUB_E:    M_SUB(CPU.DE.B.l);goto z80_loop_end;
SUB_H:    M_SUB(CPU.HL.B.h);goto z80_loop_end;
SUB_L:    M_SUB(CPU.HL.B.l);goto z80_loop_end;
SUB_A:    CPU.AF.B.h=0;CPU.AF.B.l=N_FLAG|Z_FLAG;goto z80_loop_end;
SUB_xHL:  I=RdZ80(CPU.HL.W);M_SUB(I);goto z80_loop_end;
SUB_BYTE: I=RdZ80(CPU.PC.W++);M_SUB(I);goto z80_loop_end;

AND_B:    M_AND(CPU.BC.B.h);goto z80_loop_end;
AND_C:    M_AND(CPU.BC.B.l);goto z80_loop_end;
AND_D:    M_AND(CPU.DE.B.h);goto z80_loop_end;
AND_E:    M_AND(CPU.DE.B.l);goto z80_loop_end;
AND_H:    M_AND(CPU.HL.B.h);goto z80_loop_end;
AND_L:    M_AND(CPU.HL.B.l);goto z80_loop_end;
AND_A:    M_AND(CPU.AF.B.h);goto z80_loop_end;
AND_xHL:  I=RdZ80(CPU.HL.W);M_AND(I);goto z80_loop_end;
AND_BYTE: I=RdZ80(CPU.PC.W++);M_AND(I);goto z80_loop_end;

OR_B:     M_OR(CPU.BC.B.h);goto z80_loop_end;
OR_C:     M_OR(CPU.BC.B.l);goto z80_loop_end;
OR_D:     M_OR(CPU.DE.B.h);goto z80_loop_end;
OR_E:     M_OR(CPU.DE.B.l);goto z80_loop_end;
OR_H:     M_OR(CPU.HL.B.h);goto z80_loop_end;
OR_L:     M_OR(CPU.HL.B.l);goto z80_loop_end;
OR_A:     M_OR(CPU.AF.B.h);goto z80_loop_end;
OR_xHL:   I=RdZ80(CPU.HL.W);M_OR(I);goto z80_loop_end;
OR_BYTE:  I=RdZ80(CPU.PC.W++);M_OR(I);goto z80_loop_end;

ADC_B:    M_ADC(CPU.BC.B.h);goto z80_loop_end;
ADC_C:    M_ADC(CPU.BC.B.l);goto z80_loop_end;
ADC_D:    M_ADC(CPU.DE.B.h);goto z80_loop_end;
ADC_E:    M_ADC(CPU.DE.B.l);goto z80_loop_end;
ADC_H:    M_ADC(CPU.HL.B.h);goto z80_loop_end;
ADC_L:    M_ADC(CPU.HL.B.l);goto z80_loop_end;
ADC_A:    M_ADC(CPU.AF.B.h);goto z80_loop_end;
ADC_xHL:  I=RdZ80(CPU.HL.W);M_ADC(I);goto z80_loop_end;
ADC_BYTE: I=RdZ80(CPU.PC.W++);M_ADC(I);goto z80_loop_end;

SBC_B:    M_SBC(CPU.BC.B.h);goto z80_loop_end;
SBC_C:    M_SBC(CPU.BC.B.l);goto z80_loop_end;
SBC_D:    M_SBC(CPU.DE.B.h);goto z80_loop_end;
SBC_E:    M_SBC(CPU.DE.B.l);goto z80_loop_end;
SBC_H:    M_SBC(CPU.HL.B.h);goto z80_loop_end;
SBC_L:    M_SBC(CPU.HL.B.l);goto z80_loop_end;
SBC_A:    M_SBC(CPU.AF.B.h);goto z80_loop_end;
SBC_xHL:  I=RdZ80(CPU.HL.W);M_SBC(I);goto z80_loop_end;
SBC_BYTE: I=RdZ80(CPU.PC.W++);M_SBC(I);goto z80_loop_end;

XOR_B:    M_XOR(CPU.BC.B.h);goto z80_loop_end;
XOR_C:    M_XOR(CPU.BC.B.l);goto z80_loop_end;
XOR_D:    M_XOR(CPU.DE.B.h);goto z80_loop_end;
XOR_E:    M_XOR(CPU.DE.B.l);goto z80_loop_end;
XOR_H:    M_XOR(CPU.HL.B.h);goto z80_loop_end;
XOR_L:    M_XOR(CPU.HL.B.l);goto z80_loop_end;
XOR_A:    CPU.AF.B.h=0;CPU.AF.B.l=P_FLAG|Z_FLAG;goto z80_loop_end;
XOR_xHL:  I=RdZ80(CPU.HL.W);M_XOR(I);goto z80_loop_end;
XOR_BYTE: I=RdZ80(CPU.PC.W++);M_XOR(I);goto z80_loop_end;

CP_B:     M_CP(CPU.BC.B.h);goto z80_loop_end;
CP_C:     M_CP(CPU.BC.B.l);goto z80_loop_end;
CP_D:     M_CP(CPU.DE.B.h);goto z80_loop_end;
CP_E:     M_CP(CPU.DE.B.l);goto z80_loop_end;
CP_H:     M_CP(CPU.HL.B.h);goto z80_loop_end;
CP_L:     M_CP(CPU.HL.B.l);goto z80_loop_end;
CP_A:     CPU.AF.B.l=N_FLAG|Z_FLAG;goto z80_loop_end;
CP_xHL:   I=RdZ80(CPU.HL.W);M_CP(I);goto z80_loop_end;
CP_BYTE:  I=RdZ80(CPU.PC.W++);M_CP(I);goto z80_loop_end;
               
LD_BC_WORD: M_LDWORD(BC);goto z80_loop_end;
LD_DE_WORD: M_LDWORD(DE);goto z80_loop_end;
LD_HL_WORD: M_LDWORD(HL);goto z80_loop_end;
LD_SP_WORD: M_LDWORD(SP);goto z80_loop_end;

LD_PC_HL: CPU.PC.W=CPU.HL.W;goto z80_loop_end;
LD_SP_HL: CPU.SP.W=CPU.HL.W;goto z80_loop_end;
LD_A_xBC: CPU.AF.B.h=RdZ80(CPU.BC.W);goto z80_loop_end;
LD_A_xDE: CPU.AF.B.h=RdZ80(CPU.DE.W);goto z80_loop_end;

ADD_HL_BC:  M_ADDW(HL,BC);goto z80_loop_end;
ADD_HL_DE:  M_ADDW(HL,DE);goto z80_loop_end;
ADD_HL_HL:  M_ADDW(HL,HL);goto z80_loop_end;
ADD_HL_SP:  M_ADDW(HL,SP);goto z80_loop_end;

DEC_BC:   CPU.BC.W--;goto z80_loop_end;
DEC_DE:   CPU.DE.W--;goto z80_loop_end;
DEC_HL:   CPU.HL.W--;goto z80_loop_end;
DEC_SP:   CPU.SP.W--;goto z80_loop_end;

INC_BC:   CPU.BC.W++;goto z80_loop_end;
INC_DE:   CPU.DE.W++;goto z80_loop_end;
INC_HL:   CPU.HL.W++;goto z80_loop_end;
INC_SP:   CPU.SP.W++;goto z80_loop_end;

DEC_B:    M_DEC(CPU.BC.B.h);goto z80_loop_end;
DEC_C:    M_DEC(CPU.BC.B.l);goto z80_loop_end;
DEC_D:    M_DEC(CPU.DE.B.h);goto z80_loop_end;
DEC_E:    M_DEC(CPU.DE.B.l);goto z80_loop_end;
DEC_H:    M_DEC(CPU.HL.B.h);goto z80_loop_end;
DEC_L:    M_DEC(CPU.HL.B.l);goto z80_loop_end;
DEC_A:    M_DEC(CPU.AF.B.h);goto z80_loop_end;
DEC_xHL:  I=RdZ80(CPU.HL.W);M_DEC(I);WrZ80(CPU.HL.W,I);goto z80_loop_end;

INC_B:    M_INC(CPU.BC.B.h);goto z80_loop_end;
INC_C:    M_INC(CPU.BC.B.l);goto z80_loop_end;
INC_D:    M_INC(CPU.DE.B.h);goto z80_loop_end;
INC_E:    M_INC(CPU.DE.B.l);goto z80_loop_end;
INC_H:    M_INC(CPU.HL.B.h);goto z80_loop_end;
INC_L:    M_INC(CPU.HL.B.l);goto z80_loop_end;
INC_A:    M_INC(CPU.AF.B.h);goto z80_loop_end;
INC_xHL:  I=RdZ80(CPU.HL.W);M_INC(I);WrZ80(CPU.HL.W,I);goto z80_loop_end;

RLCA:
  I=CPU.AF.B.h&0x80? C_FLAG:0;
  CPU.AF.B.h=(CPU.AF.B.h<<1)|I;
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  goto z80_loop_end;
RLA:
  I=CPU.AF.B.h&0x80? C_FLAG:0;
  CPU.AF.B.h=(CPU.AF.B.h<<1)|(CPU.AF.B.l&C_FLAG);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  goto z80_loop_end;
RRCA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(I? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I; 
  goto z80_loop_end;
RRA:
  I=CPU.AF.B.h&0x01;
  CPU.AF.B.h=(CPU.AF.B.h>>1)|(CPU.AF.B.l&C_FLAG? 0x80:0);
  CPU.AF.B.l=(CPU.AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  goto z80_loop_end;

RST00:    M_RST(0x0000);goto z80_loop_end;
RST08:    M_RST(0x0008);goto z80_loop_end;
RST10:    M_RST(0x0010);goto z80_loop_end;
RST18:    M_RST(0x0018);goto z80_loop_end;
RST20:    M_RST(0x0020);goto z80_loop_end;
RST28:    M_RST(0x0028);goto z80_loop_end;
RST30:    M_RST(0x0030);goto z80_loop_end;
RST38:    M_RST(0x0038);goto z80_loop_end;

PUSH_BC:  M_PUSH(BC);goto z80_loop_end;
PUSH_DE:  M_PUSH(DE);goto z80_loop_end;
PUSH_HL:  M_PUSH(HL);goto z80_loop_end;
PUSH_AF:  M_PUSH(AF);goto z80_loop_end;

POP_BC:   M_POP(BC);goto z80_loop_end;
POP_DE:   M_POP(DE);goto z80_loop_end;
POP_HL:   M_POP(HL);goto z80_loop_end;
POP_AF:   M_POP(AF);goto z80_loop_end;

DJNZ: if(--CPU.BC.B.h) { M_JR; } else CPU.PC.W++;goto z80_loop_end;
JP:   M_JP;goto z80_loop_end;
JR:   M_JR;goto z80_loop_end;
CALL: M_CALL;goto z80_loop_end;
RET:  M_RET;goto z80_loop_end;
SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);goto z80_loop_end;
CPL:  CPU.AF.B.h=~CPU.AF.B.h;S(N_FLAG|H_FLAG);goto z80_loop_end;
NOP:  goto z80_loop_end;
OUTA: OutZ80(RdZ80(CPU.PC.W++),CPU.AF.B.h);goto z80_loop_end;
INA:  CPU.AF.B.h=InZ80(RdZ80(CPU.PC.W++));goto z80_loop_end;
HALT: CPU.PC.W--;CPU.IFF|=0x80;CPU.ICount=0;goto z80_loop_end;

DI:
  CPU.IFF&=0xFE;
  goto z80_loop_end;
EI:
  CPU.IFF|=0x01;
  if(CPU.IRequest!=INT_NONE)
  {
    CPU.IFF|=0x20;
    CPU.IBackup=CPU.ICount;
    CPU.ICount=1;
  }
  goto z80_loop_end;

CCF:
  CPU.AF.B.l^=C_FLAG;R(N_FLAG|H_FLAG);
  CPU.AF.B.l|=CPU.AF.B.l&C_FLAG? 0:H_FLAG;
  goto z80_loop_end;

EXX:
  J.W=CPU.BC.W;CPU.BC.W=CPU.BC1.W;CPU.BC1.W=J.W;
  J.W=CPU.DE.W;CPU.DE.W=CPU.DE1.W;CPU.DE1.W=J.W;
  J.W=CPU.HL.W;CPU.HL.W=CPU.HL1.W;CPU.HL1.W=J.W;
  goto z80_loop_end;

EX_DE_HL: J.W=CPU.DE.W;CPU.DE.W=CPU.HL.W;CPU.HL.W=J.W;goto z80_loop_end;
EX_AF_AF: J.W=CPU.AF.W;CPU.AF.W=CPU.AF1.W;CPU.AF1.W=J.W;goto z80_loop_end;  
  
LD_B_B:   CPU.BC.B.h=CPU.BC.B.h;goto z80_loop_end;
LD_C_B:   CPU.BC.B.l=CPU.BC.B.h;goto z80_loop_end;
LD_D_B:   CPU.DE.B.h=CPU.BC.B.h;goto z80_loop_end;
LD_E_B:   CPU.DE.B.l=CPU.BC.B.h;goto z80_loop_end;
LD_H_B:   CPU.HL.B.h=CPU.BC.B.h;goto z80_loop_end;
LD_L_B:   CPU.HL.B.l=CPU.BC.B.h;goto z80_loop_end;
LD_A_B:   CPU.AF.B.h=CPU.BC.B.h;goto z80_loop_end;
LD_xHL_B: WrZ80(CPU.HL.W,CPU.BC.B.h);goto z80_loop_end;

LD_B_C:   CPU.BC.B.h=CPU.BC.B.l;goto z80_loop_end;
LD_C_C:   CPU.BC.B.l=CPU.BC.B.l;goto z80_loop_end;
LD_D_C:   CPU.DE.B.h=CPU.BC.B.l;goto z80_loop_end;
LD_E_C:   CPU.DE.B.l=CPU.BC.B.l;goto z80_loop_end;
LD_H_C:   CPU.HL.B.h=CPU.BC.B.l;goto z80_loop_end;
LD_L_C:   CPU.HL.B.l=CPU.BC.B.l;goto z80_loop_end;
LD_A_C:   CPU.AF.B.h=CPU.BC.B.l;goto z80_loop_end;
LD_xHL_C: WrZ80(CPU.HL.W,CPU.BC.B.l);goto z80_loop_end;

LD_B_D:   CPU.BC.B.h=CPU.DE.B.h;goto z80_loop_end;
LD_C_D:   CPU.BC.B.l=CPU.DE.B.h;goto z80_loop_end;
LD_D_D:   CPU.DE.B.h=CPU.DE.B.h;goto z80_loop_end;
LD_E_D:   CPU.DE.B.l=CPU.DE.B.h;goto z80_loop_end;
LD_H_D:   CPU.HL.B.h=CPU.DE.B.h;goto z80_loop_end;
LD_L_D:   CPU.HL.B.l=CPU.DE.B.h;goto z80_loop_end;
LD_A_D:   CPU.AF.B.h=CPU.DE.B.h;goto z80_loop_end;
LD_xHL_D: WrZ80(CPU.HL.W,CPU.DE.B.h);goto z80_loop_end;

LD_B_E:   CPU.BC.B.h=CPU.DE.B.l;goto z80_loop_end;
LD_C_E:   CPU.BC.B.l=CPU.DE.B.l;goto z80_loop_end;
LD_D_E:   CPU.DE.B.h=CPU.DE.B.l;goto z80_loop_end;
LD_E_E:   CPU.DE.B.l=CPU.DE.B.l;goto z80_loop_end;
LD_H_E:   CPU.HL.B.h=CPU.DE.B.l;goto z80_loop_end;
LD_L_E:   CPU.HL.B.l=CPU.DE.B.l;goto z80_loop_end;
LD_A_E:   CPU.AF.B.h=CPU.DE.B.l;goto z80_loop_end;
LD_xHL_E: WrZ80(CPU.HL.W,CPU.DE.B.l);goto z80_loop_end;

LD_B_H:   CPU.BC.B.h=CPU.HL.B.h;goto z80_loop_end;
LD_C_H:   CPU.BC.B.l=CPU.HL.B.h;goto z80_loop_end;
LD_D_H:   CPU.DE.B.h=CPU.HL.B.h;goto z80_loop_end;
LD_E_H:   CPU.DE.B.l=CPU.HL.B.h;goto z80_loop_end;
LD_H_H:   CPU.HL.B.h=CPU.HL.B.h;goto z80_loop_end;
LD_L_H:   CPU.HL.B.l=CPU.HL.B.h;goto z80_loop_end;
LD_A_H:   CPU.AF.B.h=CPU.HL.B.h;goto z80_loop_end;
LD_xHL_H: WrZ80(CPU.HL.W,CPU.HL.B.h);goto z80_loop_end;

LD_B_L:   CPU.BC.B.h=CPU.HL.B.l;goto z80_loop_end;
LD_C_L:   CPU.BC.B.l=CPU.HL.B.l;goto z80_loop_end;
LD_D_L:   CPU.DE.B.h=CPU.HL.B.l;goto z80_loop_end;
LD_E_L:   CPU.DE.B.l=CPU.HL.B.l;goto z80_loop_end;
LD_H_L:   CPU.HL.B.h=CPU.HL.B.l;goto z80_loop_end;
LD_L_L:   CPU.HL.B.l=CPU.HL.B.l;goto z80_loop_end;
LD_A_L:   CPU.AF.B.h=CPU.HL.B.l;goto z80_loop_end;
LD_xHL_L: WrZ80(CPU.HL.W,CPU.HL.B.l);goto z80_loop_end;

LD_B_A:   CPU.BC.B.h=CPU.AF.B.h;goto z80_loop_end;
LD_C_A:   CPU.BC.B.l=CPU.AF.B.h;goto z80_loop_end;
LD_D_A:   CPU.DE.B.h=CPU.AF.B.h;goto z80_loop_end;
LD_E_A:   CPU.DE.B.l=CPU.AF.B.h;goto z80_loop_end;
LD_H_A:   CPU.HL.B.h=CPU.AF.B.h;goto z80_loop_end;
LD_L_A:   CPU.HL.B.l=CPU.AF.B.h;goto z80_loop_end;
LD_A_A:   CPU.AF.B.h=CPU.AF.B.h;goto z80_loop_end;
LD_xHL_A: WrZ80(CPU.HL.W,CPU.AF.B.h);goto z80_loop_end;

LD_xBC_A: WrZ80(CPU.BC.W,CPU.AF.B.h);goto z80_loop_end;
LD_xDE_A: WrZ80(CPU.DE.W,CPU.AF.B.h);goto z80_loop_end;

LD_B_xHL:    CPU.BC.B.h=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_C_xHL:    CPU.BC.B.l=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_D_xHL:    CPU.DE.B.h=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_E_xHL:    CPU.DE.B.l=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_H_xHL:    CPU.HL.B.h=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_L_xHL:    CPU.HL.B.l=RdZ80(CPU.HL.W);goto z80_loop_end;
LD_A_xHL:    CPU.AF.B.h=RdZ80(CPU.HL.W);goto z80_loop_end;

LD_B_BYTE:   CPU.BC.B.h=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_C_BYTE:   CPU.BC.B.l=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_D_BYTE:   CPU.DE.B.h=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_E_BYTE:   CPU.DE.B.l=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_H_BYTE:   CPU.HL.B.h=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_L_BYTE:   CPU.HL.B.l=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_A_BYTE:   CPU.AF.B.h=RdZ80(CPU.PC.W++);goto z80_loop_end;
LD_xHL_BYTE: WrZ80(CPU.HL.W,RdZ80(CPU.PC.W++));goto z80_loop_end;

LD_xWORD_HL:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.HL.B.l);
  WrZ80(J.W,CPU.HL.B.h);
  goto z80_loop_end;

LD_HL_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.HL.B.l=RdZ80(J.W++);
  CPU.HL.B.h=RdZ80(J.W);
  goto z80_loop_end;

LD_A_xWORD:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++); 
  CPU.AF.B.h=RdZ80(J.W);
  goto z80_loop_end;

LD_xWORD_A:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W,CPU.AF.B.h);
  goto z80_loop_end;

EX_HL_xSP:
  J.B.l=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W++,CPU.HL.B.l);
  J.B.h=RdZ80(CPU.SP.W);WrZ80(CPU.SP.W--,CPU.HL.B.h);
  CPU.HL.W=J.W;
  goto z80_loop_end;

DAA:
  J.W=CPU.AF.B.h;
  if(CPU.AF.B.l&C_FLAG) J.W|=256;
  if(CPU.AF.B.l&H_FLAG) J.W|=512;
  if(CPU.AF.B.l&N_FLAG) J.W|=1024;
  CPU.AF.W=DAATable[J.W];
  goto z80_loop_end;

PFX_CB: CodesCB(); goto z80_loop_end;
PFX_ED: CodesED(); goto z80_loop_end;
PFX_FD: CodesFD(); goto z80_loop_end;
PFX_DD: CodesDD(); goto z80_loop_end;
    //}
 
  /* Execution stopped */
  return(CPU.PC.W);
}
