/*
** $Id: lapi.c,v 2.55.1.5 2008/07/04 18:41:18 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/


#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#define lapi_c
#define LUA_CORE

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char lua_ident[] =
  "$Lua: " LUA_RELEASE " " LUA_COPYRIGHT " $\n"
  "$Authors: " LUA_AUTHORS " $\n"
  "$URL: www.lua.org $\n";



#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->base))

#define api_checkvalidindex(L, i)	api_check(L, (i) != luaO_nilobject)

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}


static TValue *index2adr (lua_State *L, int idx) {
  if (idx > 0) {
	// 如果idx > 0,则从栈中base为基础位置取元素
    TValue *o = L->base + (idx - 1);
    api_check(L, idx <= L->ci->top - L->base);
    if (o >= L->top) return cast(TValue *, luaO_nilobject);
    else return o;
  }
  else if (idx > LUA_REGISTRYINDEX) {
	// 如果LUA_REGISTRYINDEX > idx < 0,则从栈中top为基础位置取元素
    api_check(L, idx != 0 && -idx <= L->top - L->base);
    return L->top + idx;
  }
  else switch (idx) {  /* pseudo-indices */
    case LUA_REGISTRYINDEX: return registry(L);
    case LUA_ENVIRONINDEX: {
      Closure *func = curr_func(L);
      sethvalue(L, &L->env, func->c.env);
      return &L->env;
    }
    case LUA_GLOBALSINDEX: return gt(L);
    default: {
      Closure *func = curr_func(L);
      idx = LUA_GLOBALSINDEX - idx;
      return (idx <= func->c.nupvalues)
                ? &func->c.upvalue[idx-1]
                : cast(TValue *, luaO_nilobject);
    }
  }
}

// 获取当前环境表
static Table *getcurrenv (lua_State *L) {
  if (L->ci == L->base_ci)  /* no enclosing function? */
	// 如果当前不在任何函数中,那个使用全局表
    return hvalue(gt(L));  /* use global table as environment */
  else {
    Closure *func = curr_func(L);
    return func->c.env;
  }
}


void luaA_pushobject (lua_State *L, const TValue *o) {
  setobj2s(L, L->top, o);
  api_incr_top(L);
}


LUA_API int lua_checkstack (lua_State *L, int size) {
  int res = 1;
  lua_lock(L);
  if (size > LUAI_MAXCSTACK || (L->top - L->base + size) > LUAI_MAXCSTACK)
    res = 0;  /* stack overflow */
  else if (size > 0) {
    luaD_checkstack(L, size);
    if (L->ci->top < L->top + size)
      L->ci->top = L->top + size;
  }
  lua_unlock(L);
  return res;
}

//传递 同一个 全局状态机下不同线程中的值。
//这个函数会从 from 的堆栈中弹出 n 个值， 然后把它们压入 to 的堆栈中
LUA_API void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  if (from == to) return;
  lua_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to));
  api_check(from, to->ci->top - to->top >= n);
  // from的栈指针-n,表示栈顶需要移动的元素回退
  from->top -= n;
  // 依次从from中把需要的变量移动到to中
  for (i = 0; i < n; i++) {
    setobj2s(to, to->top++, from->top + i);
  }
  lua_unlock(to);
}


LUA_API void lua_setlevel (lua_State *from, lua_State *to) {
  to->nCcalls = from->nCcalls;
}


LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
}

//创建一个新线程，并将其压入堆栈， 并返回维护这个线程的 lua_State 指针。 
//  这个函数返回的新状态机共享原有状态机中的所有对象（比如一些 table）， 但是它有独立的执行堆栈。
//  没有显式的函数可以用来关闭或销毁掉一个线程。 线程跟其它 Lua 对象一样是垃圾收集的条目之一。
//  新线程L1以一个空栈开始运行，老线程L的栈顶就是这个新线程。
LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_State *L1;
  lua_lock(L);
  luaC_checkGC(L);
  L1 = luaE_newthread(L);
  setthvalue(L, L->top, L1);
  api_incr_top(L);
  lua_unlock(L);
  luai_userstatethread(L, L1);
  return L1;
}

