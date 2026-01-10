/*************************************************************************
	> File Name: /data/coovanftp/ftp/server_srv/whserver/nginx_lua/3rd/lua-5_1_4/test_do.c
	> Author: blue
	> Mail:   
	> Created Time: Sun 23 Dec 2018 06:30:32 PM CST
 ************************************************************************/

#include <stdio.h>
#include "test_do.h"


void p_lua_value(TValue * curr, TValue * st)
{

    
    if ( ttisnil(curr) ) 
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,nil]\n",curr, (curr-st), curr->value, curr->tt );        
    }
    else if (ttisnumber(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,number] v[%lf]\n",curr, (curr-st), curr->value, curr->tt, curr->value.n );        
    }
    else if (ttisstring(curr))
    {
        TString *ts = rawgco2ts(curr->value.gc);
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,string] v[%s]\n", curr,(curr-st), curr->value, curr->tt, getstr(ts));        
    }
    else if (ttistable(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,table]\n", curr,(curr-st), curr->value, curr->tt );        
    }
    else if (ttisfunction(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,function]\n",curr, (curr-st), curr->value, curr->tt );        

        //Value v = curr->value;
        //GCObject *gc = v.gc;
        //Closure cl = gc->cl;
        //LClosure ci = cl.l;
        
        //LClosure ci = curr->value.gc->cl.l;
         //GCObject *gc = curr->value.gc;//->cl.l;
         //Closure cl= curr->value.gc->cl;//.l;
        LClosure ci = curr->value.gc->cl.l;

        if (!ci.isC) /* Lua function? prepare its call */
        {
            printf("             lua function!\n");
            p_func_proto( ci.p );
        }
        else
        {
            printf("             c function!\n");
        }

        //typedef struct LClosure {
        //  ClosureHeader;
        //  struct Proto *p;
        //  UpVal *upvals[1];  // .faq ??????
        //} LClosure;
    }
    else if (ttisboolean(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,bool]\n",curr, (curr-st), curr->value, curr->tt );        
    }
    else if (ttisuserdata(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,userdata]\n",curr, (curr-st), curr->value, curr->tt );        
    }
    else if (ttisthread(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,thread]\n", curr,(curr-st), curr->value, curr->tt );        
    }
    else if (ttislightuserdata(curr))
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] type[%d,lightuserdata]\n",curr, (curr-st), curr->value, curr->tt );        
    }
    else 
    {
        printf("      stack stkid[0x%p] idx[%d], value[0x%p] none type[%d]\n",curr,(curr-st), curr->value, curr->tt );        
    }
}
void p_lua_stack(lua_State*L, StkId st, StkId ed)
{
    if ( st < L->stack )
    {
        printf("p_stack st < L->stack err!\n");
        return;
    }
    if (st > L->stack_last)
    {
        printf("p_stack st > L->stack_last err!\n");
        return;
    }
    if ( ed < L->stack )
    {
        printf("p_stack ed < L->stack err!\n");
        return;
    }
    if (ed > L->stack_last)
    {
        printf("p_stack ed > L->stack_last err!\n");
        return;
    }

    if (st > ed)
    {
        printf("p_stack st > ed err!\n");
        return;
    }

    StkId curr = st;
    while (curr <= ed )
    {
        p_lua_value(curr, st);
        curr++;
    }
}

extern void PrintFunction(const Proto* f, int full);
void p_func_proto(Proto *p)
{
    printf(" Proto: --------------------\n");
    PrintFunction(p,1);
}



///*
// ** Union of all collectable objects
// */
//union GCObject {
//   GCheader gch;
//   union TString ts;
//   union Udata u;
//   union Closure cl;
//   struct Table h;
//   struct Proto p;
//   struct UpVal uv;
//   struct lua_State th;  /* thread */
// };





