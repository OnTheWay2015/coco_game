
//中文api
https://blog.csdn.net/runaying/article/details/15026661?spm=1001.2014.3001.5501

//4.0
https://docs.cocos2d-x.org/api-ref/cplusplus/V4.0/index.html

注：当前编译使用 x86_64


1.安装 设置 python2.7

2. TortoiseGit 拉取源码时，需要获取 子模块（submodule）, 
	a. 首次克隆包含 submodule 的项目（直接同步 submodule）, 在clone 弹出窗口，要勾选窗口下方的 「Recurse submodules」（递归拉取子模块） 选项,有的只有一个词 「Recurse」
	b. 如果是已拉取的仓库，没拉取 submodule 时， 到根目录下右键 -> TortoiseGit -> update submodule... 
	c. test 开头的路径不用拉，tools开头的两个路径选择拉取 (默认是都选择了，手动点击去除 test)
	d. 执行  D:\git_work\cocos2d-x> python .\download-deps.py
	
2.1 运行 源码根目录 setup.py 设置环境变量,  COCOS_CONSOLE_ROOT 等等 (自动添加 path 路径,设置 cocos 工具目录,当前添加了 D:\git_work\cocos2d-x\tools\cocos2d-console\bin, D:\git_work\cocos2d-x\templates ,手动设NDK,SDK)
		D:\git_work\cocos2d-x> python .\setup.py
	注意：安装 cocos studio 同样会设置对应的环境变量，如果后安装 cocos studio,注意对应环境变量是否付合意图
	
	
3.创建项目 https://docs.cocos.com/cocos2d-x/manual/zh/editors_and_tools/cocosCLTool.html
	
	cocos new MyGame -p com.MyCompany.MyGame -l cpp -d ~/MyCompany

	cocos new MyGame -p com.MyCompany.MyGame -l lua -d ~/MyCompany

	cocos new MyGame -p com.MyCompany.MyGame -l js -d ~/MyCompany

3.1 全局修改所有 CMakeLists.txt 里的 版本要求高一些, cmake_minimum_required(VERSION 3.6) --> cmake_minimum_required(VERSION 3.21)

3.2 创建 32 位的工程，可直接使用包里编译好的lib文件,顺利编译工程. C:\_work\projs\MyCompany\MyGame\bbb> cmake .. -G "Visual Studio 17 2022" -A win32
	创建 64 位的工程，需要编译对应第三方库的 64位 lib。  C:\_work\projs\MyCompany\MyGame\bbb>cmake .. -G "Visual Studio 17 2022" -A x64
 	
	
	
4.error C2065: “GWL_WNDPROC”: 未声明的标识符 是因为在 64 位 Windows 编译环境下，MSVC 不再支持 GWL_WNDPROC 等 32 位窗口扩展标志，取而代之的是 **GWLP_WNDPROC**（后缀P表示指针，适配 64 位地址）。
具体背景：
GWL_WNDPROC 是 32 位 Windows 系统中用于获取 / 设置窗口过程函数的标志（属于GetWindowLong API 的参数）；
在 64 位 Windows 系统中，GetWindowLong 被GetWindowLongPtr替代，对应的标志也从GWL_*改为GWLP_*（如GWL_WNDPROC → GWLP_WNDPROC）；
Cocos2d-x 4.0 的UIEditBoxImpl-win32.cpp代码中仍使用了老旧的 GWL_WNDPROC，在 64 位编译模式下会因标识符未定义报错（32 位模式下可能兼容，但 64 位模式下直接失效）。

修复方案
替换 GWL_WNDPROC 为 GWLP_WNDPROC（核心修复）
 

5. “GWL_USERDATA”: 未声明的标识符 与之前的 GWL_WNDPROC 错误原因完全一致：在 64 位 Windows 编译环境下，MSVC 不再支持 32 位的窗口扩展标志 GWL_USERDATA ，取而代之的是 64 位兼容的 GWLP_USERDATA。
修复方案
替换 GWL_USERDATA 为 GWLP_USERDATA（核心修复）
 
 
 
 
 
 
6.编译 cocos2d-x-external 和 coco_game\coco2d-x-more 里的工程 
	cocos2d-x-external\src\websockets 工程是单独的,找到对应目录,用 cmake 生成vs工程编译
		cmake 生成工程时, 注意一下选项 
			 LWS_WITH_STATIC = ON
			 LWS_WITH_SHARED = OFF
			 LWS_WITH_SSL = OFF (SSL Support)			  
			 LWS_WITHOUT_EXTENSIONS = ON
 			 LWS_WITHOUT_DAEMONIZE = ON
			 
	cocos2d-x-external\src\curl  工程是单独的 
  
  
  
7 报错 Windows Kits\10\Include\10.0.26100.0\ucrt\corecrt_malloc.h(58,24): error C2485: “__declspec(常数)”不是可识别的扩展属性
1>(编译源文件“../../../../src/libressl/crypto/aes/aes_core.c”)
	
	加上编译宏  _CRT_SUPPRESS_RESTRICT

