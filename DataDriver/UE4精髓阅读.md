Actor/component

宇宙拥有两个固有的状态属性：能量与信息。能量通过上帝粒子--希格斯玻色子--固定形成世界万物。既然引擎需要描述整个游戏世界，这两个基本的概念需要载体。Actor是游戏世界被玩家直接感知的，不仅仅是图像声音，还有整个游戏的运行规则。所以不是所有Actor都具有transform属性，故需要位置需实例化scenecomponent。根据逻辑概念中的同一性，每个Actor只包含一个位置组件，因为一个实体不能既在这又在那。Actor之间的父子关系通常通过位置关系决定，所以只有scene component可以嵌套，childactorcomponent则不能。既然已实体划分，就不会将车划分成轮子，地盘，引擎等各个基础功能，而是看成一个整体，通过Script编写整体逻辑，而不是每个部分都有通用的整体功能，这样虽然增加了灵活性，但是多费了无用功。

Level：将世界划分成一个个Level，类似于将国家划分成省份，方便管理，是一种抽象概念。每个Level有自己的省长管理--AlevelScriptActor.有自己的文化及规则--Aworldsetting，当然少不了人才与土地住房--Actors。







在阅读完UE4引擎的整体框架之后，将从Object进行深入分析，它是引擎的土壤。从它开始逐渐构建整个森林生态。

Object提供了GC，反射，序列化，编辑器支持...应该从哪个开始？

“GC不是，因为大不了我可以直接new/delete或者智能指针引用技术，毕竟别的很多引擎也都是这么干的。序列化也不是，大不了每个子类里手写数据排布，麻烦是麻烦，但是功能上也是可以实现的。编辑器支持，默认类对象，统计等都是之后额外附加的功能了。那你说反射为何是必需的？大多数游戏引擎用的C++没有反射，不也都用得好好的？确实也如此，不利用反射的那些功能，不动态根据类型创建对象，不遍历属性成员，不根据名字调用函数，**大不了手写绕一下，没有过不去的坎**。但是既然上文已经论述了一个统一Object模型的好处，那么如果在Object身上不加上反射，无疑就像是砍掉了Object的一双翅膀，让它只能在地上行走，而不能在更宽阔空间内发挥威力。还有另一个方面的考虑是，反射作为底层的系统，如果实现完善了，也可以大大裨益其他系统的实现，比如**有了反射，实现序列化起来就很方便**了；有没有反射，也关系到**GC实现时的方案选择，完全是两种套路**。 ”--《inside UE4》

反射是在运行时获取类型信息的系统，动态创建类对象等。但是其实反射是在类型系统之后实现的附加功能。类型系统是代表的是class类提供了object的类型信息（类名、属性类型、属性名、方法信息等等数据）；反射则是提供动态创建，动态调用函数的功能（算法）。

C++RTTI 提供type_id以及提供dynamic_cast

~~~c++
const std::type_info& info= typeid(MyClass);
class type_info
{
public:
    type_info(type_info const&) = delete;
    type_info& operator=(type_info const&) = delete;
    size_t hash_code() const throw();
    bool operator==(type_info const& _Other) const throw();
    bool before(type_info const& _Other) const throw();
    char const* name() const throw();   
}
~~~



C++使用模板实现反射rttr

~~~c++
#include <rttr/registration>
using namespace rttr;
struct MyStruct
{
    MyStruct(){};
    void func(double){};
    int data;
}
RTTR_REGISTRATION
{
	registration::class_<MyStruct>("MyStryct")
		.constructor<>()
		.property("data", &MyStruct::data)
		.method("func", &MyStruct::func)
}
~~~

工具生成代码

Qt利用基于mco（meta object compiler)实现，分析C++源文件，识别一些特殊的宏：

Q_OBJECT、Q_PROPER、Q_INVOKABLE 生成相应的moc文件，再一起编译链接。UE中的UHT也是如此，比较友好的把元数据和代码关联起来，即便生成的代码复杂也对用户隐藏起来

