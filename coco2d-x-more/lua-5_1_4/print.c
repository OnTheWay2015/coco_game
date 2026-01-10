/*
** $Id: print.c,v 1.55a 2006/05/31 13:30:05 lhf Exp $
** print bytecodes
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdio.h>

#define luac_c
#define LUA_CORE

#include "ldebug.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lundump.h"


#define Sizeof(x)	((int)sizeof(x))
#define VOID(p)		((const void*)(p))

static void PrintString(const TString* ts)
{
 const char* s=getstr(ts);
 size_t i,n=ts->tsv.len;
 putchar('"');
 for (i=0; i<n; i++)
 {
  int c=s[i];
  switch (c)
  {
   case '"': printf("\\\""); break;
   case '\\': printf("\\\\"); break;
   case '\a': printf("\\a"); break;
   case '\b': printf("\\b"); break;
   case '\f': printf("\\f"); break;
   case '\n': printf("\\n"); break;
   case '\r': printf("\\r"); break;
   case '\t': printf("\\t"); break;
   case '\v': printf("\\v"); break;
   default:	if (isprint((unsigned char)c))
   			putchar(c);
		else
			printf("\\%03u",(unsigned char)c);
  }
 }
 putchar('"');
}

static void PrintConstant(const Proto* f, int i)
{
 const TValue* o=&f->k[i];
 switch (ttype(o))
 {
  case LUA_TNIL:
	printf("nil");
	break;
  case LUA_TBOOLEAN:
	printf(bvalue(o) ? "true" : "false");
	break;
  case LUA_TNUMBER:
	printf(LUA_NUMBER_FMT,nvalue(o));
	break;
  case LUA_TSTRING:
	PrintString(rawtsvalue(o));
	break;
  default:				/* cannot happen */
	printf("? type=%d",ttype(o));
	break;
 }
}

static void PrintCode(const Proto* f)
{
    printf("function Instructions start: \n");
 const Instruction* code=f->code;
 int pc,n=f->sizecode;
  int a=0;
  int b=0;
  int c=0;
  int bx=0;
  int sbx=0;
  int line=0;
  OpCode o = 0;
 for (pc=0; pc<n; pc++)
 {
  Instruction i=code[pc];
  o=GET_OPCODE(i);
  
  a=GETARG_A(i);
  b=GETARG_B(i);
  c=GETARG_C(i);
  bx=GETARG_Bx(i);
  sbx=GETARG_sBx(i);
  line=getline(f,pc);
  printf("\t%d\t",pc+1);
  if (line>0) printf("line[%d]\t",line); else printf("[-]\t");
  printf("%-9s\t",luaP_opnames[o]);
  
  switch (getOpMode(o))
  {
   case iABC:
    printf("iABC argA[%d]",a);
    if (getBMode(o)!=OpArgN /* OpArgN 参数未设置*/) printf(" argB[%d]",ISK(b) ? (-1-INDEXK(b)) : b);
    if (getCMode(o)!=OpArgN /* OpArgN 参数未设置*/) printf(" argC[%d]",ISK(c) ? (-1-INDEXK(c)) : c);
    break;
   case iABx:
    //为什么 bx 要做处理 -1-bx? 
    //if (getBMode(o)==OpArgK) printf("iABx argA[%d] (符号.常量表)argBx[%d]",a,-1-bx); else printf("iABx argA[%d] arbBx[%d]",a,bx);
    //if (getBMode(o)==OpArgK) 
    //{
    //    printf("iABx argA[%d] argBx[%d]",a,bx/*在OP_LOADK 使用时直接用的是 bx 在符号.常量表的索引 */); 
    //}
    //else 
    //{
    //    printf("iABx argA[%d] arbBx[%d]",a,bx);
    //} 
    break;
   case iAsBx:
    if (o==OP_JMP) printf("OP_JMP argSBx[%d]",sbx); else printf("argA[%d] argSBx[%d]",a,sbx);
    break;
  }

  switch (o)
  {
   case OP_LOADK:
    printf("\t; "); PrintConstant(f,bx);
    break;
   case OP_GETUPVAL:
   case OP_SETUPVAL:
    printf("\t; %s", (f->sizeupvalues>0) ? getstr(f->upvalues[b]) : "-");
    break;
   case OP_GETGLOBAL:
   case OP_SETGLOBAL:
    printf("\t; %s",svalue(&f->k[bx]));
    break;
   case OP_GETTABLE:
   case OP_SELF:
    if (ISK(c)) { printf("\t; "); PrintConstant(f,INDEXK(c)); }
    break;
   case OP_SETTABLE:
   case OP_ADD:
   case OP_SUB:
   case OP_MUL:
   case OP_DIV:
   case OP_POW:
   case OP_EQ:
   case OP_LT:
   case OP_LE:
    if (ISK(b) || ISK(c))
    {
     printf("\t; ");
     if (ISK(b)) PrintConstant(f,INDEXK(b)); else printf("-");
     printf(" ");
     if (ISK(c)) PrintConstant(f,INDEXK(c)); else printf("-");
    }
    break;
   case OP_JMP:
   case OP_FORLOOP:
   case OP_FORPREP:
    printf("\t; to %d",sbx+pc+2);
    break;
   case OP_CLOSURE:
    printf("\t; %p",VOID(f->p[bx]));
    break;
   case OP_SETLIST:
    if (c==0) printf("\t; %d",(int)code[++pc]);
    else printf("\t; %d",c);
    break;
   default:
    break;
  }
  printf("\n");
 }
printf("function Instructions end! \n");
}

