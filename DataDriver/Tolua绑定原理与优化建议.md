# Tolua绑定原理与优化建议

>  知其然也要知其所以然

​	为了大家能够在我们的Lua开发过程中写出更加高效的代码，有必要了解一下我们的lua binding层做了些什么。本文简单介绍了Tolua的工作原理并提供了一些优化建议。



## 为什么使用Lua

> IOS是原罪



### 因为是解释型语言？

​	好多人都说Lua能热更新，是因为它是解释型语言，不用编译，在运行时能动态解释Lua代码并运行。Lua确实是解释性脚本语言，但是不是因为是解释型才能进行热更新。即使使用C++这种编译语言，也能进行热更新，将动态链接库进行更新就是，然后动态加载动态链接库获取更新的函数地址即可。 而且，还有一点，C#也是一种解释型语言，C#代码会被编译成IL，IL解释成机器码的过程可以在运行之前进行也能在运行时进行。如果在运行时进行解释，那么和Lua不就一样了吗，为啥C#不能进行热更新呢？



### JIT对IL进行解释执行的原理

​	JIT对IL如何在运行时进行解释并执行的，大致过程为：将IL解释为所在平台的机器码，开辟一段内存空间，要求这段内存空间可读、可写、**可执行**，然后把解释出的机器码放入，修改CPU中的指令指针寄存器中的地址，让CPU执行之前解释出来的机器码。

> * AOT：Ahead Of Time，指在运行前编译，比如普通的静态编译。 
> * JIT：Just In Time，指在运行时编译，边运行边编译，动态语言。



### IOS限制了什么？

​	IOS不允许获取具有可执行权限的内存空间，这就直接要求JIT要以full AOT模式，这种模式会在生成之前把IL直接翻译成机器码而不是在运行期间，进行了这种操作C#从某种角度来说和C++一样，成为了编译型语言，失去了运行时解释的功能。



### Lua的解释执行

​	如果Lua的解释执行原理和C#相同，肯定也不能在IOS平台上运行时解释执行。Lua是使用C编写的脚本语言，它在运行时读入Lua编写的代码，在解释Lua字节码（opcode）时不是翻译为机器码，而是使用C代码进行解释，不用开辟特殊的内存空间，也不会有新代码在执行，执行的是Lua的虚拟机，用C写出来的虚拟机，这和C#的机制是完全不同的，因为Lua是基于C的脚本语言。



## 前置知识

​	快速了解Lua的一些语法和特性，最好自己有时间的时候去看下书系统的了解基本语法，如果遇到不清楚或者模糊的问题去查下手册。

