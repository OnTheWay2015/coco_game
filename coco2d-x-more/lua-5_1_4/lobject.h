/*
** $Id: lobject.h,v 2.20.1.2 2008/08/06 13:29:48 roberto Exp $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define LAST_TAG	LUA_TTHREAD

#define NUM_TAGS	(LAST_TAG+1)


/*
** Extra tags for non-values
*/
#define LUA_TPROTO	(LAST_TAG+1)
#define LUA_TUPVAL	(LAST_TAG+2)
#define LUA_TDEADKEY	(LAST_TAG+3)


/*
** Union of all collectable objects
*/
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked
/*
CommonHeader 是给所有可回收对象用的，可以被包含在其它对象中。 
可以被垃圾回收的对象(string, userdata, function, thread, table) 的结构体声明里，第一行都是 CommonHeader。
    @next  说明可回收对象是可以放到链表里 
    @marked  是 GC 用来进行标记的 
    @tt 数据类型. TValue 里不是已经有一个 tt_ 字段用于表示类型了吗？为什么在 GCObject 里还需要这个字段呢？
              //要从 GCObject 反向得到 TValue 是不行的，假如 GCObject 没有 tt 字段，单单持有 GCObject 的时候，没法判断这个 GCObject 的类型是什么。
                      //GC 在回收对象的时候需要根据类型来释放资源。基于第一点，必须在 GCObject 里加一个表示类型的字段 tt。
 * */

/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;




/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc; //需要管理 gc 的lua类型 table 等
  void *p;
  lua_Number n; //lua_Number 为 double 类型
  int b; //整型
} Value;


/*
** Tagged Values
*/

#define TValuefields	Value value; int tt

typedef struct lua_TValue {
  TValuefields;
} TValue;


/* Macros to test type */
#define ttisnil(o)	(ttype(o) == LUA_TNIL)
#define ttisnumber(o)	(ttype(o) == LUA_TNUMBER)
#define ttisstring(o)	(ttype(o) == LUA_TSTRING)
#define ttistable(o)	(ttype(o) == LUA_TTABLE)
#define ttisfunction(o)	(ttype(o) == LUA_TFUNCTION)
#define ttisboolean(o)	(ttype(o) == LUA_TBOOLEAN)
#define ttisuserdata(o)	(ttype(o) == LUA_TUSERDATA)
#define ttisthread(o)	(ttype(o) == LUA_TTHREAD)
#define ttislightuserdata(o)	(ttype(o) == LUA_TLIGHTUSERDATA)

/* Macros to access values */
#define ttype(o)	((o)->tt)
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value.gc)
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value.p)
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value.n)
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value.gc->ts)
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value.gc->u)
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisfunction(o), &(o)->value.gc->cl)
#define hvalue(o)	check_exp(ttistable(o), &(o)->value.gc->h)
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value.b)
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value.gc->th)

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))

/*
** for internal debug only
*/
#define checkconsistency(obj) \
  lua_assert(!iscollectable(obj) || (ttype(obj) == (obj)->value.gc->gch.tt))

#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || \
  ((ttype(obj) == (obj)->value.gc->gch.tt) && !isdead(g, (obj)->value.gc)))


/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setnvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.n=(x); i_o->tt=LUA_TNUMBER; }

#define setpvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }

#define setbvalue(obj,x) \
  { TValue *i_o=(obj); i_o->value.b=(x); i_o->tt=LUA_TBOOLEAN; }

#define setsvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TSTRING; \
    checkliveness(G(L),i_o); }

#define setuvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TUSERDATA; \
    checkliveness(G(L),i_o); }

#define setthvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTHREAD; \
    checkliveness(G(L),i_o); }

#define setclvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TFUNCTION; \
    checkliveness(G(L),i_o); }

#define sethvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TTABLE; \
    checkliveness(G(L),i_o); }

#define setptvalue(L,obj,x) \
  { TValue *i_o=(obj); \
    i_o->value.gc=cast(GCObject *, (x)); i_o->tt=LUA_TPROTO; \
    checkliveness(G(L),i_o); }




#define setobj(L,obj1,obj2) \
  { const TValue *o2=(obj2); TValue *o1=(obj1); \
    o1->value = o2->value; o1->tt=o2->tt; \
    checkliveness(G(L),o1); }


/*
** different types of sets, according to destination
        把数据放入栈里，操作过程一样，只是设置的类型不同  i_o->tt
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue

#define setttype(obj, tt) (ttype(obj) = (tt))

// 只有这些类型的数据 才是可回收的数据
#define iscollectable(o)	(ttype(o) >= LUA_TSTRING)



typedef TValue *StkId;  /* index to stack elements */


/*
** String headers for string table
*/
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte reserved;
    unsigned int hash;//在字符表里的 hash?
    size_t len;
  } tsv;
} TString;

