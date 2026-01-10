/*
** $Id: lstate.h,v 2.24.1.2 2008/01/03 15:20:39 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"



struct lua_longjmp;  /* defined in ldo.c */


/* table of globals */
#define gt(L)	(&L->l_gt)

/* registry */
#define registry(L)	(&G(L)->l_registry)


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_CI_SIZE           8

#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)



typedef struct stringtable {
  GCObject **hash;
  lu_int32 nuse;  /* number of elements */
  int size;       // hash桶数组大小
} stringtable;


/*
** informations about a call
*/
typedef struct CallInfo {
  StkId base;  /* base for this function */
  StkId func;  /* function index in the stack */ //这个可用 ci_func 获得 Closure,找到对应的方法数据.    
  StkId	top;  /* top for this function */
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
} CallInfo;



#define curr_func(L)	(clvalue(L->ci->func))
#define ci_func(ci)	(clvalue((ci)->func))
#define f_isLua(ci)	(!ci_func(ci)->c.isC)
#define isLua(ci)	(ttisfunction((ci)->func) && f_isLua(ci))


/*
** `global state', shared by all threads of this state
*/
//typedef struct global_State {
//  stringtable strt;  /* hash table for strings 在 luaS_resize 初始化空间  */
//  lua_Alloc frealloc;  /* function to reallocate memory */
//  void *ud;         /* auxiliary data to `frealloc' */
//  lu_byte currentwhite;
//  lu_byte gcstate;  /* state of garbage collector */
//  int sweepstrgc;  /* position of sweep in `strt' */
//  GCObject *rootgc;  /* list of all collectable objects */
//  GCObject **sweepgc;  /* position of sweep in `rootgc' */
//  GCObject *gray;  /* list of gray objects */
//  GCObject *grayagain;  /* list of objects to be traversed atomically */
//  GCObject *weak;  /* list of weak tables (to be cleared) */
//  // 所有有GC方法的udata都放在tmudata链表中
//  GCObject *tmudata;  /* last element of list of userdata to be GC */
//  Mbuffer buff;  /* temporary buffer for string concatentation */
//  // 一个阈值，当这个totalbytes大于这个阈值时进行自动GC
//  lu_mem GCthreshold;
//  // 保存当前分配的总内存数量
//  lu_mem totalbytes;  /* number of bytes currently allocated */
//  // 一个估算值，根据这个计算GCthreshold
//  lu_mem estimate;  /* an estimate of number of bytes actually in use */
//  // 当前待GC的数据大小，其实就是累加totalbytes和GCthreshold的差值
//  lu_mem gcdept;  /* how much GC is `behind schedule' */
//  // 可以配置的一个值，不是计算出来的，根据这个计算GCthreshold，以此来控制下一次GC触发的时间
//  int gcpause;  /* size of pause between successive GCs */
//  // 每次进行GC操作回收的数据比例，见lgc.c/luaC_step函数
//  int gcstepmul;  /* GC `granularity' */
//  lua_CFunction panic;  /* to be called in unprotected errors */
//  TValue l_registry;
//  struct lua_State *mainthread;
//  UpVal uvhead;  /* head of double-linked list of all open upvalues */
//  struct Table *mt[NUM_TAGS];  /* metatables for basic types */
//  TString *tmname[TM_N];  /* array with tag-method names */
//} global_State;