> 《Programming in Lua》
>
> [云风翻译的Lua 5.1 参考手册](https://www.codingnow.com/2000/download/lua_manual.html)

​	

###  一些语法特性

* boolean类型  0和空字符串为真  false和nil为假 

* #取长度操作符，可以用来获取数组、字符串的长度。关于table的实现看《Lua编写指南》。

  > 取长度操作符写作一元操作 `#`。 字符串的长度是它的字节数（就是以一个字符一个字节计算的字符串长度）。
  >
  > table `t` 的长度被定义成一个整数下标 `n` 。 它满足 `t[n]` 不是 **nil** 而 `t[n+1]` 为 **nil**； 此外，如果 `t[1]` 为 **nil** ，`n` 就可能是零。 对于常规的数组，里面从 1 到 `n` 放着一些非空的值的时候， 它的长度就精确的为 `n`，即最后一个值的下标。 如果数组有一个“空洞” （就是说，**nil** 值被夹在非空值之间）， 那么 `#t` 可能是指向任何一个是**nil** 值的前一个位置的下标 （就是说，任何一个 **nil** 值都有可能被当成数组的结束）。

* 区别a.x和a[x]， 语法糖 .访问table元素，点后是字符串,相当于a["x"]，而a[x]是用x的值作为索引 

* 数组以1为起始索引 

* 相等性测试：类型不同不想等， nil只跟自身相等 

* table的构造表达式：列表式和记录式，还有更通用的中括号表示["xx"] = yy 

* local变量的访问速度比全局变量要快 

* 关于pairs和ipairs的区别。ipairs就是固定地从key值1开始，下次key累加1进行遍历，如果key对应的value不存在，就停止遍历。pairs按照key顺序遍历，而不是光遍历数组部分，其中散列表根据存储顺序。

* 函数返回多个结果只有当处于表达式的最后一个时才有效 

* 表达式... 变长参数，遍历访问，通过select访问变长参数，select(”#“,...)返回参数个数

* closure 闭合函数，闭包，函数以及函数可以访问的upvalue组成一个闭包. 

*  pcall 捕获error, pcallx提供异常发生时的处理函数

* 协程（coroutine），resume 和 yield处理，yield的参数是resume的返回值，第二次resume的参数是前一次yield的返回值 

* table.concat 连接表中的字符串 

* 关于冒号语法

  > 冒号语法可以用来定义方法， 就是说，函数可以有一个隐式的形参 `self`。 因此，如下写法：
  >
  > ```
  >      function t.a.b.c:f (params) body end
  > ```
  >
  > 是这样一种写法的语法糖：
  >
  > ```
  >      t.a.b.c.f = function (self, params) body end
  > ```

* table.sort只能排序数组,遍历表的顺序不可控制，但是可以控制遍历数组的顺序，方法是pairs遍历然后通过a[#a+1] = n 构造数组，然后在ipairs遍历数组。 

* debug.traceback() 输出调用栈信息 

* C和lua是通过栈来进行交互，每个lua_State提供独立的Lua运行状态。

* lua_pushstring是生成一个字符串副本，所以是安全的。lua_tostring不要在C函数返回后还使用字符串指针 



### 元表

> Lua 中的每个值都可以用一个 *metatable*。 这个 *metatable* 就是一个原始的 Lua table ， 它用来定义原始值在特定操作下的行为。 你可以通过在 metatable 中的特定域设一些值来改变拥有这个 metatable 的值 的指定操作之行为。 举例来说，当一个非数字的值作加法操作的时候， Lua 会检查它的 metatable 中 `"__add"` 域中的是否有一个函数。 如果有这么一个函数的话，Lua 调用这个函数来执行一次加法。 
>
> 每个 table 和 userdata 拥有独立的 metatable （当然多个 table 和 userdata 可以共享一个相同的表作它们的 metatable）； 其它所有类型的值，每种类型都分别共享唯一的一个 metatable。 因此，所有的数字一起只有一个 metatable ，所有的字符串也是，等等。 



### userdata

> *userdata* 类型用来将任意 C 数据保存在 Lua 变量中。 这个类型相当于一块原生的内存，除了赋值和相同性判断，Lua 没有为之预定义任何操作。 然而，通过使用 *metatable （元表）* ，程序员可以为 userdata 自定义一组操作 （参见 [§2.8](https://www.codingnow.com/2000/download/lua_manual.html#2.8)）。 userdata 不能在 Lua 中创建出来，也不能在 Lua 中修改。这样的操作只能通过 C API。 这一点保证了宿主程序完全掌管其中的数据。 



### 环境

> 类型为 thread ，function ，以及 userdata 的对象，除了 metatable 外还可以用另外一个与之关联的被称作 它们的环境的一个表， 像 metatable 一样，环境也是一个常规的 table ，多个对象可以共享 同一个环境。
>
> userdata 的环境在 Lua 中没有意义。 这个东西只是为了在程序员想把一个表关联到一个 userdata 上时提供便利。
>
> 关联在 C 函数上的环境可以直接被 C 代码访问（参见 [§3.3](https://www.codingnow.com/2000/download/lua_manual.html#3.3)）。 它们会作为这个 C 函数中创建的其它函数的缺省环境。
>
> 关联在 Lua 函数上的环境用来接管在函数内对全局变量（参见 [§2.3](https://www.codingnow.com/2000/download/lua_manual.html#2.3)）的所有访问。 它们也会作为这个函数内创建的其它函数的缺省环境。



### 注册表

> Lua 提供了一个注册表，这是一个预定义出来的表，可以用来保存任何 C 代码想保存的 Lua 值。 这个表可以用伪索引 `LUA_REGISTRYINDEX` 来定位。 任何 C 库都可以在这张表里保存数据，为了防止冲突，你需要特别小心的选择键名。 一般的用法是，你可以用一个包含你的库名的字符串做为键名，或者可以取你自己 C 代码 中的一个地址，以 light userdata 的形式做键。 



## Tolua原理

lua binding要解决的问题：

> * 对C#和Lua层提供合理的交互封装
> * 维护C#和Lua的对象关联
> * 解决托管堆和非托管堆栈的对象交互
> * C#<=======>Tolua(C)<==========>lua(C)

​	tolua#是Unity静态绑定lua的一个解决方案，它通过C#提供的反射信息分析代码并生成包装的类。它是一个用来简化在C#中集成lua的插件，可以自动生成用于在lua中访问Unity的绑定代码，并把C#中的常量、变量、函数、属性、类以及枚举暴露给lua。它是从cstolua衍变而来。从它的名字可以看出，它是集成了原来的tolua代码通过二次封装写了一个C#与tolua(c)的一个中间层。 



### Lua与C#的对象关联（ObjectTranslator）

----

* LuaObjectPool提供一个C#对象池，index记录对象在池中的位置(Key)。该池为一个回收链表结构，回收时头指针指向空闲节点头。 
* objectsBackMap保存object-index的键值对，提供查询。其中index作为userdata返回给lua。
* 只有当Lua中对象gc时才会删除objectpool里的强引用对象。
* UBOX表：用来在注册表中（LUA_RIDX_UBOX）保存已经Add过的userdata，用来缓存index-userdata，获取已存在的userdata，弱值引用表。 

> * C#对象返给Lua:
>
> PushUserObject --->PushUserData---->如果已经缓存Object,直接从UBOX中取出push，tolua_pushudata，否则缓存Objec，同时tolua_pushnewudata，设置Ubox。
>
> * lua传递对象给C#:
>
> ToObject--->tolua_rawnetobj获取udata对应的Index（如果是table获取&vptr存储的userdata） ---> ObjectTranslator的GetObject。



### 对C#层的封装

----

> 提供lua去访问C#层的对象的能力



#### Ulua的实现（动态反射）

​	ObjectTranslator中保存MetaFunctions，MetaFunctions提供luaNet类中基本类的一些操作，包括获取方法等，ObjectTranslator的createClassMetatable等通过设置一系列元方法，MetaFunctions可以保证直接使用lua中获取到的类对象实例来直接获取属性和方法。 从ObjectTranslator中取得实际object后调用MetaFunctions的getMember通过反射获取相应C#对象的方法。 

> 优点：动态反射，相当于随时可以使用c#中的类，而且不需要额外处理。
>
> 缺点：反射获取消耗过大(CPU、GC的消耗)、il2cpp时可能产生裁剪，需要link.xml



#### Tolua的实现（静态绑定）

> 目的：实现C#中类、成员方法变量、面向对象
>
> 基本流程：
>
> 1. 配置CustomSettings.cs
> 2. Lua--->Generate All(生成Wrap文件、生成LuaBinder.cs)
> 3. 初始化LuaState后LuaBinder.**Bind**进行绑定，会调用所有Wrap类的Register函数进行绑定。



##### BeginModule/EndModule

* tolua_beginmodule  新建一个table 元表为自己，设置.name为全路径名 
* __index设置为module_index_event函数 ，PreLoad机制，获取.name的space路径，查找AddPreLoad设置的package.preload加载函数



##### BeginClass/EndClass 

* 设置type与关联的TypeIndex到metaMap中

* tolua_beginclass 将注册表中创建新table作为class元表，同时设置父类的table为元表的元表

* 元方法__call设置为class_new_event  构造函数调用“New” 返回对象

* 元方法__index设置为class_index_event 遵循以下查找循序

  > 1. 先直接在userdata的环境表peer查找
  > 2. 在peer的gettable找对应的getfunction,并调用这个function 
  > 3. 如果peer有元表，则在peer的元表重复1，2，3步骤
  > 4. 在userdata的元表umt找，如果name是一个number类型，说明userdata可能是一个Array类型，则在value = umt[".geti"]找，如果value为function(luafunction或者cfunction)则call这个function，function的返回值赋值给userdata.name。如果name不是number类型，则查找对应的值。
  > 5. 在userdata的元表umt的gettable找对应的getfunction,并调用这个function。
  > 6. 如果umt有元表，则在umt的元表重复4，5，6步骤
  > 7. 找不到报错tolua_endclass时将元表设置进class表 

* tolua_endclass时将元表设置进class表 

* 设置__gc 用于回收ObjectTranslator中的index



##### RegVar 

* tolua_variable 设置get/set表，对应valueName-function 
* initget/initset 设置返回参数对应table的get/set表用于设置



##### RegFunction

* tolua_function --> tolua_pushcfunction用一个bool和function作为upvalue



##### metaMap/typeMap：

* 保存类型元表在注册表中的reference和类型type的键值对
* 创建对象时调用对应TypeTraits的GetLuaReference，tolua_pushnewudata时设置为userdata的元表



##### peer表：

* 提供一种lua中可以扩展C#封装对象的机制，元表实现类的方法，peer表保存实例的信息，通过设置环境表实现。 
* 参照例子17，实现继承wrap导出类。



### 对Lua的封装

----

> 提供C#层访问lua的能力



##### LuaBaseRef（LuaFunction、LuaTable、LuaThread）

* lua中对象的C#封装，引用计数，luaState.Collect回收。
* toluaL_ref  注册对象到注册表，同时将(func/table)-reference设置到FixedMap，用于反向查找。
* C#中保留lua的对象不释放，会有内存泄露，注意dispose



##### GetFunction

* funcMap 保存名称-弱引用（WeakReference）
* PushLuaFunction 将function压倒栈顶，支持.的层级目录，会PushLuaTable压栈 



##### TypeTraits

* 类型特征给输入参数做类型检查用，基本类型、枚举wrap、委托



##### StackTraits

* 类型堆栈操作 提供Push、check、to操作 



##### 关于delegate

* 参考例子11
* XXX_Event继承LuaDelegate 实现回调时Call或者CallSelf函数 
* DelegateFactory.Init(); DelegateFactory.dict.Add与 DelegateTraits的init，初始化Create函数 
* wrap类中的委托成员变量，设置时会调用CheckDelegate判断是否是C#的委托还是luafunction，是否需要创建委托 
* 委托的引用直接绑定LuaFunction不在进行额外计数 
* function中的引用是为了兼容委托自动创建和手动获取，委托是在移除的时候垃圾回收删除



##### 关于int64

* number ：double 
* userdata实现，兼容number和string表示 
* _int64eq 判断两个userdata代表的值是否相等，lua类型判断，如果类型不同直接不相等，所以不能做类型隐式转换
* 如果需要判断相等需要使用equals 
* 作为tabel的可以的时候会导致同值代表不同的key 



## 优化建议

> 核心优化原则：尽量减少Lua和C#之间的交互
>
> 勿以善小而不为，勿以恶小而为之！



### 减少频繁的对象创建与查询

```lua
 local transPos = UIObj.CloudRoot.transform:GetChild(currentCloudIndex).transform.localPosition
```
1. 获取transform对象，创建对应的userdata，设置元表，缓存对象，返回对象
2. 调用child方法，创建对应的userdata，设置元表，缓存对象，返回对象
3. 获取child的transform对象，创建对应的userdata，设置元表，缓存对象，返回对象
4. 调用localPosition方法，返回Vector3，转换为lua中的Vector3



```lua
 LuaData_PlayerData.Instance.WorldId = PayModule.TmpWorldId;
```
频繁调用Instance去获得单例对象



* 使用静态辅助函数去代替transform这类临时对象的频繁创建
* 对于频繁调用的单例类对象，统一初始化全局对象，不要再去获取
* 访问C#绑定的容器类尤为要注意。



### 减少特殊类型的传递

>  托管堆和非托管间传递、对象引用的管理

* 对象类型（自定义、Vector3等）

* string类型(非托管--->托管)

  ```c#
  public static string lua_ptrtostring(IntPtr str, int len)
          {
              string ss = Marshal.PtrToStringAnsi(str, len);
  
              if (ss == null)
              {
                  byte[] buffer = new byte[len];
                  Marshal.Copy(str, buffer, 0, len);
                  return Encoding.UTF8.GetString(buffer);
              }
  
              return ss;
          }
  ```



- 使用静态辅助函数去代替Vector3这类对象的传递，或者利用Out参数。

  ```lua
  Vector3 GetPos(GameObject obj)
  --可以写成
  void GetPos(GameObject obj, out float x, out float y, out float z)
  ```

- 考虑对频繁调用的string参数进行优化（字符串池之类的）

  

### 频繁调用的函数，参数的数量要控制

​	无论是lua的pushint/checkint，还是c到c#的参数传递，参数转换都是最主要的消耗，而且是逐个参数进行的，因此，lua调用c#的性能，除了跟参数类型相关外，也跟参数个数有很大关系。一般而言，频繁调用的函数不要超过4个参数，而动辄十几个参数的函数如果频繁调用，你会看到很明显的性能下降，手机上可能一帧调用数百次就可以看到10ms级别的时间



### 减少C#对Lua接口的调用

​	性能很差，能直接在lua中调用的就直接调用，如果需要高性能处理可以写到c# 中但是不要在c#中驱动调用lua接口。能各自实现的就不要互相访问，例如禁止lua使用C#的定时器。