//字符串实体保存在 ts(TString)对像之后,如  (char*)((TString*)0x0000024326f48b00+1)
#define getstr(ts)	cast(const char *, (ts) + 1)
#define svalue(o)       getstr(rawtsvalue(o))


// 这里为什么需要使用union类型？
typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;
  } uv;
} Udata;




/*
** Function Prototypes
*/
// 存放函数原型的数据结构.  LuaF_newproto 
// 使用的地方 struct LClosure.  搜索  Proto *p    / Proto *p = ci_func(L->ci)->l.p; /  Proto *p = f->l.p; 
typedef struct Proto {
  CommonHeader;
  TValue *k;  /* constants used by the function */
  // 存放函数体的opcode  // Instruction 为 unsigned int
  Instruction *code; // 存储函数编译后的字节码指令数组，是函数执行的核心逻辑载体

  // 在这个函数中定义的函数
  struct Proto **p;  /* functions defined inside the function */

  /*
 map from opcodes to source lines  用于错误定位、调试、栈回溯，关联字节码和源码行号
 lineinfo 数组的下标对应 code 数组的指令下标：
lineinfo[i] = N → 表示 code[i] 这条字节码指令对应源码的第 N 行 
  */
  int *lineinfo;  



  // 存放局部变量的数组
  struct LocVar *locvars;  /* information about local variables */
  TString **upvalues;  /* upvalue names */
  TString  *source;
  int sizeupvalues;
  int sizek;  /* size of `k' 定义的常量数量 */
  int sizecode; //代码结构数量, 对应 Instruction *code;

  int sizelineinfo; // 记录 lineinfo 数组的长度,sizelineinfo/lineinfo 是指令到源码的 “映射”

  int sizep;  /* size of `p' 当前方法中定义的其他方法数量  */
  int sizelocvars; //local 变量数量  
  int linedefined; //"function" 在文件的开始行 ,当 linedefined==0 时,为入口文件
  int lastlinedefined;//"end" 在文件的结束行
  GCObject *gclist;
  lu_byte nups;  /* number of upvalues */
  lu_byte numparams; //固定参数个数
  lu_byte is_vararg; //  标记函数是否为可变参数函数( ... 语法)
 
// 最大栈大小，它表示这个 Lua 函数在执行时，需要占用的虚拟机栈（Lua Stack）的最大深度。
// 简单来说，它是 Lua 解释器为这个函数预分配栈空间的依据，也是函数执行期间栈使用的上限。
  lu_byte maxstacksize; 
} Proto;


/* masks for new-style vararg */
#define VARARG_HASARG		1
#define VARARG_ISVARARG		2
#define VARARG_NEEDSARG		4

// 存放局部变量的结构体
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;



/*
** Upvalues
*/

typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
	// 当这个upval被close时,保存upval的值,后面可能还会被引用到
    TValue value;  /* the value (when closed) */
    // 当这个upval还在open状态时,以下链表串连在openupval链表中
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/

#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist; \
	struct Table *env

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f; //c 方法原型
  TValue upvalue[1];
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p; //lua 函数原型
  UpVal *upvals[1];
} LClosure;


//函数调用数据结构
typedef union Closure {
    CClosure c;    //c 原生方法
    LClosure l;  //lua 方法
} Closure;

#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */
  } nk;
  TValue tvk;
} TKey;

// 每个节点都有key和val
typedef struct Node {
  TValue i_val;
  TKey i_key;//key, 有链表 next 指针
} Node;


//table在设计的时候以两种结构来存放数据。
//一般情况对于整数key，会用array来存放，而其它数据类型key会存放在哈希表上。
//并且用lsizenode作为链表的长度，sizearray作为数组长度。
typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */ 
  lu_byte lsizenode;  /* log2 of size of `node' array 以2的lsizenode次方作为哈希表长度 */
  struct Table *metatable; //元表 
  TValue *array;  /* array part   保存数组时的数据*/
  Node *node; // 保存map时的数据
  Node *lastfree;  /* any free position is before this position  指向最后一个为闲置的链表空间*/
  GCObject *gclist;
  int sizearray;  /* size of `array' array */
} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
// (size&(size-1)) == 0 是检查size是2的次幂, 当size 是 2的幂时，减1 所有位都要变化,再和原数相与，结果一定是零
// (s) & ((size)-1)) = s % (size-1)
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
// sizenode返回的值必然是2的次幂
#define sizenode(t)	(twoto((t)->lsizenode))


#define luaO_nilobject		(&luaO_nilobject_)

LUAI_DATA const TValue luaO_nilobject_;

#define ceillog2(x)	(luaO_log2((x)-1) + 1)

LUAI_FUNC int luaO_log2 (unsigned int x);
LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, lua_Number *result);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