typedef struct global_State {
	stringtable strt;  /* 字符串哈希表，在 luaS_resize 中初始化内存空间  */
	lua_Alloc frealloc;  /* 用于重新分配内存的函数（内存重分配回调） */
	void* ud;         /* 传递给 `frealloc` 函数的辅助数据 */
	lu_byte currentwhite;  /* 当前白标记（GC 标记阶段的当前白色标记位） */
	lu_byte gcstate;  /* 垃圾回收器的当前状态 */
	int sweepstrgc;  /* 在 `strt` 字符串表中进行清扫（sweep）的当前位置 */
	GCObject* rootgc;  /* 所有可回收对象的根链表（存放全部GC可管理对象） */
	GCObject** sweepgc;  /* 在 `rootgc` 链表中进行清扫（sweep）的当前位置 */
	GCObject* gray;  /* 灰色对象链表（待遍历标记的对象） */
	GCObject* grayagain;  /* 需被原子化遍历的对象链表（再次标记的灰色对象） */
	GCObject* weak;  /* 弱引用表链表（GC 阶段需要被清理的弱表） */
	// 所有有GC方法的udata都放在tmudata链表中
	GCObject* tmudata;  /* 待进行垃圾回收的用户数据（udata）链表的尾元素 */
	Mbuffer buff;  /* 用于字符串拼接的临时缓冲区 */
	// 一个阈值，当这个totalbytes大于这个阈值时进行自动GC
	lu_mem GCthreshold;  /* 垃圾回收触发阈值，当已分配内存超过该值时触发自动GC */
	// 保存当前分配的总内存数量
	lu_mem totalbytes;  /* 当前已分配的内存字节总数 */
	// 一个估算值，根据这个计算GCthreshold
	lu_mem estimate;  /* 实际正在使用的内存字节数的估算值（用于计算GC阈值） */
	// 当前待GC的数据大小，其实就是累加totalbytes和GCthreshold的差值
	lu_mem gcdept;  /* 垃圾回收「滞后进度」（待回收的内存大小，即totalbytes与GCthreshold的差值累加） */
	// 可以配置的一个值，不是计算出来的，根据这个计算GCthreshold，以此来控制下一次GC触发的时间
	int gcpause;  /* 连续两次垃圾回收之间的暂停间隔大小（可配置，用于计算下一次GC阈值） */
	// 每次进行GC操作回收的数据比例，见lgc.c/luaC_step函数
	int gcstepmul;  /* 垃圾回收的「粒度」（每次GC操作的内存回收比例，参考lgc.c/luaC_step函数） */
	lua_CFunction panic;  /* 未受保护的错误发生时调用的恐慌处理函数 */
	TValue l_registry;  /* Lua 注册表（全局核心注册表，存放全局共享数据） */
	struct lua_State* mainthread;  /* 主线程（Lua 虚拟机的主线程对象） */
	UpVal uvhead;  /* 所有开放上值（upvalue）的双向链表头节点 */
	struct Table* mt[NUM_TAGS];  /* 基本数据类型对应的元表数组（各基础类型的元表） */
	TString* tmname[TM_N];  /* 标签方法（tag-method）名称的数组 */
} global_State;


/*
** `per thread' state
*/
//struct lua_State {
//  CommonHeader;
//  lu_byte status;
//  StkId top;  /* first free slot in the stack */
//  StkId base;  /* base of current function */
//  global_State *l_G;
//  CallInfo *ci;  /* call info for current function */
//  const Instruction *savedpc;  /* `savedpc' of current function */
//  StkId stack_last;  /* last free slot in the stack */
//  StkId stack;  /* stack base */
//  CallInfo *end_ci;  /* points after end of ci array*/
//  CallInfo *base_ci;  /* array of CallInfo's */
//  int stacksize;
//  int size_ci;  /* size of array `base_ci' */
//  unsigned short nCcalls;  /* number of nested C calls */
//  unsigned short baseCcalls;  /* nested C calls when resuming coroutine */
//  lu_byte hookmask;
//  lu_byte allowhook;
//  int basehookcount;
//  int hookcount;
//  lua_Hook hook;
//  TValue l_gt;  /* table of globals */
//  TValue env;  /* temporary place for environments */
//  GCObject *openupval;  /* list of open upvalues in this stack */
//  GCObject *gclist;
//  struct lua_longjmp *errorJmp;  /* current error recover point */
//  ptrdiff_t errfunc;  /* current error handling function (stack index) */
//};