//#define ClosureHeader \
//	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
//	struct Table *env
//
//typedef struct CClosure {
//  ClosureHeader;
//  lua_CFunction f;
//  TValue upvalue[1];
//} CClosure;
//
//
//typedef struct LClosure {
//  ClosureHeader;
//  struct Proto *p;
//  UpVal *upvals[1]; //.faq ????
//} LClosure;
//
//
//typedef union Closure {
//  CClosure c;
//  LClosure l;
//} Closure;


//typedef struct Proto {
//  CommonHeader;
//  TValue *k;  /* constants used by the function */ // 使用的常量
//  Instruction *code;// 存放函数体的opcode
//  struct Proto **p;  /* functions defined inside the function */
//  int *lineinfo;  /* map from opcodes to source lines */
//  struct LocVar *locvars;  /* information about local variables */// 存放局部变量的数组
//  TString **upvalues;  /* upvalue names */
//  TString  *source;
//  int sizeupvalues;
//  int sizek;  /* size of `k' */
//  int sizecode;
//  int sizelineinfo;
//  int sizep;  /* size of `p' */
//  int sizelocvars;
//  int linedefined;
//  int lastlinedefined;
//  GCObject *gclist;
//  lu_byte nups;  /* number of upvalues */
//  lu_byte numparams;
//  lu_byte is_vararg;
//  lu_byte maxstacksize;
//} Proto;



void p_callinfo(char* str, CallInfo *ci)
{
    printf("%s top[0x%p] \n",str, ci->top);
    printf("%s base[0x%p] \n",str, ci->base);
    printf("%s func[0x%p] \n",str, ci->func);
    printf("%s Instruction[0x%p] \n",str, ci->savedpc);
    printf("%s nresults[%d] \n",str, ci->nresults);
    printf("%s tailcalls[%d] \n",str, ci->tailcalls);
}

int print_stack(lua_State *L)
{
    //p_lua_stack(L, L->stack,L->stack_last );
    p_lua_state(L);
    return 0;
}

void p_lua_state(lua_State *L)
{
    printf("commonHeader ---------- : \n");
    //#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked
    printf("L CommonHeader next[0x%p]", L->next);
    printf("L CommonHeader tt[%d]", L->tt);
    printf("L CommonHeader market[%d]", L->marked);
    //------------------------------- 
    printf("globalstate---------- : \n");
    printf("....\n");

    //------------------------------- 
    printf("L status [%d] \n",L->status);
    printf("L stkid top[0x%p] \n", L->top);/* first free slot in the stack */
    printf("L stkid base [0x%p]\n", L->base); /* base of current function */
    printf("L stkid stack[0x%p]\n", L->stack); /* stack base */
    printf("L stkid stack_last[0x%p] stack_last-stack[%d]\n", L->stack_last, L->stack_last-L->stack); /* last free slot in the stack */
    printf("L curr used stack(L->top- L->base) len[%d]\n",(L->top- L->base));
    printf("L total used stack(L->top - L->stack) len[%d]\n",(L->top - L->stack));
    printf("L stack size[%d]\n",(L->stacksize));

    p_lua_stack(L, L->stack,L->stack_last );

    if (L->ci) /* call info for current function */
    {
        printf("--------- callinfo ci:");
        p_callinfo("L ci ", L->ci);
    }
    else
    {
        printf("L->ci is null!");
    }

    if (L->base_ci)  /* array of CallInfo's */
    {
        printf("--------- callinfo base_ci:");
        p_callinfo("L base_ci ", L->base_ci);
    }
    else
    {
        printf("L->base_ci is null!");
    }

    if (L->end_ci)  /* points after end of ci array*/
    {
        printf("--------- callinfo end_ci:");
        p_callinfo("L end_ci ", L->end_ci);
    }
    else
    {
        printf("L->end_ci is null!");
    }

    printf("L size_ci[%d]\n",(L->size_ci)); /* size of array `base_ci' */

    printf("L Instruction[0x%p] \n", L->savedpc);/* `savedpc' of current function */

    printf("L nCcalls[%d] \n", L->nCcalls); /* number of nested C calls */ //nested 嵌套的
    printf("L baseCcalls[%d] \n", L->baseCcalls);  /* nested C calls when resuming coroutine */

    printf("L hookmask[%d] \n", L->hookmask); 
    printf("L allowhook[%d] \n", L->allowhook); 
    printf("L basehookcount[%d] \n", L->basehookcount); 
    printf("L hookcount[%d] \n", L->hookcount); 
    printf("L hook[0x0x%p] \n", L->hook); 

    printf("L tvalue l_gt ...\n");/* table of globals */ 
    printf("L tvalue env ...\n"); /* temporary place for environments */
    printf("L openupval[0x%p] \n", L->openupval);  /* list of open upvalues in this stack */
    printf("L gclist[0x%p] \n", L->gclist); 
    printf("L errorJmp[0x%p] \n", L->errorJmp); /* current error recover point */
    printf("L errfunc[%ld] \n", L->errfunc);  /* current error handling function (stack index) */
    //ptrdiff_t是C/C++标准库中定义的一个与机器相关的数据类型。ptrdiff_t类型变量通常用来保存两个指针减法操作的结果。
    //ptrdiff_t定义在stddef.h（cstddef）这个文件内。ptrdiff_t通常被定义为long int类型。    

	stackDump(L,0);
}

