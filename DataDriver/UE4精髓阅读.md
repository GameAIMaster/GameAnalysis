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
2. 无论是声明还是定义都能够附加一份metadata，应该在基类中存储
3. 方便统一添加一些额外的方法，例如打印日志。