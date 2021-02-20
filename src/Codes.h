/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                          Codes.h                        **/
/**                                                         **/
/** This file contains implementation for the main table of **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

JR_NZ:   if(R->AF.B.l&Z_FLAG) R->PC.W++; else { R->ICount-=5;M_JR; } goto z80_loop_end;
JR_NC:   if(R->AF.B.l&C_FLAG) R->PC.W++; else { R->ICount-=5;M_JR; } goto z80_loop_end;
JR_Z:    if(R->AF.B.l&Z_FLAG) { R->ICount-=5;M_JR; } else R->PC.W++; goto z80_loop_end;
JR_C:    if(R->AF.B.l&C_FLAG) { R->ICount-=5;M_JR; } else R->PC.W++; goto z80_loop_end;

JP_NZ:   if(R->AF.B.l&Z_FLAG) R->PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_NC:   if(R->AF.B.l&C_FLAG) R->PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_PO:   if(R->AF.B.l&P_FLAG) R->PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_P:    if(R->AF.B.l&S_FLAG) R->PC.W+=2; else { M_JP; } goto z80_loop_end;
JP_Z:    if(R->AF.B.l&Z_FLAG) { M_JP; } else R->PC.W+=2; goto z80_loop_end;
JP_C:    if(R->AF.B.l&C_FLAG) { M_JP; } else R->PC.W+=2; goto z80_loop_end;
JP_PE:   if(R->AF.B.l&P_FLAG) { M_JP; } else R->PC.W+=2; goto z80_loop_end;
JP_M:    if(R->AF.B.l&S_FLAG) { M_JP; } else R->PC.W+=2; goto z80_loop_end;

RET_NZ:  if(!(R->AF.B.l&Z_FLAG)) { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_NC:  if(!(R->AF.B.l&C_FLAG)) { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_PO:  if(!(R->AF.B.l&P_FLAG)) { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_P:   if(!(R->AF.B.l&S_FLAG)) { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_Z:   if(R->AF.B.l&Z_FLAG)    { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_C:   if(R->AF.B.l&C_FLAG)    { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_PE:  if(R->AF.B.l&P_FLAG)    { R->ICount-=6;M_RET; } goto z80_loop_end;
RET_M:   if(R->AF.B.l&S_FLAG)    { R->ICount-=6;M_RET; } goto z80_loop_end;

CALL_NZ: if(R->AF.B.l&Z_FLAG) R->PC.W+=2; else { R->ICount-=7;M_CALL; } goto z80_loop_end;
CALL_NC: if(R->AF.B.l&C_FLAG) R->PC.W+=2; else { R->ICount-=7;M_CALL; } goto z80_loop_end;
CALL_PO: if(R->AF.B.l&P_FLAG) R->PC.W+=2; else { R->ICount-=7;M_CALL; } goto z80_loop_end;
CALL_P:  if(R->AF.B.l&S_FLAG) R->PC.W+=2; else { R->ICount-=7;M_CALL; } goto z80_loop_end;
CALL_Z:  if(R->AF.B.l&Z_FLAG) { R->ICount-=7;M_CALL; } else R->PC.W+=2; goto z80_loop_end;
CALL_C:  if(R->AF.B.l&C_FLAG) { R->ICount-=7;M_CALL; } else R->PC.W+=2; goto z80_loop_end;
CALL_PE: if(R->AF.B.l&P_FLAG) { R->ICount-=7;M_CALL; } else R->PC.W+=2; goto z80_loop_end;
CALL_M:  if(R->AF.B.l&S_FLAG) { R->ICount-=7;M_CALL; } else R->PC.W+=2; goto z80_loop_end;

ADD_B:    M_ADD(R->BC.B.h);goto z80_loop_end;
ADD_C:    M_ADD(R->BC.B.l);goto z80_loop_end;
ADD_D:    M_ADD(R->DE.B.h);goto z80_loop_end;
ADD_E:    M_ADD(R->DE.B.l);goto z80_loop_end;
ADD_H:    M_ADD(R->HL.B.h);goto z80_loop_end;
ADD_L:    M_ADD(R->HL.B.l);goto z80_loop_end;
ADD_A:    M_ADD(R->AF.B.h);goto z80_loop_end;
ADD_xHL:  I=RdZ80(R->HL.W);M_ADD(I);goto z80_loop_end;
ADD_BYTE: I=RdZ80(R->PC.W++);M_ADD(I);goto z80_loop_end;

SUB_B:    M_SUB(R->BC.B.h);goto z80_loop_end;
SUB_C:    M_SUB(R->BC.B.l);goto z80_loop_end;
SUB_D:    M_SUB(R->DE.B.h);goto z80_loop_end;
SUB_E:    M_SUB(R->DE.B.l);goto z80_loop_end;
SUB_H:    M_SUB(R->HL.B.h);goto z80_loop_end;
SUB_L:    M_SUB(R->HL.B.l);goto z80_loop_end;
SUB_A:    R->AF.B.h=0;R->AF.B.l=N_FLAG|Z_FLAG;goto z80_loop_end;
SUB_xHL:  I=RdZ80(R->HL.W);M_SUB(I);goto z80_loop_end;
SUB_BYTE: I=RdZ80(R->PC.W++);M_SUB(I);goto z80_loop_end;

AND_B:    M_AND(R->BC.B.h);goto z80_loop_end;
AND_C:    M_AND(R->BC.B.l);goto z80_loop_end;
AND_D:    M_AND(R->DE.B.h);goto z80_loop_end;
AND_E:    M_AND(R->DE.B.l);goto z80_loop_end;
AND_H:    M_AND(R->HL.B.h);goto z80_loop_end;
AND_L:    M_AND(R->HL.B.l);goto z80_loop_end;
AND_A:    M_AND(R->AF.B.h);goto z80_loop_end;
AND_xHL:  I=RdZ80(R->HL.W);M_AND(I);goto z80_loop_end;
AND_BYTE: I=RdZ80(R->PC.W++);M_AND(I);goto z80_loop_end;

OR_B:     M_OR(R->BC.B.h);goto z80_loop_end;
OR_C:     M_OR(R->BC.B.l);goto z80_loop_end;
OR_D:     M_OR(R->DE.B.h);goto z80_loop_end;
OR_E:     M_OR(R->DE.B.l);goto z80_loop_end;
OR_H:     M_OR(R->HL.B.h);goto z80_loop_end;
OR_L:     M_OR(R->HL.B.l);goto z80_loop_end;
OR_A:     M_OR(R->AF.B.h);goto z80_loop_end;
OR_xHL:   I=RdZ80(R->HL.W);M_OR(I);goto z80_loop_end;
OR_BYTE:  I=RdZ80(R->PC.W++);M_OR(I);goto z80_loop_end;

ADC_B:    M_ADC(R->BC.B.h);goto z80_loop_end;
ADC_C:    M_ADC(R->BC.B.l);goto z80_loop_end;
ADC_D:    M_ADC(R->DE.B.h);goto z80_loop_end;
ADC_E:    M_ADC(R->DE.B.l);goto z80_loop_end;
ADC_H:    M_ADC(R->HL.B.h);goto z80_loop_end;
ADC_L:    M_ADC(R->HL.B.l);goto z80_loop_end;
ADC_A:    M_ADC(R->AF.B.h);goto z80_loop_end;
ADC_xHL:  I=RdZ80(R->HL.W);M_ADC(I);goto z80_loop_end;
ADC_BYTE: I=RdZ80(R->PC.W++);M_ADC(I);goto z80_loop_end;

SBC_B:    M_SBC(R->BC.B.h);goto z80_loop_end;
SBC_C:    M_SBC(R->BC.B.l);goto z80_loop_end;
SBC_D:    M_SBC(R->DE.B.h);goto z80_loop_end;
SBC_E:    M_SBC(R->DE.B.l);goto z80_loop_end;
SBC_H:    M_SBC(R->HL.B.h);goto z80_loop_end;
SBC_L:    M_SBC(R->HL.B.l);goto z80_loop_end;
SBC_A:    M_SBC(R->AF.B.h);goto z80_loop_end;
SBC_xHL:  I=RdZ80(R->HL.W);M_SBC(I);goto z80_loop_end;
SBC_BYTE: I=RdZ80(R->PC.W++);M_SBC(I);goto z80_loop_end;

XOR_B:    M_XOR(R->BC.B.h);goto z80_loop_end;
XOR_C:    M_XOR(R->BC.B.l);goto z80_loop_end;
XOR_D:    M_XOR(R->DE.B.h);goto z80_loop_end;
XOR_E:    M_XOR(R->DE.B.l);goto z80_loop_end;
XOR_H:    M_XOR(R->HL.B.h);goto z80_loop_end;
XOR_L:    M_XOR(R->HL.B.l);goto z80_loop_end;
XOR_A:    R->AF.B.h=0;R->AF.B.l=P_FLAG|Z_FLAG;goto z80_loop_end;
XOR_xHL:  I=RdZ80(R->HL.W);M_XOR(I);goto z80_loop_end;
XOR_BYTE: I=RdZ80(R->PC.W++);M_XOR(I);goto z80_loop_end;

CP_B:     M_CP(R->BC.B.h);goto z80_loop_end;
CP_C:     M_CP(R->BC.B.l);goto z80_loop_end;
CP_D:     M_CP(R->DE.B.h);goto z80_loop_end;
CP_E:     M_CP(R->DE.B.l);goto z80_loop_end;
CP_H:     M_CP(R->HL.B.h);goto z80_loop_end;
CP_L:     M_CP(R->HL.B.l);goto z80_loop_end;
CP_A:     R->AF.B.l=N_FLAG|Z_FLAG;goto z80_loop_end;
CP_xHL:   I=RdZ80(R->HL.W);M_CP(I);goto z80_loop_end;
CP_BYTE:  I=RdZ80(R->PC.W++);M_CP(I);goto z80_loop_end;
               
LD_BC_WORD: M_LDWORD(BC);goto z80_loop_end;
LD_DE_WORD: M_LDWORD(DE);goto z80_loop_end;
LD_HL_WORD: M_LDWORD(HL);goto z80_loop_end;
LD_SP_WORD: M_LDWORD(SP);goto z80_loop_end;

LD_PC_HL: R->PC.W=R->HL.W;goto z80_loop_end;
LD_SP_HL: R->SP.W=R->HL.W;goto z80_loop_end;
LD_A_xBC: R->AF.B.h=RdZ80(R->BC.W);goto z80_loop_end;
LD_A_xDE: R->AF.B.h=RdZ80(R->DE.W);goto z80_loop_end;

ADD_HL_BC:  M_ADDW(HL,BC);goto z80_loop_end;
ADD_HL_DE:  M_ADDW(HL,DE);goto z80_loop_end;
ADD_HL_HL:  M_ADDW(HL,HL);goto z80_loop_end;
ADD_HL_SP:  M_ADDW(HL,SP);goto z80_loop_end;

DEC_BC:   R->BC.W--;goto z80_loop_end;
DEC_DE:   R->DE.W--;goto z80_loop_end;
DEC_HL:   R->HL.W--;goto z80_loop_end;
DEC_SP:   R->SP.W--;goto z80_loop_end;

INC_BC:   R->BC.W++;goto z80_loop_end;
INC_DE:   R->DE.W++;goto z80_loop_end;
INC_HL:   R->HL.W++;goto z80_loop_end;
INC_SP:   R->SP.W++;goto z80_loop_end;

DEC_B:    M_DEC(R->BC.B.h);goto z80_loop_end;
DEC_C:    M_DEC(R->BC.B.l);goto z80_loop_end;
DEC_D:    M_DEC(R->DE.B.h);goto z80_loop_end;
DEC_E:    M_DEC(R->DE.B.l);goto z80_loop_end;
DEC_H:    M_DEC(R->HL.B.h);goto z80_loop_end;
DEC_L:    M_DEC(R->HL.B.l);goto z80_loop_end;
DEC_A:    M_DEC(R->AF.B.h);goto z80_loop_end;
DEC_xHL:  I=RdZ80(R->HL.W);M_DEC(I);WrZ80(R->HL.W,I);goto z80_loop_end;

INC_B:    M_INC(R->BC.B.h);goto z80_loop_end;
INC_C:    M_INC(R->BC.B.l);goto z80_loop_end;
INC_D:    M_INC(R->DE.B.h);goto z80_loop_end;
INC_E:    M_INC(R->DE.B.l);goto z80_loop_end;
INC_H:    M_INC(R->HL.B.h);goto z80_loop_end;
INC_L:    M_INC(R->HL.B.l);goto z80_loop_end;
INC_A:    M_INC(R->AF.B.h);goto z80_loop_end;
INC_xHL:  I=RdZ80(R->HL.W);M_INC(I);WrZ80(R->HL.W,I);goto z80_loop_end;

RLCA:
  I=R->AF.B.h&0x80? C_FLAG:0;
  R->AF.B.h=(R->AF.B.h<<1)|I;
  R->AF.B.l=(R->AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  goto z80_loop_end;
RLA:
  I=R->AF.B.h&0x80? C_FLAG:0;
  R->AF.B.h=(R->AF.B.h<<1)|(R->AF.B.l&C_FLAG);
  R->AF.B.l=(R->AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
  goto z80_loop_end;
RRCA:
  I=R->AF.B.h&0x01;
  R->AF.B.h=(R->AF.B.h>>1)|(I? 0x80:0);
  R->AF.B.l=(R->AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I; 
  goto z80_loop_end;
RRA:
  I=R->AF.B.h&0x01;
  R->AF.B.h=(R->AF.B.h>>1)|(R->AF.B.l&C_FLAG? 0x80:0);
  R->AF.B.l=(R->AF.B.l&~(C_FLAG|N_FLAG|H_FLAG))|I;
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

DJNZ: if(--R->BC.B.h) { M_JR; } else R->PC.W++;goto z80_loop_end;
JP:   M_JP;goto z80_loop_end;
JR:   M_JR;goto z80_loop_end;
CALL: M_CALL;goto z80_loop_end;
RET:  M_RET;goto z80_loop_end;
SCF:  S(C_FLAG);R(N_FLAG|H_FLAG);goto z80_loop_end;
CPL:  R->AF.B.h=~R->AF.B.h;S(N_FLAG|H_FLAG);goto z80_loop_end;
NOP:  goto z80_loop_end;
OUTA: OutZ80(RdZ80(R->PC.W++),R->AF.B.h);goto z80_loop_end;
INA:  R->AF.B.h=InZ80(RdZ80(R->PC.W++));goto z80_loop_end;
HALT: R->PC.W--;R->IFF|=0x80;R->ICount=0;goto z80_loop_end;

DI:
  R->IFF&=0xFE;
  goto z80_loop_end;
EI:
  R->IFF|=0x01;
  if(R->IRequest!=INT_NONE)
  {
    R->IFF|=0x20;
    R->IBackup=R->ICount;
    R->ICount=1;
  }
  goto z80_loop_end;

CCF:
  R->AF.B.l^=C_FLAG;R(N_FLAG|H_FLAG);
  R->AF.B.l|=R->AF.B.l&C_FLAG? 0:H_FLAG;
  goto z80_loop_end;

EXX:
  J.W=R->BC.W;R->BC.W=R->BC1.W;R->BC1.W=J.W;
  J.W=R->DE.W;R->DE.W=R->DE1.W;R->DE1.W=J.W;
  J.W=R->HL.W;R->HL.W=R->HL1.W;R->HL1.W=J.W;
  goto z80_loop_end;

EX_DE_HL: J.W=R->DE.W;R->DE.W=R->HL.W;R->HL.W=J.W;goto z80_loop_end;
EX_AF_AF: J.W=R->AF.W;R->AF.W=R->AF1.W;R->AF1.W=J.W;goto z80_loop_end;  
  
LD_B_B:   R->BC.B.h=R->BC.B.h;goto z80_loop_end;
LD_C_B:   R->BC.B.l=R->BC.B.h;goto z80_loop_end;
LD_D_B:   R->DE.B.h=R->BC.B.h;goto z80_loop_end;
LD_E_B:   R->DE.B.l=R->BC.B.h;goto z80_loop_end;
LD_H_B:   R->HL.B.h=R->BC.B.h;goto z80_loop_end;
LD_L_B:   R->HL.B.l=R->BC.B.h;goto z80_loop_end;
LD_A_B:   R->AF.B.h=R->BC.B.h;goto z80_loop_end;
LD_xHL_B: WrZ80(R->HL.W,R->BC.B.h);goto z80_loop_end;

LD_B_C:   R->BC.B.h=R->BC.B.l;goto z80_loop_end;
LD_C_C:   R->BC.B.l=R->BC.B.l;goto z80_loop_end;
LD_D_C:   R->DE.B.h=R->BC.B.l;goto z80_loop_end;
LD_E_C:   R->DE.B.l=R->BC.B.l;goto z80_loop_end;
LD_H_C:   R->HL.B.h=R->BC.B.l;goto z80_loop_end;
LD_L_C:   R->HL.B.l=R->BC.B.l;goto z80_loop_end;
LD_A_C:   R->AF.B.h=R->BC.B.l;goto z80_loop_end;
LD_xHL_C: WrZ80(R->HL.W,R->BC.B.l);goto z80_loop_end;

LD_B_D:   R->BC.B.h=R->DE.B.h;goto z80_loop_end;
LD_C_D:   R->BC.B.l=R->DE.B.h;goto z80_loop_end;
LD_D_D:   R->DE.B.h=R->DE.B.h;goto z80_loop_end;
LD_E_D:   R->DE.B.l=R->DE.B.h;goto z80_loop_end;
LD_H_D:   R->HL.B.h=R->DE.B.h;goto z80_loop_end;
LD_L_D:   R->HL.B.l=R->DE.B.h;goto z80_loop_end;
LD_A_D:   R->AF.B.h=R->DE.B.h;goto z80_loop_end;
LD_xHL_D: WrZ80(R->HL.W,R->DE.B.h);goto z80_loop_end;

LD_B_E:   R->BC.B.h=R->DE.B.l;goto z80_loop_end;
LD_C_E:   R->BC.B.l=R->DE.B.l;goto z80_loop_end;
LD_D_E:   R->DE.B.h=R->DE.B.l;goto z80_loop_end;
LD_E_E:   R->DE.B.l=R->DE.B.l;goto z80_loop_end;
LD_H_E:   R->HL.B.h=R->DE.B.l;goto z80_loop_end;
LD_L_E:   R->HL.B.l=R->DE.B.l;goto z80_loop_end;
LD_A_E:   R->AF.B.h=R->DE.B.l;goto z80_loop_end;
LD_xHL_E: WrZ80(R->HL.W,R->DE.B.l);goto z80_loop_end;

LD_B_H:   R->BC.B.h=R->HL.B.h;goto z80_loop_end;
LD_C_H:   R->BC.B.l=R->HL.B.h;goto z80_loop_end;
LD_D_H:   R->DE.B.h=R->HL.B.h;goto z80_loop_end;
LD_E_H:   R->DE.B.l=R->HL.B.h;goto z80_loop_end;
LD_H_H:   R->HL.B.h=R->HL.B.h;goto z80_loop_end;
LD_L_H:   R->HL.B.l=R->HL.B.h;goto z80_loop_end;
LD_A_H:   R->AF.B.h=R->HL.B.h;goto z80_loop_end;
LD_xHL_H: WrZ80(R->HL.W,R->HL.B.h);goto z80_loop_end;

LD_B_L:   R->BC.B.h=R->HL.B.l;goto z80_loop_end;
LD_C_L:   R->BC.B.l=R->HL.B.l;goto z80_loop_end;
LD_D_L:   R->DE.B.h=R->HL.B.l;goto z80_loop_end;
LD_E_L:   R->DE.B.l=R->HL.B.l;goto z80_loop_end;
LD_H_L:   R->HL.B.h=R->HL.B.l;goto z80_loop_end;
LD_L_L:   R->HL.B.l=R->HL.B.l;goto z80_loop_end;
LD_A_L:   R->AF.B.h=R->HL.B.l;goto z80_loop_end;
LD_xHL_L: WrZ80(R->HL.W,R->HL.B.l);goto z80_loop_end;

LD_B_A:   R->BC.B.h=R->AF.B.h;goto z80_loop_end;
LD_C_A:   R->BC.B.l=R->AF.B.h;goto z80_loop_end;
LD_D_A:   R->DE.B.h=R->AF.B.h;goto z80_loop_end;
LD_E_A:   R->DE.B.l=R->AF.B.h;goto z80_loop_end;
LD_H_A:   R->HL.B.h=R->AF.B.h;goto z80_loop_end;
LD_L_A:   R->HL.B.l=R->AF.B.h;goto z80_loop_end;
LD_A_A:   R->AF.B.h=R->AF.B.h;goto z80_loop_end;
LD_xHL_A: WrZ80(R->HL.W,R->AF.B.h);goto z80_loop_end;

LD_xBC_A: WrZ80(R->BC.W,R->AF.B.h);goto z80_loop_end;
LD_xDE_A: WrZ80(R->DE.W,R->AF.B.h);goto z80_loop_end;

LD_B_xHL:    R->BC.B.h=RdZ80(R->HL.W);goto z80_loop_end;
LD_C_xHL:    R->BC.B.l=RdZ80(R->HL.W);goto z80_loop_end;
LD_D_xHL:    R->DE.B.h=RdZ80(R->HL.W);goto z80_loop_end;
LD_E_xHL:    R->DE.B.l=RdZ80(R->HL.W);goto z80_loop_end;
LD_H_xHL:    R->HL.B.h=RdZ80(R->HL.W);goto z80_loop_end;
LD_L_xHL:    R->HL.B.l=RdZ80(R->HL.W);goto z80_loop_end;
LD_A_xHL:    R->AF.B.h=RdZ80(R->HL.W);goto z80_loop_end;

LD_B_BYTE:   R->BC.B.h=RdZ80(R->PC.W++);goto z80_loop_end;
LD_C_BYTE:   R->BC.B.l=RdZ80(R->PC.W++);goto z80_loop_end;
LD_D_BYTE:   R->DE.B.h=RdZ80(R->PC.W++);goto z80_loop_end;
LD_E_BYTE:   R->DE.B.l=RdZ80(R->PC.W++);goto z80_loop_end;
LD_H_BYTE:   R->HL.B.h=RdZ80(R->PC.W++);goto z80_loop_end;
LD_L_BYTE:   R->HL.B.l=RdZ80(R->PC.W++);goto z80_loop_end;
LD_A_BYTE:   R->AF.B.h=RdZ80(R->PC.W++);goto z80_loop_end;
LD_xHL_BYTE: WrZ80(R->HL.W,RdZ80(R->PC.W++));goto z80_loop_end;

LD_xWORD_HL:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W++,R->HL.B.l);
  WrZ80(J.W,R->HL.B.h);
  goto z80_loop_end;

LD_HL_xWORD:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  R->HL.B.l=RdZ80(J.W++);
  R->HL.B.h=RdZ80(J.W);
  goto z80_loop_end;

LD_A_xWORD:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++); 
  R->AF.B.h=RdZ80(J.W);
  goto z80_loop_end;

LD_xWORD_A:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W,R->AF.B.h);
  goto z80_loop_end;

EX_HL_xSP:
  J.B.l=RdZ80(R->SP.W);WrZ80(R->SP.W++,R->HL.B.l);
  J.B.h=RdZ80(R->SP.W);WrZ80(R->SP.W--,R->HL.B.h);
  R->HL.W=J.W;
  goto z80_loop_end;

DAA:
  J.W=R->AF.B.h;
  if(R->AF.B.l&C_FLAG) J.W|=256;
  if(R->AF.B.l&H_FLAG) J.W|=512;
  if(R->AF.B.l&N_FLAG) J.W|=1024;
  R->AF.W=DAATable[J.W];
  goto z80_loop_end;