/*
 当调用Lua C API中的大多数函数时，这些函数都作用于某个特定的栈。
 当我们调用lua_pushnumber时，就会将数字压入一个栈中，那么Lua是如何知道该使用哪个栈的呢？
 答案就在类型lua_State中。这些C API的第一个参数不仅表示了一个Lua状态，还表示了一个记录在该状态中的线程。

 只要创建一个Lua状态，Lua就会自动在这个状态中创建一个新线程，这个线程称为"主线程"。主线程永远不会被回收。
 当调用lua_close关闭状态时，它会随着状态一起释放。调用lua_newthread便可以在一个状态中创建其他的线程。
 
 lua_State *lua_newthread(lua_State *L);
 这个函数返回一个lua_State指针，表示新建的线程。它会将新线程作为一个类型为"thread"的值压入栈中。如果我们执行了：
 
 L1 = lua_newthread(L);
 现在，我们拥有了两个线程L和L1，它们内部都引用了相同的Lua状态。每个线程都有其自己的栈。
 新线程L1以一个空栈开始运行，老线程L的栈顶就是这个新线程。
 
 除了主线程以外，其它线程和其它Lua对象一样都是垃圾回收的对象。
 当新建一个线程时，线程会压入栈，这样能确保新线程不会成为垃圾，
 而有的时候，你在处理栈中数据时，不经意间就把线程弹出栈了，而当你再次使用该线程时，
 可能导致找不到对应的线程而程序崩溃。为了避免这种情况的发生，可以保持一个对线程的引用，
 比如在注册表中保存一个对线程的引用。
 
 当拥有了一个线程以后，我们就可以像主线程那样来使用它，以前博文中提到的对栈的操作，
 对这个新的线程都适用。然而，使用多线程的目的不是为了实现这些简单的功能，而是为了实现协同程序。
 
 为了挂起某些协同程序的执行，并在稍后恢复执行，我们可以使用lua_resume函数来实现。
 
 int lua_resume(lua_State *L, int narg);
 lua_resume可以启动一个协同程序，它的用法就像lua_call一样。
 将待调用的函数压入栈中，并压入其参数，最后在调用lua_resume时传入参数的数量narg。
 这个行为与lua_pcall类似，但有3点不同。
 
 lua_resume 没有参数用于指出期望的结果数量，它总是返回被调用函数的所有结果；
 它没有用于指定错误处理函数的参数，发生错误时不会展开栈，这就可以在发生错误后检查栈中的情况；
 如果正在运行的函数交出（yield）了控制权，lua_resume就会返回一个特殊的代码LUA_YIELD，并将线程置于一个可以被再次恢复执行的状态。
 当lua_resume返回LUA_YIELD时，线程的栈中只能看到交出控制权时所传递的那些值。调用lua_gettop则会返回这些值的数量。
 若要将这些值移到另一个线程，可以使用lua_xmove。
 
 为了恢复一个挂起线程的执行，可以再次调用lua_resume。在这种调用中，Lua假设栈中所有的值都是由yield调用返回的，
 当然了，你也可以任意修改栈中的值。作为一个特例，如果在一个lua_resume返回后与再次调用lua_resume之间没有改变过线程栈中的内容，
 那么yield恰好返回它交出的值。如果能很好的理解这个特例是什么意思，那就说明你已经非常理解Lua中的协同程序了

 * */






/*
** basic stack manipulation
*/


LUA_API int lua_gettop (lua_State *L) {
  return cast_int(L->top - L->base);
}


LUA_API void lua_settop (lua_State *L, int idx) {
  lua_lock(L);
  if (idx >= 0) {
    api_check(L, idx <= L->stack_last - L->base);
    while (L->top < L->base + idx)
      setnilvalue(L->top++);
    L->top = L->base + idx;
  }
  else {
    api_check(L, -(idx+1) <= (L->top - L->base));
    L->top += idx+1;  /* `subtract' index (index is negative) */
  }
  lua_unlock(L);
}


LUA_API void lua_remove (lua_State *L, int idx) {
  StkId p;
  lua_lock(L);
  p = index2adr(L, idx);
  api_checkvalidindex(L, p);
  // 把idx后面的数据往前挪
  while (++p < L->top) setobjs2s(L, p-1, p);
  L->top--;
  lua_unlock(L);
}


LUA_API void lua_insert (lua_State *L, int idx) {
  StkId p;
  StkId q;
  lua_lock(L);
  p = index2adr(L, idx);
  api_checkvalidindex(L, p);
  for (q = L->top; q>p; q--) setobjs2s(L, q, q-1);
  setobjs2s(L, p, L->top);
  lua_unlock(L);
}

