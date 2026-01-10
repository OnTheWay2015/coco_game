/*
** $Id: ldo.c,v 2.38.1.3 2008/01/18 22:31:22 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ldo_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"





/*
** {======================================================
** Error-recovery functions
** =======================================================
*/


/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;  /* error code */
};


void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, MEMERRMSG));
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    case LUA_ERRSYNTAX:
    case LUA_ERRRUN: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


static void restore_stack_limit (lua_State *L) {
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK - 1);
  if (L->size_ci > LUAI_MAXCALLS) {  /* there was an overflow? */
    int inuse = cast_int(L->ci - L->base_ci);
    if (inuse + 1 < LUAI_MAXCALLS)  /* can `undo' overflow? */
      luaD_reallocCI(L, LUAI_MAXCALLS);
  }
}


static void resetstack (lua_State *L, int status) {
  L->ci = L->base_ci;
  L->base = L->ci->base;
  luaF_close(L, L->base);  /* close eventual pending closures */
  luaD_seterrorobj(L, status, L->base);
  L->nCcalls = L->baseCcalls;
  L->allowhook = 1;
  restore_stack_limit(L);
  L->errfunc = 0;
  L->errorJmp = NULL;
}


void luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    LUAI_THROW(L, L->errorJmp);
  }
  else {
    L->status = cast_byte(errcode);
    if (G(L)->panic) {
      resetstack(L, errcode);
      lua_unlock(L);
      G(L)->panic(L);
    }
    exit(EXIT_FAILURE);
  }
}

// 任何需要保护jmp的调用,都要用这个函数保护
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  struct lua_longjmp lj;
  lj.status = 0;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  return lj.status; // .faq status 是在哪里修改的?  f()?   ==> luaD_throw() ?
}



//struct lua_longjmp {
//  struct lua_longjmp *previous;
//  luai_jmpbuf b;
//  volatile int status;  /* error code */
//};
//#define LUAI_TRY(L,c,a)	if (setjmp((c)->b) == 0) { a }
//
//
//setjmp 属于 C 函数库，作用是分别承担非局部标号和 goto 作用。
/*
 与 abort() 和 exit() 相比, goto 语句看起来是处理异常的更可行方案。不幸的是，
 goto 是本地的：它只能跳到所在函数内部的标号上，而不能将控制权转移到所在程序的任意地点。
 为了解决这个限制，C函数库提供了 setjmp() 和 longjmp() 函数，它们分别承担非局部标号和 goto 作用。
 头文件<setjmp.h>申明了这些函数及同时所需的 jmp_buf 数据类型。

 原理非常简单：
 1.setjmp(j) 设置 "jump" 点，用正确的程序上下文填充 jmp_buf 对象 j 。这个上下文包括程序存放位置、栈和框架指针，
    其它重要的寄存器和内存数据。 当初始化完 jump 的上下文，setjmp() 返回 0 值。
 2. 以后调用 longjmp(j,r) 的效果就是一个非局部的 goto 或 "长跳转"到由 j 描述的上下文处（也就是到那原来设置 j 的 setjmp() 处）。
 当作为长跳转的目标而被调用时，setjmp()返回 r 或 1 （如果 r 设为0的话）。（记住，setjmp()不能在这种情况时返回 0。）
 通过有两类返回值，setjmp()让你知道它正在被怎么使用。当设置 j 时，setjmp()如你期望地执行；
 但当作为长跳转的目标时，setjmp()就从外面 "唤醒" 它的上下文。你可以用 longjmp() 来终止异常，用 setjmp()标记相应的异常处理程序。
 
 * */


/* }====================================================== */


static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  GCObject *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->gch.next)
    gco2uv(up)->v = (gco2uv(up)->v - oldstack) + L->stack;
  for (ci = L->base_ci; ci <= L->ci; ci++) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->base = (ci->base - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
  }
  L->base = (L->base - oldstack) + L->stack;
}


void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int realsize = newsize + 1 + EXTRA_STACK;
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK - 1);
  luaM_reallocvector(L, L->stack, L->stacksize, realsize, TValue);
  L->stacksize = realsize;
  L->stack_last = L->stack+newsize;
  correctstack(L, oldstack);
}


void luaD_reallocCI (lua_State *L, int newsize) {
  CallInfo *oldci = L->base_ci;
  luaM_reallocvector(L, L->base_ci, L->size_ci, newsize, CallInfo);
  L->size_ci = newsize;
  L->ci = (L->ci - oldci) + L->base_ci;
  L->end_ci = L->base_ci + L->size_ci - 1;
}