//SS 打印单词 instruction 复数情况下后面要加 "s" 
#define SS(x)	(x<=1)?"":"s"
#define S(x)	x,SS(x)

static void PrintHeader(const Proto* f)
{
    const char* s=getstr(f->source);
    if (*s=='@' || *s=='=')
    {
        //s++; //文件名？
    }
    else if (*s==LUA_SIGNATURE[0])
    {
        s="(binarry string)no filename";
    } 
    else
    {
        s="(string)no filename";
    }

    printf("\n%s <%s:%d,%d> (%d instruction%s, %d bytes at %p)\n",
            (f->linedefined==0)?"main":"function", //当是入口文件时，显示方法名为 main,其他的都是 function  
            s,//文件名
            f->linedefined, //function 在文件的开始行 
            f->lastlinedefined, //end 在文件的结束行
            S(f->sizecode),//instruction 执行代码数量
            f->sizecode*Sizeof(Instruction), /* 代码占用空间大小 */ 
            VOID(f) );

    printf("%d%s param%s, %d slot%s(maxstacksize), %d upvalue%s, sizeupvalues[%d] \n",
        f->numparams, // 固定参数个数
        f->is_vararg ? "+" : "", //是否变参?
        SS(f->numparams),// 单词 param 是否复数
        S(f->maxstacksize), //slot
        S(f->nups), // upvalue 引用上层局部变量数量??  和 sizeupvalues,f->upvalues 关系?
        f->sizeupvalues);
    
    printf("%d local%s, %d constant%s, %d function%s\n",
            S(f->sizelocvars),//local 变量个数  
            S(f->sizek),  //定义的常量数量 
            S(f->sizep)); //方法数量
}

static void PrintConstants(const Proto* f)
{
 int i,n=f->sizek;
 printf("constants (%d) for %p:\n",n,VOID(f));
 for (i=0; i<n; i++)
 {
  printf("\t%d\t",i+1);
  PrintConstant(f,i);
  printf("\n");
 }
}

static void PrintLocals(const Proto* f)
{
 int i,n=f->sizelocvars;
 printf("locals (%d) for %p:\n",n,VOID(f));
 for (i=0; i<n; i++)
 {
  printf("\t%d\t%s\t%d\t%d\n",
  i,getstr(f->locvars[i].varname),f->locvars[i].startpc+1,f->locvars[i].endpc+1);
 }
}

static void PrintUpvalues(const Proto* f)
{
 int i,n=f->sizeupvalues;
 printf("upvalues (%d) for %p:\n",n,VOID(f));
 if (f->upvalues==NULL) return;
 for (i=0; i<n; i++)
 {
  printf("\t%d\t%s\n",i,getstr(f->upvalues[i]));
 }
}

void PrintFunction(const Proto* f, int full)
{
 int i,n=f->sizep;  /* size of `p' */
 PrintHeader(f);
 PrintCode(f);
 if (full)
 {
  PrintConstants(f);
  PrintLocals(f);
  PrintUpvalues(f);
 }
 for (i=0; i<n; i++) PrintFunction(f->p[i],full);
}
