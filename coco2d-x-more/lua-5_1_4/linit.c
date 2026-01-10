/*
** $Id: linit.c,v 1.14.1.1 2007/12/27 13:02:25 roberto Exp $
** Initialization of libraries for lua.c
** See Copyright Notice in lua.h
*/


#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lualib.h"
#include "lauxlib.h"


static const luaL_Reg lualibs[] = { //注册函数例表
  {"", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};


LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib = lualibs;
  for (; lib->func; lib++) {
	// 向函数栈 压入 c 函数
    lua_pushcfunction(L, lib->func); //比如第二个对应是 luaopen_package

    // 向函数栈 压入一个字符串参数,供对应的函数使用
    lua_pushstring(L, lib->name);

    // 调用注册的C函数, 也就是上面的一堆luaopen_*函数
    // 问题: 为什么这里不直接调用呢?非得压入lua栈中调用?
    // 传入1的原因是要跳过上一步压入的函数名,0的意思是这些函数全都返回值为0即没有返回值
    lua_call(L, 1, 0); // lua_call (lua_State *L, int nargs, int nresults) {
  }
}