void luaD_growstack (lua_State *L, int n) {
  if (n <= L->stacksize)  /* double size is enough? */
    luaD_reallocstack(L, 2*L->stacksize);
  else
    luaD_reallocstack(L, L->stacksize + n);
}

// 增长ci数组的size
static CallInfo *growCI (lua_State *L) {
  if (L->size_ci > LUAI_MAXCALLS)  /* overflow while handling overflow? */
	// 过大了
    luaD_throw(L, LUA_ERRERR);
  else {
	// 加大一倍
    luaD_reallocCI(L, 2*L->size_ci);
    if (L->size_ci > LUAI_MAXCALLS)
      luaG_runerror(L, "stack overflow");
  }
  return ++L->ci;
}


void luaD_callhook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, L->ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    if (event == LUA_HOOKTAILRET)
      ar.i_ci = 0;  /* tail call; no debug information about it */
    else
      ar.i_ci = cast_int(L->ci - L->base_ci);
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    L->ci->top = L->top + LUA_MINSTACK;
    lua_assert(L->ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    L->ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
  }
}

// 根据函数的参数数量调整base和top指针位置
static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  // numparams是函数的参数数量
  int nfixargs = p->numparams;
  Table *htab = NULL;
  StkId base, fixed;
  // 把没有赋值的函数参数值置nil
  for (; actual < nfixargs; ++actual)
    setnilvalue(L->top++);
#if defined(LUA_COMPAT_VARARG)
  if (p->is_vararg & VARARG_NEEDSARG) { /* compat. with old-style vararg? */
    int nvar = actual - nfixargs;  /* number of extra arguments */
    lua_assert(p->is_vararg & VARARG_HASARG);
    luaC_checkGC(L);
    htab = luaH_new(L, nvar, 1);  /* create `arg' table */
    for (i=0; i<nvar; i++)  /* put extra arguments into `arg' table */
      setobj2n(L, luaH_setnum(L, htab, i+1), L->top - nvar + i);
    /* store counter in field `n' */
    setnvalue(luaH_setstr(L, htab, luaS_newliteral(L, "n")), cast_num(nvar));
  }
#endif
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  // base指针指向最后一个函数参数的位置
  base = L->top;  /* final position of first argument */
  // OK, 逐个把函数传入的参数挪到局部变量处, 并且把原来传入的参数置nil
  for (i=0; i<nfixargs; i++) {
    setobjs2s(L, L->top++, fixed+i);
    setnilvalue(fixed+i);
  }
  /* add `arg' parameter */
  if (htab) {
    sethvalue(L, L->top++, htab);
    lua_assert(iswhite(obj2gco(htab)));
  }
  return base;
}


