/*
** $Id: lstate.c,v 2.36.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lstate_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#define state_size(x)	(sizeof(x) + LUAI_EXTRASPACE)
#define fromstate(l)	(cast(lu_byte *, (l)) - LUAI_EXTRASPACE)
#define tostate(l)   (cast(lua_State *, cast(lu_byte *, l) + LUAI_EXTRASPACE))


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  lua_State l;
  global_State g;
} LG;
  


static void stack_init (lua_State *L1, lua_State *L) {
  /* initialize CallInfo array */
  // 创建CallInfo数组
  L1->base_ci = luaM_newvector(L, BASIC_CI_SIZE, CallInfo);
  L1->ci = L1->base_ci;
  L1->size_ci = BASIC_CI_SIZE;
  L1->end_ci = L1->base_ci + L1->size_ci - 1;
  /* initialize stack array */
  // 创建 TValue 数组
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, TValue);
  L1->stacksize = BASIC_STACK_SIZE + EXTRA_STACK;
  L1->top = L1->stack;
  L1->stack_last = L1->stack+(L1->stacksize - EXTRA_STACK)-1;
  /* initialize first ci */
  L1->ci->func = L1->top;
  // 这里的作用是把当前top所在区域的值set成nil,然后top++
  // 在top++之前,从上一句代码可以看出,top指向的位置是L1->ci->func,也就是把L1->ci->func set为nil
  setnilvalue(L1->top++);  /* `function' entry for this `ci' */
  // 执行这句调用之后, base = top = stack + 1, 但是base是存放什么值的呢??
  L1->base = L1->ci->base = L1->top;
  // 这里的意思是,每个lua函数最开始预留LUA_MINSTACK个栈位置,不够的时候再增加,见 luaD_checkstack 函数
  L1->ci->top = L1->top + LUA_MINSTACK;
}


static void freestack (lua_State *L, lua_State *L1) {
  luaM_freearray(L, L1->base_ci, L1->size_ci, CallInfo);
  luaM_freearray(L, L1->stack, L1->stacksize, TValue);
}


/*
** open parts that may cause memory-allocation errors
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  // 初始化堆栈
  // 创建 ci(CallInfo 数组), stack(TValue 数组)  (代码段, 栈内存 ?) 
  stack_init(L, L);  /* init stack */
  
  // 初始化全局表 (堆内存 ?)  
  // 没有放在 stack 栈里,而是 l_gt 
  // #define gt(L)	(&L->l_gt)
  sethvalue(L, gt(L), luaH_new(L, 0, 2));  /* table of globals */
  
  // 初始化寄存器
  sethvalue(L, registry(L), luaH_new(L, 0, 2));  /* registry */
  
  luaS_resize(L, MINSTRTABSIZE);  /* initial size of string table */
 
  // 初始化 meta method
  luaT_init(L);
  
  // 初始化 lua 关键字字符串,注意到并不保存它们,只是在 StringTable 中保存下来并且标记为不可回收
  luaX_init(L);

  // 初始化 not enough memory 这个字符串并且标记为不可回收
  luaS_fix(luaS_newliteral(L, MEMERRMSG));
  g->GCthreshold = 4*g->totalbytes;
}


static void preinit_state (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->stacksize = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->size_ci = 0;
  L->nCcalls = L->baseCcalls = 0;
  L->status = 0;
  L->base_ci = L->ci = NULL;
  L->savedpc = NULL;
  L->errfunc = 0;
  setnilvalue(gt(L));
}


static void close_state (lua_State *L) {
  global_State *g = G(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_freeall(L);  /* collect all objects */
  lua_assert(g->rootgc == obj2gco(L));
  lua_assert(g->strt.nuse == 0);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size, TString *);
  luaZ_freebuffer(L, &g->buff);
  freestack(L, L);
  lua_assert(g->totalbytes == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), state_size(LG), 0);
}


lua_State *luaE_newthread (lua_State *L) {
  lua_State *L1 = tostate(luaM_malloc(L, state_size(lua_State)));
  luaC_link(L, obj2gco(L1), LUA_TTHREAD);
  preinit_state(L1, G(L));
  stack_init(L1, L);  /* init stack */
  setobj2n(L, gt(L1), gt(L));  /* share table of globals */
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  lua_assert(iswhite(obj2gco(L1)));
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L1);
  freestack(L, L1);
  luaM_freemem(L, fromstate(L1), state_size(lua_State));
}


//LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
//  int i;
//  lua_State *L;
//  global_State *g;
//  void *l = (*f)(ud, NULL, 0, state_size(LG));
//  if (l == NULL) return NULL;
//  L = tostate(l);
//  g = &((LG *)L)->g;
//  L->next = NULL;
//  L->tt = LUA_TTHREAD;
//  g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
//  L->marked = luaC_white(g);
//  set2bits(L->marked, FIXEDBIT, SFIXEDBIT);
//  
//  preinit_state(L, g);
//  
//  g->frealloc = f;
//  g->ud = ud;
//  g->mainthread = L;
//  g->uvhead.u.l.prev = &g->uvhead;
//  g->uvhead.u.l.next = &g->uvhead;
//  g->GCthreshold = 0;  /* mark it as unfinished state */
//  g->strt.size = 0;
//  g->strt.nuse = 0;
//  g->strt.hash = NULL;
//  setnilvalue(registry(L));
//  luaZ_initbuffer(L, &g->buff);
//  g->panic = NULL;
//  g->gcstate = GCSpause;
//  g->rootgc = obj2gco(L);
//  g->sweepstrgc = 0;
//  g->sweepgc = &g->rootgc;
//  g->gray = NULL;
//  g->grayagain = NULL;
//  g->weak = NULL;
//  g->tmudata = NULL;
//  g->totalbytes = sizeof(LG);
//  g->gcpause = LUAI_GCPAUSE;
//  g->gcstepmul = LUAI_GCMUL;
//  g->gcdept = 0;
//  for (i=0; i<NUM_TAGS; i++) g->mt[i] = NULL;
//  
//  //f_luaopen 初始化
//  if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
//    /* memory allocation error: free partial state */
//    close_state(L);
//    L = NULL;
//  }
//  else
//    luai_userstateopen(L);
//  return L;
//}

/**
 * @brief  创建一个新的 Lua 虚拟机状态（创建新的 Lua 线程/环境）
 * @param  f  内存分配/重分配/释放的回调函数，供 Lua 虚拟机管理内存
 * @param  ud  传递给内存回调函数 `f` 的辅助用户数据
 * @return  成功返回新创建的 lua_State 指针，失败返回 NULL
 */
