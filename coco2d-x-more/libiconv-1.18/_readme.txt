
安装 MSYS2（功能更全，适合复杂编译 ）

 
 

==================================================================================================================================================
可执行脚本有好多
	MSYS2 CLANG64 
	MSYS2 CLANGARM64 
	MSYS2 MINGW64 
	MSYS2 MSYS 
	MSYS2 UCRT64 

当前选择 运行环境 ==> MSYS2 MINGW64 Shell
编译源码 libiconv-1.18

MSYS2 是升级版的 MinGW，自带包管理器，可一键安装 make：
官网：https://www.msys2.org/  按指引安装（路径建议 C:\msys64，无空格 / 中文）
安装 make 工具：打开 MSYS2 终端（MSYS2 MSYS），执行：

pacman -Syu  # 第一次更新会提示重启终端，按提示关闭后重新打开
pacman -Su   # 二次更新剩余包

#安装编译依赖
pacman -S libtool pkg-config

# 安装 make, gcc
pacman -S make gcc   

#安装 windows 下专用的 gcc, make
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make  
pacman -S base-devel gcc  msys2-runtime-devel

pacman -S --needed   mingw-w64-x86_64-toolchain  


# 配置编译参数（指定安装路径，默认安装到 d:/local/build）
./configure --prefix=/d/local/build --host=x86_64-w64-mingw32   CC=x86_64-w64-mingw32-gcc CFLAGS="-I/mingw64/include -std=c99" LDFLAGS="-L/mingw64/lib -lws2_32" --disable-posix-process  --without-alsa   --without-pulse    --without-jack    --without-portaudio --disable-shared   --enable-static

make
make install

在安装目录找到 libiconv.a 改名为 libiconv.lib


#注意 MSYS2 命令行根目录 c:为 /c,  d:为 /d   等等..
# 切换到 C 盘根目录
cd /c

编译只要静态库,不要dll. --disable-shared ,因为编 dll 有 asm 兼容问题，可在豆包解决方法(未测试),问: 在 Windows  怎样编译 Iconv库. 


==================================================================================================================================================

#生成 makefile 文件. 若为交叉编译，指定 MinGW 编译器和 C 标准（C99 及以上）
./configure \
  --prefix=/c/build \  # 绝对路径
  --host=x86_64-w64-mingw32 \
  CC=x86_64-w64-mingw32-gcc \   #指定用 windows 下的 gcc
  CFLAGS="-std=c99" \ # 强制 C99 标准（wint_t 是 C99 特性）
  --enable-static  # 编译成静态库
  
./configure --prefix=/d/local/build --host=x86_64-w64-mingw32   CC=x86_64-w64-mingw32-gcc CFLAGS="-I/mingw64/include -std=c99" LDFLAGS="-L/mingw64/lib -lws2_32"  --disable-posix-process  --without-alsa   --without-pulse    --without-jack    --without-portaudio --enable-static

# 编译（使用 mingw32-make 替代 make，兼容性更好）
mingw32-make
 
//失败时清除缓存
make clean 
rm -f config.cache
 
 