//把栈顶元素移动到给定位置（并且把这个栈顶元素弹出）， 
//不移动任何元素（因此在那个位置处的值被覆盖掉）
LUA_API void lua_replace (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  /* explicit test for incompatible code */
  if (idx == LUA_ENVIRONINDEX && L->ci == L->base_ci)
    luaG_runerror(L, "no calling environment");
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  if (idx == LUA_ENVIRONINDEX) {
    Closure *func = curr_func(L);
    api_check(L, ttistable(L->top - 1)); 
    func->c.env = hvalue(L->top - 1);
    luaC_barrier(L, func, L->top - 1);
  }
  else {
    setobj(L, o, L->top - 1);
    if (idx < LUA_GLOBALSINDEX)  /* function upvalue? */
      luaC_barrier(L, curr_func(L), L->top - 1);
  }
  L->top--;
  lua_unlock(L);
}

//把堆栈上给定有效处索引处的元素作一个拷贝压栈。
LUA_API void lua_pushvalue (lua_State *L, int idx) {
  lua_lock(L);
  setobj2s(L, L->top, index2adr(L, idx));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (o == luaO_nilobject) ? LUA_TNONE : ttype(o);
}


LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  return (t == LUA_TNONE) ? "no value" : luaT_typenames[t];
}


LUA_API int lua_iscfunction (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return iscfunction(o);
}


LUA_API int lua_isnumber (lua_State *L, int idx) {
  TValue n;
  const TValue *o = index2adr(L, idx);
  return tonumber(o, &n);
}


LUA_API int lua_isstring (lua_State *L, int idx) {
  int t = lua_type(L, idx);
  return (t == LUA_TSTRING || t == LUA_TNUMBER);
}


LUA_API int lua_isuserdata (lua_State *L, int idx) {
  const TValue *o = index2adr(L, idx);
  return (ttisuserdata(o) || ttislightuserdata(o));
}


LUA_API int lua_rawequal (lua_State *L, int index1, int index2) {
  StkId o1 = index2adr(L, index1);
  StkId o2 = index2adr(L, index2);
  return (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
         : luaO_rawequalObj(o1, o2);
}


LUA_API int lua_equal (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = index2adr(L, index1);
  o2 = index2adr(L, index2);
  i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0 : equalobj(L, o1, o2);
  lua_unlock(L);
  return i;
}


LUA_API int lua_lessthan (lua_State *L, int index1, int index2) {
  StkId o1, o2;
  int i;
  lua_lock(L);  /* may call tag method */
  o1 = index2adr(L, index1);
  o2 = index2adr(L, index2);
  i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0
       : luaV_lessthan(L, o1, o2);
  lua_unlock(L);
  return i;
}


//把给定索引处的 Lua 值转换为 lua_Number(double) 这样一个 C 类型。 这个 Lua 值必须是一个数字或是一个可转换为数字的字符串， 否则，lua_tonumber 返回 0 
LUA_API lua_Number lua_tonumber (lua_State *L, int idx) {
  TValue n;
  const TValue *o = index2adr(L, idx);
  if (tonumber(o, &n))
    return nvalue(o);
  else
    return 0;
}

//把给定索引处的 Lua 值转换为 lua_Integer(__int64) 这样一个有符号整数类型。 这个 Lua 值必须是一个数字或是一个可以转换为数字的字符串， 否则 lua_tointeger 返回 0 。
LUA_API lua_Integer lua_tointeger (lua_State *L, int idx) {
  TValue n;
  const TValue *o = index2adr(L, idx);
  if (tonumber(o, &n)) {
    lua_Integer res;
    lua_Number num = nvalue(o);
    lua_number2integer(res, num);
    return res;
  }
  else
    return 0;
}


/*
把指定的索引处的的 Lua 值转换为一个 C 中的 boolean 值（ 0 或是 1 ） 
和 Lua 中做的所有测试一样, lua_toboolean 会把任何 不同于 false 和 nil 的值当作 1 返回 
否则就返回 0.  如果用一个无效索引去调用也会返回 0 

如果想只接收真正的 boolean 值，就需要使用 lua_isboolean 来测试值的类型。
*/
LUA_API int lua_toboolean (lua_State *L, int idx) {
  const TValue *o = index2adr(L, idx);
  return !l_isfalse(o);
}



/*
*
lua_tostring 是利用 lua_tolstring 的一个宏 . 
lua_tolstring 返回 Lua 状态机中字符串的以对齐指针.
这个字符串总能保证最后一个字符为零 ('\0') ,而且它允许在字符串内包含多个这样的零.
因为 Lua 中可能发生垃圾收集,所以不保证 lua_tolstring 返回的指针,在对应的值从堆栈中移除后依然有效. 
 
 
把给定索引处的 Lua 值转换为一个 C 字符串. 如果 len 不为 NULL , 它还把字符串长度设到 *len 中. 
这个 Lua 值必须是一个字符串或是一个数字； 否则返回返回 NULL . 
如果值是一个数字,lua_tolstring 还会把堆栈中的那个值的实际类型转换为一个字符串.
*/
LUA_API const char *lua_tolstring (lua_State *L, int idx, size_t *len) {
  StkId o = index2adr(L, idx);
  if (!ttisstring(o)) {
    lua_lock(L);  /* `luaV_tostring' may create a new string */
    if (!luaV_tostring(L, o)) {  /* conversion failed? */
      if (len != NULL) *len = 0;
      lua_unlock(L);
      return NULL;
    }
    luaC_checkGC(L);
    o = index2adr(L, idx);  /* previous call may reallocate the stack */
    lua_unlock(L);
  }
  if (len != NULL) *len = tsvalue(o)->len;
  return svalue(o);
}


LUA_API size_t lua_objlen (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TSTRING: return tsvalue(o)->len;
    case LUA_TUSERDATA: return uvalue(o)->len;
    case LUA_TTABLE: return luaH_getn(hvalue(o));
    case LUA_TNUMBER: {
      size_t l;
      lua_lock(L);  /* `luaV_tostring' may create a new string */
      l = (luaV_tostring(L, o) ? tsvalue(o)->len : 0);
      lua_unlock(L);
      return l;
    }
    default: return 0;
  }
}