LUA_API lua_State* lua_newstate(lua_Alloc f, void* ud) {
    int i;
    lua_State* L;          // 指向新创建的 Lua 线程状态
    global_State* g;       // 指向全局虚拟机状态（关联在 Lua 线程中）
    // 1. 调用内存分配函数，申请足够的内存存放 lua_State 和 global_State（合为 LG 结构体）
    // state_size(LG) 计算 LG 结构体的总内存大小，参数 NULL/0 表示新分配内存
    void* l = (*f)(ud, NULL, 0, state_size(LG));
    // 内存分配失败，直接返回 NULL
    if (l == NULL) return NULL;

    // 2. 将分配到的内存转换为 lua_State 类型指针（LG 结构体首地址即为 lua_State 地址）
    L = tostate(l);
    // 3. 获取 LG 结构体中的 global_State 成员（全局状态机）
    g = &((LG*)L)->g;

    // 4. 初始化 Lua 线程（lua_State）的基础属性
    L->next = NULL;                // 线程链表下一个节点置空（当前线程暂未加入链表）
    L->tt = LUA_TTHREAD;           // 标记当前对象类型为 Lua 线程（LUA_TTHREAD）
    // 初始化全局状态机的当前白标记（结合 WHITE0BIT 和 FIXEDBIT，用于 GC 标记）
    g->currentwhite = bit2mask(WHITE0BIT, FIXEDBIT);
    // 为当前线程设置 GC 白色标记（标记为未被回收的活跃对象）
    L->marked = luaC_white(g);
    // 设置当前线程的 FIXEDBIT 和 SFIXEDBIT 标记（标记为固定对象，不参与 GC 清扫）
    set2bits(L->marked, FIXEDBIT, SFIXEDBIT);

    // 5. 预初始化 Lua 线程和全局状态机的核心字段（栈、调用信息等基础结构）
    preinit_state(L, g);

    // 6. 初始化全局状态机（global_State）的内存管理相关字段
    g->frealloc = f;               // 保存内存回调函数，供后续虚拟机内存操作使用
    g->ud = ud;                    // 保存内存回调函数的辅助用户数据
    g->mainthread = L;             // 记录虚拟机的主线程（当前创建的线程即为主线程）

    // 7. 初始化全局开放上值（upvalue）的双向链表头节点（形成闭环空链表）
    g->uvhead.u.l.prev = &g->uvhead;
    g->uvhead.u.l.next = &g->uvhead;

    // 8. 初始化垃圾回收（GC）相关的核心参数
    g->GCthreshold = 0;            // 设置 GC 触发阈值为 0（标记当前状态机尚未初始化完成）
    g->strt.size = 0;              // 字符串哈希表大小初始化为 0
    g->strt.nuse = 0;              // 字符串哈希表中已使用的槽位初始化为 0
    g->strt.hash = NULL;           // 字符串哈希表的哈希数组指针置空（后续动态分配）
    setnilvalue(registry(L));      // 将 Lua 注册表初始化为 nil（后续会初始化为表结构）

    // 9. 初始化字符串拼接的临时缓冲区
    luaZ_initbuffer(L, &g->buff);

    // 10. 初始化 GC 相关的状态和链表（初始处于暂停状态）
    g->panic = NULL;               // 恐慌处理函数置空（后续可由用户设置）
    g->gcstate = GCSpause;         // GC 状态设置为暂停（GCSpause）
    g->rootgc = obj2gco(L);        // 将主线程加入 GC 根对象链表（作为第一个可回收对象）
    g->sweepstrgc = 0;             // 字符串表清扫位置初始化为 0（GC 清扫阶段的起始位置）
    g->sweepgc = &g->rootgc;       // GC 根链表清扫指针初始化为根对象链表首地址
    g->gray = NULL;                // 灰色对象链表置空（暂无待标记对象）
    g->grayagain = NULL;           // 需再次标记的灰色对象链表置空
    g->weak = NULL;                // 弱引用表链表置空（暂无待清理的弱表）
    g->tmudata = NULL;             // 待 GC 的用户数据链表置空

    // 11. 初始化内存统计相关参数
    g->totalbytes = sizeof(LG);    // 当前已分配总内存初始化为 LG 结构体的大小
    g->gcpause = LUAI_GCPAUSE;     // 设置 GC 暂停间隔（默认值，控制两次 GC 的间隔）
    g->gcstepmul = LUAI_GCMUL;     // 设置 GC 步长乘数（默认值，控制 GC 回收粒度）
    g->gcdept = 0;                 // GC 滞后进度初始化为 0（暂无待回收的滞后内存）

    // 12. 初始化所有基本数据类型的元表（全部置空，后续按需创建和赋值）
    for (i = 0; i < NUM_TAGS; i++) g->mt[i] = NULL;

    // 13. 以受保护模式运行 Lua 核心初始化函数（f_luaopen），避免初始化过程中出错崩溃
    // f_luaopen 负责初始化 Lua 标准库、注册表、全局环境等核心结构
    if (luaD_rawrunprotected(L, f_luaopen, NULL) != 0) {
        /* 内存分配错误：释放已部分分配的状态机内存 */
        close_state(L);  // 清理已初始化的资源并释放内存
        L = NULL;        // 线程指针置空，标识创建失败
    }
    else
        // 14. 初始化用户自定义状态（留给用户扩展的初始化接口，默认空实现）
        luai_userstateopen(L);

    // 15. 返回创建成功的 Lua 线程状态指针（失败则返回 NULL）
    return L;
}


static void callallgcTM (lua_State *L, void *ud) {
  UNUSED(ud);
  luaC_callGCTM(L);  /* call GC metamethods for all udata */
}


LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  luaF_close(L, L->stack);  /* close all upvalues for this thread */
  luaC_separateudata(L, 1);  /* separate udata that have GC metamethods */
  L->errfunc = 0;  /* no error function during GC metamethods */
  do {  /* repeat until no more errors */
    L->ci = L->base_ci;
    L->base = L->top = L->ci->base;
    L->nCcalls = L->baseCcalls = 0;
  } while (luaD_rawrunprotected(L, callallgcTM, NULL) != 0);
  lua_assert(G(L)->tmudata == NULL);
  luai_userstateclose(L);
  close_state(L);
}