//extern TValue *index2adr (lua_State *L, int idx);
void DumpStkId(lua_State* L,int i, int isbase)
{
    StkId o;
    if (!isbase)
    {
        //o = index2adr(L, i); 
        o = L->base + (i- 1);//当前栈
    }
    else
    {
        o = L->stack + (i- 1); //全局栈
    }
    int t = (o == luaO_nilobject) ? LUA_TNONE : ttype(o);
    //int t = lua_type(L, i);
    printf("idx[%d], ", i);
    switch (t) {
    case LUA_TSTRING:
    {
        printf("LUA_TSTRING: %s \n", svalue(o));
    }
    break;
    case LUA_TBOOLEAN:
    {
        printf("LUA_TBOOLEAN:");
        printf(bvalue(o) ? " true \n" : "false \n");
    }break;
    case LUA_TNUMBER:
    {
        printf("LUA_TNUMBER: %g \n", nvalue(o));
    }
    break;

    case LUA_TNONE:
    {
        printf("LUA_TNONE:\n");
    }
    break;
    case LUA_TNIL:
    {
        printf("LUA_TNIL:\n");
    }
    break;
    case LUA_TTABLE:
    {
        printf("LUA_TTABLE:\n");
    }
    break;
    case LUA_TLIGHTUSERDATA	:
    {
        printf("LUA_TLIGHTUSERDATA:\n");
    }
    break;
    case LUA_TFUNCTION:
    {
        printf("LUA_TFUNCTION:");
        Closure* cl = clvalue(o);
        if (iscfunction(o))
        {
            printf(" [isC] ");
        }
        else
        {
            printf(" [notC] ");
        }
        
        printf("\n");
        
        
//#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
//#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)

 
    }
    break;
    case LUA_TUSERDATA:
    {
        printf("LUA_TUSERDATA:\n");
    }
    break;
    case LUA_TTHREAD:
    {
        printf("LUA_TTHREAD:\n");
    }
    break;
    default:
    {
        printf("type:%d,  error, need to do more.  \n", t);
    }
    break;
    }

}

void stackDump(lua_State* L, int isGlobal)
{
    int i = 0; 
    //int top = isGlobal==0 ? lua_gettop(L) :(L->stack_last - L->stack);
    int top = isGlobal==0 ? lua_gettop(L) :(L->top - L->stack);

    printf("\n\n");
    printf(" begin dump lua isGlobal[%d] stack len[%d]\n",isGlobal,top);
    for (i = 1; i <= top; ++i) {
        DumpStkId(L,i,isGlobal); 
    }
    printf(" end dump lua stack\n");
}