/*

可能你会疑惑为什么有 lua_isfunction 和 lua_iscfunction 函数,但却只有 lua_tocfunction 而没有 lua_tofunction 函数,
其实仔细想想就知道只有转化成c function才有意义,假设是一个 lua function 在 c 代码里是没有用处的.
  

 
把给定索引处的 Lua 值转换为一个 C 函数. 这个值必须是一个 C 函数；如果不是就返回 NULL .
*/
LUA_API lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (!iscfunction(o)) ? NULL : clvalue(o)->c.f;
}

/*
如果给定索引处的值是一个完整的 userdata ,函数返回内存块的地址. 
如果值是一个 light userdata ,那么就返回它表示的指针. 否则返回 NULL 
*/
LUA_API void *lua_touserdata (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TUSERDATA: return (rawuvalue(o) + 1);
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}

/*
把给定索引处的值转换为一个 Lua 线程（由 lua_State* 代表）. 这个值必须是一个线程；否则函数返回 NULL .
*/
LUA_API lua_State *lua_tothread (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}

// 把给定索引处的值转换为一般的 C 指针 (void*) . 这个值可以是一个 userdata ,table ,thread 或是一个 function  
// 否则,lua_topointer 返回 NULL . 不同的对象有不同的指针. 不存在把指针再转回原有类型的方法.
LUA_API const void *lua_topointer (lua_State *L, int idx) {
  StkId o = index2adr(L, idx);
  switch (ttype(o)) {
    case LUA_TTABLE: return hvalue(o);
    case LUA_TFUNCTION: return clvalue(o);
    case LUA_TTHREAD: return thvalue(o);
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
      return lua_touserdata(L, idx);
    default: return NULL;
  }
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setnvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushinteger (lua_State *L, lua_Integer n) {
  lua_lock(L);
  setnvalue(L->top, cast_num(n));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushlstring (lua_State *L, const char *s, size_t len) {
  lua_lock(L);
  luaC_checkGC(L);
  setsvalue2s(L, L->top, luaS_newlstr(L, s, len));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushstring (lua_State *L, const char *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_pushlstring(L, s, strlen(s));
}


LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  lua_lock(L);
  luaC_checkGC(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  lua_unlock(L);
  return ret;
}


LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lua_lock(L);
  luaC_checkGC(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_unlock(L);
  return ret;
}

// 向栈中push一个C函数,n是upval的数量
//把一个新的 C closure 压入堆栈。
//当创建了一个 C 函数后，你可以给它关联一些值，这样就是在创建一个 C closure （参见 3.4）:
//  接下来无论函数何时被调用，这些值都可以被这个函数访问到。 
//  为了将一些值关联到一个 C 函数上， 首先这些值需要先被压入堆栈（如果有多个值，第一个先压）
//  接下来调用 lua_pushcclosure 来创建出 closure 并把这个 C 函数压到堆栈上。 
//  参数 n 告之函数有多少个值需要关联到函数上。 lua_pushcclosure 也会把这些值从栈上弹出。
LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  Closure *cl;
  lua_lock(L);
  luaC_checkGC(L);
  api_checknelems(L, n);
  // 创建一个closure指针
  cl = luaF_newCclosure(L, n, getcurrenv(L));
  // 记录函数指针
  cl->c.f = fn;
  // 首先将栈空出n个位置来存放参数
  L->top -= n; //弹出关联的参数
  // 初始化upval存放的位置
  while (n--)
    setobj2n(L, &cl->c.upvalue[n], L->top+n);//关联参数
  // 将closure指针push到栈中
  setclvalue(L, L->top, cl);//把 closure 放到栈顶
  lua_assert(iswhite(obj2gco(cl)));
  // 栈指针加1
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, (b != 0));  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}

//把一个 light userdata 压栈。
//userdata 在 Lua 中表示一个 C 值。 light userdata 表示一个指针。 
//它是一个像数字一样的值： 你不需要专门创建它，它也没有独立的 metatable ， 而且也不会被收集（因为从来不需要创建）。 
//只要表示的 C 地址相同，两个 light userdata 就相等
LUA_API void lua_pushlightuserdata (lua_State *L, void *p) {
  lua_lock(L);


//#define setpvalue(obj,x) \
//  { TValue *i_o=(obj); i_o->value.p=(x); i_o->tt=LUA_TLIGHTUSERDATA; }
  setpvalue(L->top, p); //当前栈顶设置数据 x
  
  api_incr_top(L); //栈顶+1
  lua_unlock(L);
}


LUA_API int lua_pushthread (lua_State *L) {
  lua_lock(L);
  setthvalue(L, L->top, L);
  api_incr_top(L);
  lua_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Lua -> stack)
*/
// gettable会调用到luaV_gettable,这样如果在本身没找到,还会根据__index方法到基类中查找
// 而raw系列只会在自己上面查找
/*
   void lua_gettable (lua_State *L, int index)
   操作:     ele  = Stack[index]
             key = Stack.top()
             Stack.pop()
             value = ele[key]
             Stack.push(value)
   根据index指定取到相应的表; 取栈顶元素为key, 并弹出栈; 获取表中key的值压入栈顶.
   无返回值
   栈高度不变, 但是发生了一次弹出和压入的操作, 弹出的是key, 压入的是value
   注意, 该操作将触发 __index 元方法
 * */
LUA_API void lua_gettable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  luaV_gettable(L, t, L->top - 1, L->top - 1);
  lua_unlock(L);
}