static StkId tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  ptrdiff_t funcr = savestack(L, func);
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at `func' */
  for (p = L->top; p > func; p--) setobjs2s(L, p, p-1);
  incr_top(L);
  func = restorestack(L, funcr);  /* previous call may change stack */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
  return func;
}



#define inc_ci(L) \
  ((L->ci == L->end_ci) ? growCI(L) : \
   (condhardstacktests(luaD_reallocCI(L, L->size_ci)), ++L->ci))

// 函数调用的预处理, func是函数closure所在位置, nresults是返回值数量
int luaD_precall (lua_State *L, StkId func, int nresults) {
  LClosure *cl;
  ptrdiff_t funcr;
  if (!ttisfunction(func)) /* `func' is not a function? */
    func = tryfuncTM(L, func);  /* check the `function' tag method */
  
  // 首先计算函数指针距离stack的偏移量
  funcr = savestack(L, func); 

  // 获取closure指针
  cl = &clvalue(func)->l;
  //保存PC,  当前方法的指令指针保存
  L->ci->savedpc = L->savedpc;
  if (!cl->isC) {  /* Lua function? prepare its call */
	//方法是脚本方法时
    CallInfo *ci;
    StkId st, base;
    Proto *p = cl->p;
    luaD_checkstack(L, p->maxstacksize);
    func = restorestack(L, funcr);// .faq 
    if (!p->is_vararg) {  /* no varargs? */
      base = func + 1;
      if (L->top > base + p->numparams)
        L->top = base + p->numparams;
    }
    else {  /* vararg function */
      int nargs = cast_int(L->top - func) - 1;
      base = adjust_varargs(L, p, nargs);
      func = restorestack(L, funcr);  /* previous call may change the stack */
    }
    // 存放新的函数信息
    // 首先从callinfo数组中分配出一个新的callinfo
    ci = inc_ci(L);  /* now `enter' new function */
    ci->func = func;
    L->base = ci->base = base;
    ci->top = L->base + p->maxstacksize;
    lua_assert(ci->top <= L->stack_last);
    // 改变代码执行的路径,  切换新方法的指令指针
    L->savedpc = p->code;  /* starting point */
    ci->tailcalls = 0;
    ci->nresults = nresults;
    for (st = L->top; st < ci->top; st++)
      setnilvalue(st);
    L->top = ci->top;
    if (L->hookmask & LUA_MASKCALL) {
      L->savedpc++;  /* hooks assume 'pc' is already incremented */
      luaD_callhook(L, LUA_HOOKCALL, -1);
      L->savedpc--;  /* correct 'pc' */
    }
    return PCRLUA;
  }
  else {  /* if is a C function, call it */
    CallInfo *ci;
    int n;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    // 从CallInfo数组中返回一个CallInfo指针
    ci = inc_ci(L);  /* now `enter' new function */
    // 根据之前保存的偏移量从栈中得到函数地址
    ci->func = restorestack(L, funcr);
    L->base = ci->base = ci->func + 1;
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    // 期待返回多少个返回值
    ci->nresults = nresults;
    if (L->hookmask & LUA_MASKCALL)
      luaD_callhook(L, LUA_HOOKCALL, -1);
    lua_unlock(L);
    // 调用C函数
    n = (*curr_func(L)->c.f)(L);  /* do the actual call */
    lua_lock(L);
    if (n < 0)  /* yielding? */
      return PCRYIELD;
    else {
      // 调用结束之后的处理
      luaD_poscall(L, L->top - n);
      return PCRC;
    }
  }
}


static StkId callrethooks (lua_State *L, StkId firstResult) {
  ptrdiff_t fr = savestack(L, firstResult);  /* next call may change stack */
  luaD_callhook(L, LUA_HOOKRET, -1);
  if (f_isLua(L->ci)) {  /* Lua function? */
    while ((L->hookmask & LUA_MASKRET) && L->ci->tailcalls--) /* tail calls */
      luaD_callhook(L, LUA_HOOKTAILRET, -1);
  }
  return restorestack(L, fr);
}