![preview](https://pic4.zhimg.com/v2-59142324b676346608c872022097d5c3_r.jpg) 

UHT收集完成数据后，需要将数据保存到数据结构中，结构如上图所示。途中有个错误，UScriptStruct中只能包含UProperty。

为什么要使用基类UField？

1. 为了统一所有的数据类型，可以讲所有数据存入到一个数组，可以遍历也可以保存属性各个结构的顺序。
2. 无论是声明还是定义都能够附加一份metadata，应该在基类中存储（类中包含了许多操作metadata的的方法）
3. 方便统一添加一些额外的方法，例如打印日志。



### **C++代码需要预处理，编译，汇编，链接 UE4 的代码需要经过生成，收集，注册，链接**

#### 生成

UHT类似于语法分析器，具体没有详细了解，类似做过的功能则是语法分析器，通过正则表达式描述语法，收集足够的数据信息。

生成代码中DECLARE_CLASS 是最重要的一个声明 ，GetStaticClass是常用的方法，它会调用GetPrivateStaticClass，里面会传入package名，Package的概念涉及到后续Object的组织方式，目前可以简单理解为一个大的Object包含了其他子Object。

生成的代码中包含了收集、注册、链接等流程。一是各种Z_辅助方法用来构造出各种UClass*等对象；另一部分是都包含着一两个static对象用来在程序启动的时候驱动登记，继而调用到前者的Z_方法，最终完成注册。

#### 收集

```c++
UClass* UMyClass::GetPrivateStaticClass(const TCHAR* Package)
UProperty* NewProp_Score = new(EC_InternalUseOnlyConstructor, OuterClass, TEXT("Score"), RF_Public|RF_Transient|RF_MarkAsNative) UFloatProperty(CPP_PROPERTY_BASE(Score, UMyClass), 0x0010000000000004);//添加属性
//添加方法
OuterClass->LinkChild(Z_Construct_UFunction_UMyClass_CallableFunc());
OuterClass->AddFunctionToFunctionMapWithOverriddenName (Z_Construct_UFunction_UMyClass_CallableFunc(), "CallableFunc"); // 774395847
```

#### 注册

一一种直接的方式是在程序启动后手动的一个个调用注册函数 ，另一种是自动注冊模式只能在独立的地址空间才能有效，如果该文件被静态链接且没有被引用到的话则很可能会绕过static的初始化。不过UE因为都是dll动态链接，且没有出现静态lib再引用Lib，然后又不引用文件的情况出现，所以避免了该问题。或者你也可以找个地方强制的去include一下来触发static初始化。 

```C++
IMPLEMENT_CLASS(UMyInterface, 4286549343);  //注册类
//什么时候延迟注册
//UE Static 自动注册模式
template <typename TClass>
struct TClassCompiledInDefer : public FFieldCompiledInInfo
{
	TClassCompiledInDefer(const TCHAR* InName, SIZE_T InClassSize, uint32 InCrc)
	: FFieldCompiledInInfo(InClassSize, InCrc)
	{
		UClassCompiledInDefer(this, InName, InClassSize, InCrc);
	}
	virtual UClass* Register() const override
	{
		return TClass::StaticClass();
	}
};

static TClassCompiledInDefer<TClass> AutoInitialize##TClass(TEXT(#TClass), sizeof(TClass), TClassCrc); 
//或者
struct FCompiledInDefer
{
	FCompiledInDefer(class UClass *(*InRegister)(), class UClass *(*InStaticClass)(), const TCHAR* Name, bool bDynamic, const TCHAR* DynamicPackageName = nullptr, const TCHAR* DynamicPathName = nullptr, void (*InInitSearchableValues)(TMap<FName, FName>&) = nullptr)
	{
		if (bDynamic)
		{
			GetConvertedDynamicPackageNameToTypeName().Add(FName(DynamicPackageName), FName(Name));
		}
		UObjectCompiledInDefer(InRegister, InStaticClass, Name, bDynamic, DynamicPathName, InInitSearchableValues);
	}
};
static FCompiledInDefer Z_CompiledInDefer_UClass_UMyClass(Z_Construct_UClass_UMyClass, &UMyClass::StaticClass, TEXT("UMyClass"), false, nullptr, nullptr, nullptr);

```

#### 链接

```c++
OuterClass->StaticLink();
```



### Subsystem子系统

What：Subsystems是一套可以定义自动实例化和释放类的框架。

对比示例：manager类，创建manager时都只是复制一套单例代码，修改类名，自己手动加创建和释放的代码。当提出子系统的概念就可以实现自动加载释放，每个子系统像是有钩子似的挂在需要依附的类上。

![preview](https://picb.zhimg.com/v2-c4e2d200db7386c086cb59ab49a48449_r.jpg) 

其中菱形代表包含引用，三角形代表继承

利用到了反射收集所有的父类下的子系统，然后添加到Map结构中进行挂载，父类释放时遍历Map释放其引用， 下一帧则会被GC系统释放

Why：为什么要使用Subsystem，我们已经有component来组装Actor了，组件多和功能挂钩，能够进行移植。Subsystem是用来分层逻辑，我们知道playercontroller可以用来编写玩家控制逻辑，但是随着游戏中玩家组件越多控制的逻辑将会越混乱，引入子系统则能够梳理业务逻辑，还能自动控制声明周期，岂不乐乎。

关键的倒不是如何继续添加一个，而是在吸收了这些架构知识营养后，读者们可以在自己的游戏结构里灵活应用上对象的反射和事件注册等知识。 