// 从一个table中(idx所在)查找key对应的值, 找到值存放在top - 1中返回
LUA_API void lua_getfield (lua_State *L, int idx, const char *k) {
  StkId t;
  TValue key;
  lua_lock(L);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  setsvalue(L, &key, luaS_new(L, k));
  // 查找key对应的值, 找到存放到top中
  luaV_gettable(L, t, &key, L->top);
  api_incr_top(L);
  lua_unlock(L);
}

// 从idx存放的表中,根据top - 1存放的field名称查找表成员,并且将结果push到top - 1的栈中
// raw系列只会在自己上面查找,因为它调用的是luaH_get
//类似于 lua_gettable， 但是作一次直接访问（不触发元方法）。
LUA_API void lua_rawget (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));

//#define setobj(L,obj1,obj2) \
//  { const TValue *o2=(obj2); TValue *o1=(obj1); \
//    o1->value = o2->value; o1->tt=o2->tt; \
//    checkliveness(G(L),o1); }

  setobj2s(L, L->top - 1, luaH_get(hvalue(t), L->top - 1));// 按  L->top - 1 的 key 取在 t  上的值，再放到 L->top - 1 上
  lua_unlock(L);
}


LUA_API void lua_rawgeti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  setobj2s(L, L->top, luaH_getnum(hvalue(o), n));
  api_incr_top(L);
  lua_unlock(L);
}