// 结束完一次函数调用(无论是C还是lua函数)的处理, firstResult是函数第一个返回值的地址
int luaD_poscall (lua_State *L, StkId firstResult) {
  StkId res;
  int wanted, i;
  CallInfo *ci;
  if (L->hookmask & LUA_MASKRET)
    firstResult = callrethooks(L, firstResult);
  // 得到当时的CallInfo指针
  ci = L->ci--;
  res = ci->func;  /* res == final position of 1st result */
  // 本来需要有多少返回值
  wanted = ci->nresults;
  // 把base和savepc指针置回调用前的位置
  L->base = (ci - 1)->base;  /* restore base */
  L->savedpc = (ci - 1)->savedpc;  /* restore savedpc */
  /* move results to correct place */
  // 返回值压入栈中
  for (i = wanted; i != 0 && firstResult < L->top; i--)
    setobjs2s(L, res++, firstResult++);
  // 剩余的返回值置nil
  while (i-- > 0)
    setnilvalue(res++);
  // 可以将top指针置回调用之前的位置了
  L->top = res;
  return (wanted - LUA_MULTRET);  /* 0 if wanted == LUA_MULTRET */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/ 
void luaD_call (lua_State *L, StkId func, int nResults) {
  // 函数调用栈数量+1, 判断函数调用栈是不是过长
  if (++L->nCcalls >= LUAI_MAXCCALLS) {
    if (L->nCcalls == LUAI_MAXCCALLS)
      luaG_runerror(L, "C stack overflow");
    else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
      luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
  }
  if (luaD_precall(L, func, nResults) == PCRLUA)  /* is a Lua function? */
    luaV_execute(L, 1);  /* call it */
  // 调用完毕, 函数调用栈-1
  L->nCcalls--;
  luaC_checkGC(L);
}


static void resume (lua_State *L, void *ud) {
  StkId firstArg = cast(StkId, ud);
  CallInfo *ci = L->ci;
  if (L->status == 0) {  /* start coroutine? */
	  // 协程第一次运行的情况
    lua_assert(ci == L->base_ci && firstArg > L->base);
    if (luaD_precall(L, firstArg - 1, LUA_MULTRET) != PCRLUA)
      return;
  }
  else {  /* resuming from previous yield */
	  // 从之前的状态中恢复
    lua_assert(L->status == LUA_YIELD);
    L->status = 0;
    if (!f_isLua(ci)) {  /* `common' yield? */
      /* finish interrupted execution of `OP_CALL' */
      lua_assert(GET_OPCODE(*((ci-1)->savedpc - 1)) == OP_CALL ||
                 GET_OPCODE(*((ci-1)->savedpc - 1)) == OP_TAILCALL);
      if (luaD_poscall(L, firstArg))  /* complete it... */
        L->top = L->ci->top;  /* and correct top if not multiple results */
    }
    else  /* yielded inside a hook: just continue its execution */
      L->base = L->ci->base;
  }
  luaV_execute(L, cast_int(L->ci - L->base_ci));
}


static int resume_error (lua_State *L, const char *msg) {
  L->top = L->ci->base;
  setsvalue2s(L, L->top, luaS_new(L, msg));
  incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


LUA_API int lua_resume (lua_State *L, int nargs) {
  int status;
  lua_lock(L);
  // 检查状态
  if (L->status != LUA_YIELD && (L->status != 0 || L->ci != L->base_ci))
      return resume_error(L, "cannot resume non-suspended coroutine");
  // 函数调用层次太多
  if (L->nCcalls >= LUAI_MAXCCALLS)
    return resume_error(L, "C stack overflow");
  luai_userstateresume(L, nargs);
  lua_assert(L->errfunc == 0);
  // 调用之前递增函数调用层次
  L->baseCcalls = ++L->nCcalls;
  // 以保护模式调用函数
  status = luaD_rawrunprotected(L, resume, L->top - nargs);
  if (status != 0) {  /* error? */
    L->status = cast_byte(status);  /* mark thread as `dead' */
    luaD_seterrorobj(L, status, L->top);
    L->ci->top = L->top;
  }
  else {
    lua_assert(L->nCcalls == L->baseCcalls);
    status = L->status;
  }
  // 减少调用层次
  --L->nCcalls;
  lua_unlock(L);
  return status;
}


LUA_API int lua_yield (lua_State *L, int nresults) {
  luai_userstateyield(L, nresults);
  lua_lock(L);
  if (L->nCcalls > L->baseCcalls)
    luaG_runerror(L, "attempt to yield across metamethod/C-call boundary");
  L->base = L->top - nresults;  /* protect stack slots below */
  L->status = LUA_YIELD;
  lua_unlock(L);
  return -1;
}


//status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
// 带错误保护的函数调用
//ptrdiff_t old_top  数值,记录上一个调用栈的栈顶位置
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  // 调用之前保存调用前的ci地址和top地址,用于可能发生的错误恢复
  int status;
  unsigned short oldnCcalls = L->nCcalls;
  ptrdiff_t old_ci = saveci(L, L->ci);
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;

  //执行方法
  status = luaD_rawrunprotected(L, func, u);
  // 如果status不为0,则表示有错误发生
  if (status != 0) {  /* an error occurred? */
	  // 将保存的ci和top取出来恢复
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close eventual pending closures */
    luaD_seterrorobj(L, status, oldtop);
    L->nCcalls = oldnCcalls;
    L->ci = restoreci(L, old_ci);
    L->base = L->ci->base;
    L->savedpc = L->ci->savedpc;
    L->allowhook = old_allowhooks;
    restore_stack_limit(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to `f_parser' */
  // 读入数据的缓冲区
  ZIO *z;
  // 缓存当前扫描数据的缓冲区
  Mbuffer buff;  /* buffer to be used by the scanner */
  // 源文件的文件名
  const char *name;
};

//把lua脚本文件解析成一个 Closuer 并放到栈顶
static void f_parser (lua_State *L, void *ud) {
  int i;
  Proto *tf; //把 二进制或文本脚本解析后获得的对象
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  // 预读入第一个字符
  int c = luaZ_lookahead(p->z);
  luaC_checkGC(L);
  // 根据之前预读的数据来决定下面的分析采用哪个函数, luaU_undump 或 luaY_parser 
  tf = ((c == LUA_SIGNATURE[0]) ? luaU_undump /*编译过的二进制*/ : luaY_parser /*原脚本文件*/)(L, p->z,
                                                             &p->buff, p->name); //p->name 在哪赋值? 在 luaD_protectedparser 
  // tf->nups 函数调用的upval数量
  cl = luaF_newLclosure(L, tf->nups, hvalue(gt(L)));
  cl->l.p = tf;  // cl->l,lua型函数调用, cl->l.p 型数原型指向 tf;
  for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
    cl->l.upvals[i] = luaF_newupval(L); //.faq 在哪赋值?使用？ 入口方法没有 upvalue? 全局数据怎么表示？
  setclvalue(L, L->top, cl); //把函数调用实例放到栈顶，等待参数压栈后调用 
  incr_top(L);
}


//luaL_loadfile ==>  lua_load  ==> luaD_protectedparser  ==> f_parser 
//static void f_parser (lua_State *L, void *ud) {
//Proto * luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff, const char *name) {
//     ==>[1] void luaX_next (LexState *ls) {
//            static int llex (LexState *ls, SemInfo *seminfo) //识别token 
//第一个字符确定 token
//下面结合 token 分词, 词法分析
//     ==>[2] static void chunk (LexState *ls) {
//            static int statement (LexState *ls)==>exprstat() ==> primaryexp()

int luaD_protectedparser (lua_State *L, ZIO *z, const char *name) {
  struct SParser p;
  int status;
  p.z = z; p.name = name;
  luaZ_initbuffer(L, &p.buff);
    // f_parser 会把文件解析成一个函数调用方法，并放到栈顶
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  
  luaZ_freebuffer(L, &p.buff);
  return status;
}

//任何高级一点的编译器，在解析源代码时，都需要进行词法分析。
//而词法分析的过程就是先识别token的一个过程，总体来说，lua里面的token大致分为：
//1. 数字和字符串
//2. 特殊字符：包括运算符和括号
//3. 关键词
//对于每一类token lua都有唯一的id与之对应，此id用int来表示，对于第2种类型，
//直接用该字符的ASCII码来表示，对于1,3两类，则定义一组枚举，为了与第2种区别开，
//枚举从257开始。比如关键字break 对应 TK_BREAK， do   对应 TK_DO。
//
//先来看看luaX_next，它用来识别下一个 token，会调用 llex 函数，返回 token type和 seminfo。
//有了token，接下来就会分析一条条的语句。

//一个 statementlist 的 production 为： statlist -> { stat [`;'] } ，下面先将一个 statement 的grammer production列出:
//
//stat = { ifstat | dostat | whilestat | functionstat | localstat | retstat | forstat | repeatestat | goto | breakstat  | exprstat }
//
//以 ifstat 为例：
//
//ifstat -> IF exprstat THEN statlist END
//
//exprstat -> subexpr
//
//subexpr ->(simpleexp | unop subexpr) { binop subexpr }
//
//simpleexp -> NUMBER | STRING | NIL | TRUE | FALSE | ... | constructor | FUNCTION body | suffixedexp 



//关键词以大写表示，在读完 IF token之后，接着读 exprstat ， exprstat 继续向下分解为 subexpr， 接着是 simpleexp ，
//直至基础数据 NUMBER 为止。 这实际上就是所谓的自顶向下分析法。 
//
//说到这里，似乎词法分析很简单，单论词法分析，lua确实很简单，但lua是解释型的脚本语言，
//实际在是一边做词法分析，一边再做语义分析，同时根据语义分析结果，生成lua指令。
//也就是说，在词法分析的同时，干了很多事情。 

//具体来看看lua指令，一个lua指令用一个32位的整数来表示。其中6bit用于表示指令码，
//8bit表示操作数A，9bit表示B，9bit表示C（这种做法在逻辑上有点类似汇编）,操作数一般来说分几种类型:
//
//1,R(x): 表示该操作数在寄存器中
//2,Kst(x):表示该操作数在常量表中
//3,RK(x):表示该操作数如果是常量的话，则表示2，否则表示1
//举例：指令OP_MOVE，看代码注释为R(A) := R(B)，这就表示将寄存器B的值赋值给寄存器A。
//到这里不禁要问， lua寄存器是个什么概念？ 我们知道，对汇编来说，具体的寄存器是使用具体寄存器名字来指定的
//例如mov ax, bx ...，而lua则由下标来指向寄存器。实质上R(x) = [base+x] ， 其中base表示当前函数调用的一个基地址。
//lua解析是以function为单位来进行的，加载一个文件，可以看作是在一个全局的大函数里面对文件进行解析。
//那么，最终生成的指令都会放在归属函数Proto的opcodes数组里面。执行一个函数的时候，
//VM直接取指令分析操作数，接着往下执行就可以了。