struct lua_State {
    CommonHeader;  /* 通用头信息（用于GC标记、对象类型标识，Lua中可回收对象的统一头部） */
    lu_byte status;  /* 虚拟机当前运行状态（标识是否有错误、是否处于挂起等状态） */
    StkId top;  /* 栈中第一个空闲槽位（栈顶指针，指向待入栈数据的下一个位置） */
    StkId base;  /* 当前函数的栈基地址（指向当前调用函数的参数起始位置） */
    global_State* l_G;  /* 指向全局状态机（关联当前线程所属的全局虚拟机环境） */
    CallInfo* ci;  /* 当前函数的调用信息（指向当前活跃的调用帧结构） */
    const Instruction* savedpc;  /* 当前函数的保存指令指针（记录当前执行到的字节码位置） */
    StkId stack_last;  /* 栈中最后一个空闲槽位（栈的上限，超出该位置需扩容栈空间） */
    StkId stack;  /* 栈的基地址（栈底指针，指向栈空间的起始位置） */
    CallInfo* end_ci;  /* 指向调用信息数组的末尾之后（标识CallInfo数组的内存上限） */
    CallInfo* base_ci;  /* 调用信息数组的起始地址（存放所有调用帧的数组首指针） */
    int stacksize;  /* 栈空间的总大小（栈中可容纳的槽位总数） */
    int size_ci;  /* 调用信息数组`base_ci`的大小（CallInfo数组可容纳的调用帧总数） */
    unsigned short nCcalls;  /* 嵌套C调用的层数（当前已嵌套的C函数调用深度） */
    unsigned short baseCcalls;  /* 恢复协程时的嵌套C调用层数（协程挂起时记录的C调用嵌套深度，恢复时复用） */
    lu_byte hookmask;  /* 钩子掩码（标识启用的钩子类型，如行钩子、调用钩子、返回钩子等） */
    lu_byte allowhook;  /* 钩子允许标记（标识当前是否允许触发钩子函数，用于避免钩子嵌套冲突） */
    int basehookcount;  /* 钩子计数基准值（钩子触发的计数初始值，用于步长钩子的计数重置） */
    int hookcount;  /* 当前钩子计数（步长钩子的剩余计数，计数归零时触发步长钩子） */
    lua_Hook hook;  /* 钩子函数指针（指向注册的自定义钩子处理函数，触发钩子时调用） */
    TValue l_gt;  /* 全局环境表（存放全局变量的核心表，对应Lua中的_G全局表） */
    TValue env;  /* 环境临时存储位置（用于临时存放函数、表等的环境对象，辅助环境查找） */
    GCObject* openupval;  /* 当前栈中的开放上值链表（存放当前线程栈中未闭合的上值，供闭包使用） */
    GCObject* gclist;  /* 待回收对象链表（当前线程中需要加入GC流程的可回收对象链表） */
    struct lua_longjmp* errorJmp;  /* 当前错误恢复点（指向长跳转结构，用于处理Lua中的错误捕获与恢复） */
    ptrdiff_t errfunc;  /* 当前错误处理函数（栈上的索引位置，指向用于处理错误的自定义函数） */
};

#define G(L)	(L->l_G)


/*
** Union of all collectable objects
*/
union GCObject {
  GCheader gch;
  union TString ts;
  union Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct UpVal uv;
  struct lua_State th;  /* thread */
};


/* macros to convert a GCObject into a specific value */
#define rawgco2ts(o)	check_exp((o)->gch.tt == LUA_TSTRING, &((o)->ts))
#define gco2ts(o)	(&rawgco2ts(o)->tsv)
#define rawgco2u(o)	check_exp((o)->gch.tt == LUA_TUSERDATA, &((o)->u))
#define gco2u(o)	(&rawgco2u(o)->uv)
#define gco2cl(o)	check_exp((o)->gch.tt == LUA_TFUNCTION, &((o)->cl))
#define gco2h(o)	check_exp((o)->gch.tt == LUA_TTABLE, &((o)->h))
#define gco2p(o)	check_exp((o)->gch.tt == LUA_TPROTO, &((o)->p))
#define gco2uv(o)	check_exp((o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define ngcotouv(o) \
	check_exp((o) == NULL || (o)->gch.tt == LUA_TUPVAL, &((o)->uv))
#define gco2th(o)	check_exp((o)->gch.tt == LUA_TTHREAD, &((o)->th))

/* macro to convert any Lua object into a GCObject */
#define obj2gco(v)	(cast(GCObject *, (v)))


LUAI_FUNC lua_State *luaE_newthread (lua_State *L);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);

#endif