//创建一个新的空 table 压入堆栈。 这个新 table 将被预分配 narr 个元素的数组空间 以及 nrec 个元素的非数组空间。 
//当你明确知道表中需要多少个元素时，预分配就非常有用。 如果你不知道，可以使用函数 lua_newtable(L) 相当于 lua_createtable(L, 0, 0)
LUA_API void lua_createtable (lua_State *L, int narray, int nrec) {
  lua_lock(L);
  luaC_checkGC(L);
  sethvalue(L, L->top, luaH_new(L, narray, nrec));
  api_incr_top(L);
  lua_unlock(L);
}


LUA_API int lua_getmetatable (lua_State *L, int objindex) {
  const TValue *obj;
  Table *mt = NULL;
  int res;
  lua_lock(L);
  obj = index2adr(L, objindex);
  switch (ttype(obj)) {
    case LUA_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt == NULL)
    res = 0;
  else {
    sethvalue(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


LUA_API void lua_getfenv (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  switch (ttype(o)) {
    case LUA_TFUNCTION:
      sethvalue(L, L->top, clvalue(o)->c.env);
      break;
    case LUA_TUSERDATA:
      sethvalue(L, L->top, uvalue(o)->env);
      break;
    case LUA_TTHREAD:
      setobj2s(L, L->top,  gt(thvalue(o)));
      break;
    default:
      setnilvalue(L->top);
      break;
  }
  api_incr_top(L);
  lua_unlock(L);
}


/*
** set functions (stack -> Lua)
*/
//作一个等价于 t[k] = v 的操作， 这里 t 是一个给定有效索引 index 处的值， v 指栈顶的值， 而 k 是栈顶之下的那个值。
//这个函数会把键和值都从堆栈中弹出。 和在 Lua 中一样，这个函数可能触发 "newindex" 事件的元方法
// 向idx索引的表中,插入key为top - 2,val为top - 1的数据,完事了之后top - 2把k/v退栈
LUA_API void lua_settable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}

//出栈 1
//做一个等价于 t[k] = v 的操作， 这里 t 是给出的有效索引 index 处的值， 而 v 是栈顶的那个值。
//这个函数将把这个值弹出堆栈。 跟在 Lua 中一样，这个函数可能触发一个 "newindex" 事件的元方法 
// 将k与idx的值对应, 换句话说,比如用table[k]找到的是idx的值
LUA_API void lua_setfield (lua_State *L, int idx, const char *k) {
  StkId t;
  TValue key;
  lua_lock(L);
  api_checknelems(L, 1);
  // 首先寻找idx对应的值
  t = index2adr(L, idx);
  api_checkvalidindex(L, t);
  // 分配一个字符串,存放k值
  setsvalue(L, &key, luaS_new(L, k));
  // 将key的值为top - 1的值,将它们的对应关系写到t中(t一般是个table)
  luaV_settable(L, t, &key, L->top - 1);
  L->top--;  /* pop value */
  lua_unlock(L);
}


LUA_API void lua_rawset (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  setobj2t(L, luaH_set(L, hvalue(t), L->top-2), L->top-1);
  luaC_barriert(L, hvalue(t), L->top-1);
  L->top -= 2;
  lua_unlock(L);
}


LUA_API void lua_rawseti (lua_State *L, int idx, int n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_check(L, ttistable(o));
  setobj2t(L, luaH_setnum(L, hvalue(o), n), L->top-1);
  luaC_barriert(L, hvalue(o), L->top-1);
  L->top--;
  lua_unlock(L);
}


//出栈 1
LUA_API int lua_setmetatable (lua_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = index2adr(L, objindex);
  api_checkvalidindex(L, obj);
  if (ttisnil(L->top - 1))
    mt = NULL;
  else {
    api_check(L, ttistable(L->top - 1));
    mt = hvalue(L->top - 1);
  }
  switch (ttype(obj)) {
    case LUA_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt)
        luaC_objbarriert(L, hvalue(obj), mt);
      break;
    }
    case LUA_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt)
        luaC_objbarrier(L, rawuvalue(obj), mt);
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  lua_unlock(L);
  return 1;
}

