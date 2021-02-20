/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                         CodesED.h                       **/
/**                                                         **/
/** This file contains implementation for the ED table of   **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/** This is a special patch for emulating BIOS calls: ********/
case DB_FE:     PatchZ80();break;
/*************************************************************/

case ADC_HL_BC: M_ADCW(BC);break;
case ADC_HL_DE: M_ADCW(DE);break;
case ADC_HL_HL: M_ADCW(HL);break;
case ADC_HL_SP: M_ADCW(SP);break;

case SBC_HL_BC: M_SBCW(BC);break;
case SBC_HL_DE: M_SBCW(DE);break;
case SBC_HL_HL: M_SBCW(HL);break;
case SBC_HL_SP: M_SBCW(SP);break;

case LD_xWORDe_HL:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.HL.B.l);
  WrZ80(J.W,CPU.HL.B.h);
  break;
case LD_xWORDe_DE:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.DE.B.l);
  WrZ80(J.W,CPU.DE.B.h);
  break;
case LD_xWORDe_BC:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.BC.B.l);
  WrZ80(J.W,CPU.BC.B.h);
  break;
case LD_xWORDe_SP:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  WrZ80(J.W++,CPU.SP.B.l);
  WrZ80(J.W,CPU.SP.B.h);
  break;

case LD_HL_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.HL.B.l=RdZ80(J.W++);
  CPU.HL.B.h=RdZ80(J.W);
  break;
case LD_DE_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.DE.B.l=RdZ80(J.W++);
  CPU.DE.B.h=RdZ80(J.W);
  break;
case LD_BC_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.BC.B.l=RdZ80(J.W++);
  CPU.BC.B.h=RdZ80(J.W);
  break;
case LD_SP_xWORDe:
  J.B.l=RdZ80(CPU.PC.W++);
  J.B.h=RdZ80(CPU.PC.W++);
  CPU.SP.B.l=RdZ80(J.W++);
  CPU.SP.B.h=RdZ80(J.W);
  break;

case RRD:
  I=RdZ80(CPU.HL.W);
  J.B.l=(I>>4)|(CPU.AF.B.h<<4);
  WrZ80(CPU.HL.W,J.B.l);
  CPU.AF.B.h=(I&0x0F)|(CPU.AF.B.h&0xF0);
  CPU.AF.B.l=PZSTable[CPU.AF.B.h]|(CPU.AF.B.l&C_FLAG);
  break;
case RLD:
  I=RdZ80(CPU.HL.W);
  J.B.l=(I<<4)|(CPU.AF.B.h&0x0F);
  WrZ80(CPU.HL.W,J.B.l);
  CPU.AF.B.h=(I>>4)|(CPU.AF.B.h&0xF0);
  CPU.AF.B.l=PZSTable[CPU.AF.B.h]|(CPU.AF.B.l&C_FLAG);
  break;

case LD_A_I:
  CPU.AF.B.h=CPU.I;
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|(CPU.IFF&1? P_FLAG:0)|ZSTable[CPU.AF.B.h];
  break;

case LD_A_R:
  CPU.R++;
  CPU.AF.B.h=(byte)(CPU.R-CPU.ICount);
  CPU.AF.B.l=(CPU.AF.B.l&C_FLAG)|(CPU.IFF&1? P_FLAG:0)|ZSTable[CPU.AF.B.h];
  break;

case LD_I_A:   CPU.I=CPU.AF.B.h;break;
case LD_R_A:   break;

case IM_0:     CPU.IFF&=0xF9;break;
case IM_1:     CPU.IFF=(CPU.IFF&0xF9)|2;break;
case IM_2:     CPU.IFF=(CPU.IFF&0xF9)|4;break;

case RETI:     M_RET;break;
case RETN:     if(CPU.IFF&0x40) CPU.IFF|=0x01; else CPU.IFF&=0xFE;
               M_RET;break;

case NEG:      I=CPU.AF.B.h;CPU.AF.B.h=0;M_SUB(I);break;