编译  crypto 库时报这了这个错 
7.1  用的是 glew 静态库，所以在用的工程(cocos2d.lib)里加上编译宏  GLEW_STATIC
	用的是 openal 静态库，所以加上 AL_LIBTYPE_STATIC
	用的是 curl 静态库，所以加上 CURL_STATICLIB
  
  

9.
coco_game 工程链接,添加 Wldap32.lib . Windows 系统原生提供了 LDAP 相关的库文件 

10. 编译 tolua 工程,cmake 先生成工程
	D:\git_work\cocos2d-x\external\lua\tolua
 
 
 
11. luasocket 报错,如果不用这个模块，可注释相关代码.
		在 cocos2d-x\external\lua\luasocket 的代码 windows 下编译不通过.
		下载源码 https://github.com/lunarmodules/luasocket 当前已保存在 coco_game\coco2d-x-more\luasocket
	配合 ext_luasocket.lib 使用 
 


8.修改链接的库路径
 
 compat.lib

最后 cocos2dx 引入库
 {
 
 
alsoft.fmt.lib
alsoft.common.lib
OpenAL32.lib
avrt.lib
mpg123.lib
libogg.lib
libvorbis_static.lib
libvorbisfile_static.lib
cocos2d.lib
luacocos2d.lib
lua51.lib
luasocket.lib
luasocket_mime.lib
ext_tolua.lib
ext_luasocket.lib
LinearMath.lib
external.lib
box2d.lib
chipmunk.lib
freetype.lib
ext_recast.lib
jpeg.lib
webp.lib
websockets_static.lib
ssl.lib
crypto.lib
uv_a.lib
ext_tinyxml2.lib
ext_xxhash.lib
ext_xxtea.lib
ext_clipper.lib
ext_edtaa3func.lib
ext_convertUTF.lib
ext_poly2tri.lib
ext_md5.lib
libcurl.lib
png.lib
glew32sd.lib
iconv.lib
glfw3.lib
zlib.lib
ext_unzip.lib
ws2_32.lib
Crypt32.lib
userenv.lib
psapi.lib
winmm.lib
Version.lib
Iphlpapi.lib
opengl32.lib
kernel32.lib
user32.lib
gdi32.lib
winspool.lib
ole32.lib
oleaut32.lib
uuid.lib
comdlg32.lib
advapi32.lib
BulletDynamics.lib
BulletSoftBody.lib
BulletCollision.lib
Wldap32.lib
shlwapi.lib
shell32.lib

 }
 
 
//脚本创建工程默认链接的库
{
lib\Debug\luacocos2d.lib
lib\Debug\cocos2d.lib
lib\Debug\external.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\Box2D\prebuilt\win32\debug\libbox2d.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\chipmunk\prebuilt\win32\debug-lib\libchipmunk.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\freetype2\prebuilt\win32\freetype.lib
lib\Debug\ext_recast.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\bullet\prebuilt\win32\debug\libbullet.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\jpeg\prebuilt\win32\libjpeg.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\webp\prebuilt\win32\libwebp.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\websockets\prebuilt\win32\websockets.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\openssl\prebuilt\win32\libssl.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\openssl\prebuilt\win32\libcrypto.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\uv\prebuilt\win32\uv_a.lib
lib\Debug\ext_tinyxml2.lib
lib\Debug\ext_xxhash.lib
lib\Debug\ext_xxtea.lib
lib\Debug\ext_clipper.lib
lib\Debug\ext_edtaa3func.lib
lib\Debug\ext_convertUTF.lib
lib\Debug\ext_poly2tri.lib
lib\Debug\ext_md5.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\curl\prebuilt\win32\libcurl.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\png\prebuilt\win32\libpng.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\gles\prebuilt\glew32.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\icon\prebuilt\libiconv.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\MP3Decoder\prebuilt\libmpg123.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\OggDecoder\prebuilt\libogg.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\OggDecoder\prebuilt\libvorbis.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\OggDecoder\prebuilt\libvorbisfile.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\win32-specific\OpenalSoft\prebuilt\OpenAL32.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\glfw3\prebuilt\win32\glfw3.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\zlib\..\win32-specific\zlib\prebuilt\libzlib.lib
D:\git_work\coco_game\mygame\frameworks\cocos2d-x\external\lua\luajit\prebuilt\win32\lua51.lib
lib\Debug\ext_tolua.lib
lib\Debug\ext_luasocket.lib

lib\Debug\ext_unzip.lib
ws2_32.lib
userenv.lib
psapi.lib
winmm.lib
Version.lib
Iphlpapi.lib
opengl32.lib

kernel32.lib
user32.lib
gdi32.lib
winspool.lib
shell32.lib
ole32.lib
oleaut32.lib
uuid.lib
comdlg32.lib
advapi32.lib
} 
 








 
 
 