//  从堆栈上弹出一个 table 并把它设为指定索引处值的新环境。 
//  如果指定索引处的值即不是函数又不是线程或是 userdata ， lua_setfenv 会返回 0 ， 否则返回 1 。
LUA_API int lua_setfenv (lua_State *L, int idx) {
  StkId o;
  int res = 1;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2adr(L, idx);
  api_checkvalidindex(L, o);
  api_check(L, ttistable(L->top - 1));
  switch (ttype(o)) {
    case LUA_TFUNCTION:
      clvalue(o)->c.env = hvalue(L->top - 1);
      break;
    case LUA_TUSERDATA:
      uvalue(o)->env = hvalue(L->top - 1);
      break;
    case LUA_TTHREAD:
      sethvalue(L, gt(thvalue(o)), hvalue(L->top - 1));
      break;
    default:
      res = 0;
      break;
  }
  if (res) luaC_objbarrier(L, gcvalue(o), hvalue(L->top - 1));
  L->top--;
  lua_unlock(L);
  return res;
}


/*
** `load' and `call' functions (run Lua code)
*/

//不定参数，栈平衡?
#define adjustresults(L,nres) \
    { if (nres == LUA_MULTRET && L->top >= L->ci->top) L->ci->top = L->top; }   //.faq


// L->ci 当前调用方法。
// 调用方法的返回值会压入栈中, 覆盖参数？ 如果返回数比参数多，ci->top 比 L->top 要大,开辟更多的栈空间 ？
#define checkresults(L,na,nr) \
     api_check(L, (nr) == LUA_MULTRET || (L->ci->top - L->top >= (nr) - (na)))
	
//调用前先把要调用的方法压栈，再把参数压栈
LUA_API void lua_call (lua_State *L, int nargs, int nresults) {
  StkId func;
  lua_lock(L);  //.faq 什么时候有效？
    
    //printf("nargs[%d] nresults[%d] L->base[%d] L->top[%d]\n",nargs, nresults, L->base, L->top); 
  //#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->base))
  api_checknelems(L, nargs+1);//.faq 为什么要这个判断? 确定栈里有这样多的参数?
  
  checkresults(L, nargs, nresults);
  // 当前top指针后退nargs + 1个位置得到函数指针
  func = L->top - (nargs+1);//获得方法在栈的位置
  luaD_call(L, func, nresults);
  adjustresults(L, nresults);
  lua_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to `f_call' */
  StkId func;
  int nresults;
};


static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_call(L, c->func, c->nresults);
}



LUA_API int lua_pcall (lua_State *L, int nargs, int nresults, int errfunc) {
  struct CallS c;//为了安全调用方法，加了个封装，走安全调用, 在f_call使用
  int status;
  ptrdiff_t func;
  lua_lock(L);
  api_checknelems(L, nargs+1); //方法压栈+1, 参数压栈nargs
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2adr(L, errfunc); // 获得全局位置 StkId o 
    api_checkvalidindex(L, o);
    func = savestack(L, o);
  }
  c.func = L->top - (nargs+1);  /* function to be called */
  c.nresults = nresults;
  status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  adjustresults(L, nresults);
  lua_unlock(L);
  return status;
}


/*
** Execute a protected C call.
*/
struct CCallS {  /* data to `f_Ccall' */
  lua_CFunction func;
  void *ud;
};


static void f_Ccall (lua_State *L, void *ud) {
  struct CCallS *c = cast(struct CCallS *, ud);
  Closure *cl;
  cl = luaF_newCclosure(L, 0, getcurrenv(L));
  cl->c.f = c->func;
  setclvalue(L, L->top, cl);  /* push function */
  api_incr_top(L);
  setpvalue(L->top, c->ud);  /* push only argument */
  api_incr_top(L);
  luaD_call(L, L->top - 2, 0);
}


LUA_API int lua_cpcall (lua_State *L, lua_CFunction func, void *ud) {
  struct CCallS c;
  int status;
  lua_lock(L);
  c.func = func;
  c.ud = ud;
  status = luaD_pcall(L, f_Ccall, &c, savestack(L, L->top), 0);
  lua_unlock(L);
  return status;
}

//lua_load 把一个编译好的 chunk 作为一个 Lua 函数压入堆栈
//仅仅加栽 chunk ；而不会去运行它。
LUA_API int lua_load (lua_State *L, lua_Reader reader, void *data,
                      const char *chunkname) {
  ZIO z;
  int status;
  lua_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(L, &z, reader, data);
  status = luaD_protectedparser(L, &z, chunkname);
  lua_unlock(L);
  return status;
}