case IN_B_xC:  M_IN(CPU.BC.B.h);break;
case IN_C_xC:  M_IN(CPU.BC.B.l);break;
case IN_D_xC:  M_IN(CPU.DE.B.h);break;
case IN_E_xC:  M_IN(CPU.DE.B.l);break;
case IN_H_xC:  M_IN(CPU.HL.B.h);break;
case IN_L_xC:  M_IN(CPU.HL.B.l);break;
case IN_A_xC:  M_IN(CPU.AF.B.h);break;
case IN_F_xC:  M_IN(J.B.l);break;

case OUT_xC_B: OutZ80(CPU.BC.B.l,CPU.BC.B.h);break;
case OUT_xC_C: OutZ80(CPU.BC.B.l,CPU.BC.B.l);break;
case OUT_xC_D: OutZ80(CPU.BC.B.l,CPU.DE.B.h);break;
case OUT_xC_E: OutZ80(CPU.BC.B.l,CPU.DE.B.l);break;
case OUT_xC_H: OutZ80(CPU.BC.B.l,CPU.HL.B.h);break;
case OUT_xC_L: OutZ80(CPU.BC.B.l,CPU.HL.B.l);break;
case OUT_xC_A: OutZ80(CPU.BC.B.l,CPU.AF.B.h);break;

case INI:
  WrZ80(CPU.HL.W++,InZ80(CPU.BC.B.l));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  break;

case INIR:
  do
  {
    WrZ80(CPU.HL.W++,InZ80(CPU.BC.B.l));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  break;

case IND:
  WrZ80(CPU.HL.W--,InZ80(CPU.BC.B.l));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  break;

case INDR:
  do
  {
    WrZ80(CPU.HL.W--,InZ80(CPU.BC.B.l));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  break;

case OUTI:
  OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W++));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  break;

case OTIR:
  do
  {
    OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W++));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  break;

case OUTD:
  OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W--));
  CPU.BC.B.h--;
  CPU.AF.B.l=N_FLAG|(CPU.BC.B.h? 0:Z_FLAG);
  break;

case OTDR:
  do
  {
    OutZ80(CPU.BC.B.l,RdZ80(CPU.HL.W--));
    CPU.BC.B.h--;CPU.ICount-=21;
  }
  while(CPU.BC.B.h&&(CPU.ICount>0));
  if(CPU.BC.B.h) { CPU.AF.B.l=N_FLAG;CPU.PC.W-=2; }
  else { CPU.AF.B.l=Z_FLAG|N_FLAG;CPU.ICount+=5; }
  break;

case LDI:
  WrZ80(CPU.DE.W++,RdZ80(CPU.HL.W++));
  CPU.BC.W--;
  CPU.AF.B.l=(CPU.AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(CPU.BC.W? P_FLAG:0);
  break;

case LDIR:
  do
  {
    WrZ80(CPU.DE.W++,RdZ80(CPU.HL.W++));
    CPU.BC.W--;CPU.ICount-=21;
  }
  while(CPU.BC.W&&(CPU.ICount>0));
  CPU.AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(CPU.BC.W) { CPU.AF.B.l|=N_FLAG;CPU.PC.W-=2; }
  else CPU.ICount+=5;
  break;

case LDD:
  WrZ80(CPU.DE.W--,RdZ80(CPU.HL.W--));
  CPU.BC.W--;
  CPU.AF.B.l=(CPU.AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(CPU.BC.W? P_FLAG:0);
  break;

case LDDR:
  do
  {
    WrZ80(CPU.DE.W--,RdZ80(CPU.HL.W--));
    CPU.BC.W--;CPU.ICount-=21;
  }
  while(CPU.BC.W&&(CPU.ICount>0));
  CPU.AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(CPU.BC.W) { CPU.AF.B.l|=N_FLAG;CPU.PC.W-=2; }
  else CPU.ICount+=5;
  break;

case CPI:
  I=RdZ80(CPU.HL.W++);
  J.B.l=CPU.AF.B.h-I;
  CPU.BC.W--;
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  break;

case CPIR:
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
  break;  

case CPD:
  I=RdZ80(CPU.HL.W--);
  J.B.l=CPU.AF.B.h-I;
  CPU.BC.W--;
  CPU.AF.B.l =
    N_FLAG|(CPU.AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((CPU.AF.B.h^I^J.B.l)&H_FLAG)|(CPU.BC.W? P_FLAG:0);
  break;

case CPDR:
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
  break;