LUA_API int lua_dump (lua_State *L, lua_Writer writer, void *data) {
  int status;
  TValue *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = L->top - 1;
  if (isLfunction(o))
    status = luaU_dump(L, clvalue(o)->l.p, writer, data, 0);
  else
    status = 1;
  lua_unlock(L);
  return status;
}


LUA_API int  lua_status (lua_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/

LUA_API int lua_gc (lua_State *L, int what, int data) {
  int res = 0;
  global_State *g;
  lua_lock(L);
  g = G(L);
  switch (what) {
    case LUA_GCSTOP: {
      // 停止GC, 把阙值标记为一个无限大的量, 这样就无法自动GC了
      g->GCthreshold = MAX_LUMEM;
      break;
    }
    case LUA_GCRESTART: {
      // GC重新开始, 将阙值定为当前内存值
      g->GCthreshold = g->totalbytes;
      break;
    }
    case LUA_GCCOLLECT: {
      // 全回收 不管三七二十一
      luaC_fullgc(L);
      break;
    }
    case LUA_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      // 换算成KBytes
      res = cast_int(g->totalbytes >> 10);
      break;
    }
    case LUA_GCCOUNTB: {
      // 换算成bit
      res = cast_int(g->totalbytes & 0x3ff);
      break;
    }
    case LUA_GCSTEP: {
      // 首先换算成byte
      lu_mem a = (cast(lu_mem, data) << 10);
      if (a <= g->totalbytes)
        g->GCthreshold = g->totalbytes - a;
      else
        g->GCthreshold = 0;
      while (g->GCthreshold <= g->totalbytes) {
    	  // 当当前内存数据还是大于所要求的阙值时,就一直进行回收操作
        luaC_step(L);
        // GC停止了, 返回错误
        if (g->gcstate == GCSpause) {  /* end of cycle? */
          res = 1;  /* signal it */
          break;
        }
      }
      break;
    }
    case LUA_GCSETPAUSE: {
      res = g->gcpause;
      g->gcpause = data;
      break;
    }
    case LUA_GCSETSTEPMUL: {
      res = g->gcstepmul;
      g->gcstepmul = data;
      break;
    }
    default: res = -1;  /* invalid option */
  }
  lua_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


LUA_API int lua_error (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaG_errormsg(L);
  lua_unlock(L);
  return 0;  /* to avoid warnings */
}


LUA_API int lua_next (lua_State *L, int idx) {
  StkId t;
  int more;
  lua_lock(L);
  t = index2adr(L, idx);
  api_check(L, ttistable(t));
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return more;
}


LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaC_checkGC(L);
    luaV_concat(L, n, cast_int(L->top - L->base) - 1);
    L->top -= (n-1);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue2s(L, L->top, luaS_newlstr(L, "", 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  lua_unlock(L);
}


LUA_API lua_Alloc lua_getallocf (lua_State *L, void **ud) {
  lua_Alloc f;
  lua_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  lua_unlock(L);
  return f;
}


LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud) {
  lua_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  lua_unlock(L);
}


LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  luaC_checkGC(L);
  u = luaS_newudata(L, size, getcurrenv(L));
  setuvalue(L, L->top, u);
  api_incr_top(L);
  lua_unlock(L);
  return u + 1;
}




static const char *aux_upvalue (StkId fi, int n, TValue **val) {
  Closure *f;
  if (!ttisfunction(fi)) return NULL;
  f = clvalue(fi);
  if (f->c.isC) {
    if (!(1 <= n && n <= f->c.nupvalues)) return NULL;
    *val = &f->c.upvalue[n-1];
    return "";
  }
  else {
    Proto *p = f->l.p;
    if (!(1 <= n && n <= p->sizeupvalues)) return NULL;
    *val = f->l.upvals[n-1]->v;
    return getstr(p->upvalues[n-1]);
  }
}


LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val;
  lua_lock(L);
  name = aux_upvalue(index2adr(L, funcindex), n, &val);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val;
  StkId fi;
  lua_lock(L);
  fi = index2adr(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val);
  if (name) {
    L->top--;
    setobj(L, val, L->top);
    luaC_barrier(L, clvalue(fi), L->top);
  }
  lua_unlock(L);
  return name;
}

