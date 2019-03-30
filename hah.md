# 第5章 构造, 析构, 拷贝语意学 #

## "无继承"情况下的对象构造 ##

### C struct的Point声明 ###
```C++
/*
	C++称这是一种所谓的Plain Ol' Data声明形式. 如果以C++来编译, 观念上, 编译器会为
	Poit声明一个trivial default construct, 一个trivial destructor, 一个
	trivial copy construct, 以及一个trivial copy assignment operator.
	但实际上, 编译器会分析这个声明, 并为它贴上Plain Ol' Data标签
*/

typedef struct {
	float x, y, z;
} Point;

/*
	观念上Point的trivial constructor和destructor都会被产生并被调用, constructor
	在程序起始处被调用 而destructor在程序的exit()处被调用. 然而, 事实上那些
	trivial members要不是没被定义, 就是没被调用, 程序的行为一如他在C中的表现一样

	那么在C和C++中有什么区别?	g++编译器支持global的重复定义
	(1) 在C中, global被视为一个"临时性的定义", 因为他没有显式的初始化操作. 一个"临时性的定义"
	可以在程序中发生多次. 那些实例会被连接器折叠起来, 只留下单独一个实例, 被放在程序
	data segment中一个"特别留给未初始化的globalobjects使用"的空间. 由于历史的原因,
	这块空间被称为BSS
	(2) 在C++中不支持"临时性的定义", 这是因为class构造行为的隐式应用的缘故. 虽然大家公认这个语言
	可以判断一个class objects或是一个Plain Ol' Data, 但似乎没有必要搞这么复杂.  因此, 
	global在C++中被完全定义(它会阻止第二个或更多的定义). C和C++的一个差异就在于,
	BSS data segment在C++中相对定位不重要. C++的所有全局对象都被以"初始化过的数据来对待"
*/

Point global;

Point foobar() {
	// 既没有构造也没有被析构
	Point local;
	// 没有default constructor实施于new运算符所传回的Point object身上
	Point *heap = new Point;
	// 如果local曾被初始化过, 一切就都没问题, 否则会产生编译警告
	// 观念上, 这样的指定操作会触发trivial copy assignment operator做拷贝搬运操作
	// 然而实际上该object是一个Plain Ol' Data, 所以赋值操作将只是想C那样的纯粹位搬移操作
	*heap = local;
	// 观念上, 会触发trivial destructor, 但实际上destructor要不是没有产生就是没有被调用
	delete heap;
	// 观念上, 会触发trivial copy constructor, 不过实际上return操作只是一个简单的位拷贝操作
	// 因为对象是个Plain Ol' Data
	return local;

}
*/
```

### 抽象数据类型 ###

```C++
/*
	提供了完整的封装性, 但没有提供任何virtual function
	这个经过封装的Point class, 其大小并没有改变, 还是三个连续的float
*/ 
class Point {
public:
	// 定义了一个构造函数
	Point(float x = 0.0, float y = 0.0, float z = 0.0)
		: _x(x), _y(y),  _z(z) {}
		// 除此之外, 没有定义其他成员函数
private:
	float _x, _y, _z;
};

// 现在有了default construct作用于其上. 由于global被定义在全局范畴中,
// 其初始化操作在程序启动时进行(6.1节对此有详细讨论)
Point global;

Point foobar() {
	/*
	 	local的定义会被附上default Point constructor的inline expansion:
	 	Point local;
	 	local._x = 0.0, local._y = 0.0, local.z = 0.0;
	*/
	Point local;
	/*
	 	现在则被附加一个"对default Point constructor的有条件调用操作":
	 	Point *heap = __new(sizeof(Point));
	 	if (heap != 0)
	 		heap->Point::Point();
	 	在条件内才又被编译器进行inline expansion操作
	*/
	Point *heap = new Point;
	// 保持着简单的位拷贝操作
	*heap = local;
	// 并不会导致destructor被调用, 因为没有显式的提供一个destruct函数实例, 但还是会释放空间
	delete heap;
	// return时, 同样保持着简单的位拷贝操作, 并没有拷贝操作
	return local;
}
```

> **总的来说, 观念上, Point class有一个相关的default copy constructor, copy operator和destructor. 然而它们都是无关痛痒的(trivial), 而且编译器实际上更本没有产生它们. 虽然没有产生, 但是像拷贝或者析构操作仍会执行**

### 包含虚函数的Point声明 ###

包含虚函数时, 除了每一个class object多负担一个vptr之外, virtual function的导入也引发编译器对于Point class**产生膨胀作用**(如, **编译器会在构造函数总插入初始化vptr的代码**)
```C++
class Point {
public:
	Point(float x = 0.0, float y = 0.0) : _x(x), _y(y) {}
	virtual float z();
private:
	float _x, _y;
};
```

- **自定义构造函数中会被附加一些代码, 以便将vptr初始化. 这些代码必须附加在任何base class constructors调用之后, 但必须由使用者(程序员)供应的代码之前**
```C++
// C++伪码: 内部膨胀
Point *Poit::Point(Point *this, float x, float y) : _x(x), _y(y) {
	// 设定object的virtual table pointer(vptr)
	this->__vptr_Point = __vtbl_Point;
	
	// 扩展member Initialization list
	this->_x = x;
	this->_y = y;

	// 传回this对象
	return this;
}
```

- 因为需要处理vptr, 所以会合成一个copy constructor和一个copy assignment operator, 而且其操作不再是trivial(但implicit destructor仍然是trivial). **如果一个Point object被初始化或以一个derived class object赋值, <font color=#DC143C>那么以位为基础(bitwise)的操作可能会对vptr带来非法设定(直接拷贝地址, 而不是重新生成一个指向自己virtual table的指针)</font>**
```C++
// C++伪码: copy constructor的内部合成
inline Point *Point::Point(Point *this, const Point &rhs) {
	// 设定object的virtual table pointer(vptr)
	this->__vptr_Point = __vtbl_Point;			

	// 将rhs左边中的连续位拷贝到this对象
	// 或是经由member assignment提供一个member

	return this;
}

// 编译器在优化状态下可能会把object的连续内容拷贝到另一个object身上
// 而不会实现一个精确的"以成员为基础的赋值操作"
```


```C++
// 和前一版本相同
Point global;

Point foobar() {
	// 和前一版相同
	Point local;
	// 和前一版相同
	Point *heap = new Point;
	// 这里可能触发copy assignment operator的合成, 以其调用操作的一个
	// inline expansion(行内扩张), 以this取代heap, 而以rhs取代local
	*heap = local;
	// 和前一版相同
	delete heap;
	// 最具戏剧性的改变在这, 下面讨论
	return local;
}
```

由于copy constructor的出现, foobar很可能被转化为下面:
```C++
Point foobar(Point &__result) {
	Point local;
	local.Point::Point(0.0, 0.0);

	// heap的部分与前面相同
	
	// copy constructor的应用
	__result.Point::Point(local);

	// local对象的destructor将在这里执行
	// 调用Point定义的destructor
	// local.Point::~Point();
	
	return;
}
```

如果支持named return value(NRV)优化, 会进一步被转化(少构造一个对象):
```C++
Point foobar(Point &__result) {
	__result.Point::Point(0.0, 0.0);

	// heap的部分与前面相同...
	
	return;
}
```

> 一般而言, 如果设计之中有许多函数都需要**以值方式传回**一个local class object, 那么提供一个copy construct就比较合理, 甚至即使default memberwise语意已经足够. 它的出现会触发NRV优化. 然而就像前面例子展现的那样, NRV优化后**将不再需要调用copy constructor**, 因为运算结果已经被直接计算域"将被传回的object"内了
> 就是NRV会少构造一个对象

## 5.2 继承体系下的对象构造 ##

当定义一个object`T object`, 如果T有一个construct(不论是有user提供或是由编译器合成的), 它会被调用. 因为constructor可能内含大量的隐藏码, 因为编译器会扩充每一个constructor, 扩充程度视class T的继承体系而定. 一般而言编译器所做的扩充操作大约如下:
(1) 记录在member initialization list中的data members初始化操作或被放进constructor的函数本体, 并以members的声明顺序为顺序
(2) 如果有一个member并没有出现在member initialization list之中, 但他有一个default constructor, 那么该default constructor必须被调用
(3) 在那之前, 如果class object有virtualtable pointer(s), 它(们)必须被设定初值, 指向适当的virtual table(s)
(4) 在那之前, 所有上一层的base class constructors必须被调用, 以base class的声明顺序为顺序(与member initialization list中的顺序没关联):
　　如果base class被列于member initialization list中, 那么任何显示指定的参数都应该传递过去.
　　如果base class没有被列于member initialization list中, 而它有default constructor(或default memberwise copy constructor), 那么久调用之
　　如果base class是多重继承下的第二或后继base class, 那么this指针必须有所调整
(5) 在那之前, 所有的virtual base class constructors必须被调用, 从左至右, 从最深到最浅:
　　如果class被列于member initialization list中, 那么如果有任何显示指定的参数, 都应该传递过去. 若没有被列于list之中, 而class有一个default constructor, 易应该调用之
　　此外, class中的每一个virtual base class subobject的偏移位置(offset)必须在执行期可被存取
　　如果class object 是最底层(most-derived)的class, 其constructors可能被调用; 某些用以支持这一行为的机制必须被放进来

```C++
class Point {
public:
	Point(float x = 0.0, float y = 0.0);
	Point(const Point&);
	Point& operator=(const Point&);

	virtual ~Point();
	virtual float z() { return 0.0 }
protected:
	float _x, _y;
};

class Line {
	Point _begin, _end;
public:
	Line( float = 0.0, float = 0.0, float = 0.0, float = 0.0 );
	Line( const Point&, const Point& );
	
	draw();
};
```

由于Point声明一个copy constructor, 一个copy operator, 以及一个destructor(本例为virtual), 所以Line clas的implicit copy copy constructor, copyoperator和destructor都将有具体效用(nontrivial)

当写下`Line a;`时, implicit Line destructor会被合成出来(**如果Line派生自Point, 那么合成出来的destruct将会是virtual. 然而由于Line只是内含Point objects而非继承自Point, 所以被合成出来的destruct只是nontrivial而已**). 其中, 它的member class objectors的destructors会被调用(以其构造的相反顺序)

```
// C++伪码: 合成出来的Line destructor
inline void Line::~Line(Line *this) {
	this->_end.Point::~Point();
	this->_begin.Point::~Point();
}
```

当然如果Point destructor是inline函数, 则每一个调用操作会在调用地点被扩展开来. 请注意, 虽然Point destructor是virtual, 但其调用操作(在containing class destructor之中)会被静态地决议出来(resolved statically)

当写下`Line b = a;`时, implicit Line copy constructor会被合成出来, 称为一个inline public member

当写下`a = b`, implicit copy assignment operator会被合成出来, 称为一个inline public member

**自我拷贝问题**, 
```C++
Line *p1 = &a;
Line *p2 = &a;
*p1 = *p2;
```
在一个由编译器合成而来的copy operator中, 上述的重复操作虽然安全却累赘, 因为并没有伴随任何的资源释放行动. 在一个由程序员供应的copy operator中忘记检查自我指派(赋值)操作是否失败, 是新手极易陷入的一项错误, 例如:
```C++
// 使用者供应的copy assignment operator
// 忘记提供一个自我拷贝时的过滤

String& String::operator=(const String &rhs) {
	// 这里需要过滤(在释放之前)
	delete[] str;
	str = new char[strlen(rhs.str) + 1];
}
```

### 虚拟继承 ###

虚继承体系如下

```C++
class Point3d : public virtual Point {
public:
	Point3d(float x = 0.0, float y = 0.0, float z = 0.0) : Point(x, y), _z(z) {}
	Point3d(const Point3 &rhs) : Point(rhs), _z(rhs._z) {}
	~Point3d();
	Point3d& operator=(const Point3d&);
	
	virtual float z() { return _z; }
protected:
	float _z;
};

class Vertex : virtual public Point {...};
class Vertex3d : public Point3d , Public Vertex {...};
class PVertex : public Vertex3d {...};
```

![](https://raw.githubusercontent.com/wu0hgl/note_pic/master/%E6%B7%B1%E5%85%A5%E6%8E%A2%E7%B4%A2C%2B%2B%E5%AF%B9%E8%B1%A1%E6%A8%A1%E5%9E%8B_5-1.png)

传统的"constructor扩充现象"并没有用, 这是因为virtual base class的共享之故, **在此情况下不能确定共享的虚基类由谁来构造**. 传统的扩展: 
```C++
// C++伪代码
// 不合法的constructor扩充, 
Point3d *Point3d::Point3d(Point3d *this, float x, float y, float z) {
	this->Point::Point(x, y);
	this->__vptr_Point3d = _vtbl_Point3d;
	this->__vptr_Point3d_Point = 
		  __vtbl_Point3d_Point;
	this->_z = rhs._z;
	return this;
}
```

**在虚继承下, 一个主要的问题时如何初始化"虚基类子对象", 调用虚基类的构造函数初始化"虚基类子对象"应该在最底层的派生类中进行**

因此, Point3d的构造函数可能被编译器扩充成如下形式: 
```C++
// 在virtual base class情况下的constructor扩充内容
Point3d* Point3d::Point3d(Point3d *this, bool __most_derived, float x, float y, float z) {
	if (__most_dirived != false)
		this->Point::Point(x, y);
	
	this->__vptr_Point3d = __vtbl__Point3d;
	this->__vptr_Point3d__Point = vtbl_Point3d__Point;

	this->_z = rhs._z;
	return this;
}
```

在更深层的继承情况下, 例如Vertex3d, 调用Point3d和Vertex的constructor时, 总会把__most_derived参数设为false, 于是就压制了两个constructor中对Point constructor的调用操作:
```C++
Vertex3d* Vertex3d::Vertex3d(Vertex3d *this, bool __most_derived, float x, float y, float z) {
	if (__most_derived != false) 
		this->Point::Point(x, y);
	
	// 调用上一层base classes设定__most_derived设定__most_derived为false
	this->Point3d::Point3d(flase, x, y, z);
	this->Vertex::Vertex(false, x, y);

	// 设定vptrs, 安插user code

	return this;
}
```

这样, Point3d origin和Vertex3d cv;都能正确的调用Point constructor

> "virtual base class constructors的被调用"有着明确的定义: 只有当一个**完整的**class object被定义出来, 它才会被调用; 如果只是个**某个完整的object的subobject**, 它就不会被调用(没太明白, 上面的Point虚基类的构造调用明显是在非虚基类的构造之前调用的)

> 某些新进的编译器把每个constructor分裂为二, 一个针对完整的object, 另一个针对subobject. "完整object"版无条件的调用virtual base constructor, 设定所有的vptrs等. "subobject"版则不调用virtual base constructs, 也可能不设定vptrs等. constructor的分裂可带来程序速度的提升, 但是使用这个技术的编译器似乎很少, 或者说没有

### vptr初始化语意学 ###

vptr会在构造函数中进行初始化, 关键是vptr应该在构造函数中何时执行初始化. 考虑这个问题是因为: 如果构造函数中调用虚函数, 那么vptr的初始化时机可能会使得程序产生不一致的表现

当定义一个PVertex object时, constructors的调用顺序如下:
```C++
Point(x, y);		// 1
Point3d(x, y, z);	// 2
Vertex(x, y, z);	// 4
PVertex(x, y, z);	// 5
```

假设这个继承体系中的每一个class都定义了一个virtual function size(), 函数复杂传回class的大小, 并且在每一个构造函数中调用这个size()函数. 那么当定义PVertex object时, 5个constructors会如何? 每一次size()都是调用PVertex::size()? 或者**每次调用会被决议为"目前正在执行的constructor所对应的class"的size()函数实例**? 答案是后者, 关键是编译器如何处理, 来实现这一点
- 如果调用操作限制必须在constructor中直接调用, 那么将每一个调用操作以静态方式决议, 而不使用虚拟机制. 例如, 在Point3d constructor中, 就显式调用Point3d::size(); 然而, 如果size()之中又调用一个virtual function, 会发生什么? 这种情况下, 这个调用也必须决议为Point3d的函数实例. 而在其他情况下, 这个调用时纯正的virtual, 必须经由虚拟机制来决定其归向. 也就是说, **虚拟机制本身必须知道是否这个调用源自于一个constructor之中**
- **根本的解决之道是, 在执行一个constructor时, 必须限制一组virtual functions候选名单.** 因此需要处理virtual table, 而处理virtual table又需要通过vptr. 所以为了控制一个class中有所作用的函数, 编译系统只要简单地控制vptr的初始化和设定操作即可

<font color=#DC143C>**vptr应该在base class constructors调用之后, 在程序员提供的代码及member initialization list中所列的members初始化操作之前进行初始化**</font>
- 如果每一个constructor都一直等待到其base class constructors执行完毕之后才设定其对象的vptr, 那么每次都能调用正确的virtual function实例
- 在程序员提供的代码之前设定vptr是因为程序员提供的代码中可能会调用virtual function, 因此必须先设定
- 在member initialization list之前设定是应为member initialization list中也可能调用virtual function. 因此需要先进性设定

那么这种方式是否安全? 考虑下列两种情况:
- **在class的constructor的member initialization list中调用该class的一个虚函数:** vptr能在member initialization list被扩展之前由编译器正确设定好. 而虚函数本身可能还得依赖未被设立初值的members, 所以语意上可能是不安全的. 然而从vptr的整体角度来看, 是安全的
- **在member initialization list中使用虚函数为base class constructor提供参数:** 这是不安全的, 由于base class constructor的执行在vptr的设定之前, 因此, 此时vptr若不是尚未被设定好, 就是被设定指向错误的class. 更进一步的说, 该函数所存取的任何class's data members一定还没有被初始化

## 5.3 对象复制语意学 ##

```C++
class Point {
public:
	Point (float x = 0.0, float y = 0.0);	
	// ... (没有virtual function)
protected:
	float _x, _y;
};
```

只有在默认行为所导致的语意不安全或不正确时, 才需要设计一个copy assignment operator. 由于坐标都内含数值, 所以不会发生"别名化(aliasing)"或"内存泄漏(memory leak)", 默认的memberwise copy行为对Point object安全且正确. 如果我们自己提供一个copy assignment operator, 程序反而到会执行得比较慢

如果不对Point供应一个copy assignment operator, 而光是仰赖默认的memberwise copy, 编译器会产生一个实例吗? 这个答案和copy constructor的情况一样: 实际上不会! 由于class已经有了bitwise copy语意, 所以implicit copy assignment operator被视为毫无用处, 也更本不会合成出来

> <font color=#DC143C>总结下, 此时情况和copy constructor一样, 就是对应的操作会有, 但是copy assignment operator函数不会合成出来</font>

一个class对于默认的copy assignment operator, 在以下情况, 不会出现bitwise copy语意:
1. 当class内含有一个member object, 而这个member object有一个copy assignment operator时
2. 当一个class的base class有一个copy assignment operator时
3. 当一个class声明了任何virtual functions时(一定不要拷贝右端class object的vptr地址, 因为它可能是一个derived class object)
4. 当class继承自一个virtual base class时(无论此base class有没有copy operator)

C++标准上说, copy assignment operator并不表示bitwise copy semantics是nontrivial. 实际上, 只有nontrivial instances才会被合成出来.

当对Point class, 这样的赋值操作: 
```C++
Point a, b;
//...
a = b;
```
**由bitwise copy完成, 把Point b拷贝到Point a, 期间并没有copy assignment operator被调用. 从语意或从效率上考虑, 这都是我们所需要的. 注意, 我们还是可能提供一个copy constructor, 为的是把name return value(NRV)优化打开. copy construct的出现不应该让我们以为也一定要提供一个copy assignment operator**

> **意思是除了上述四种情况, 当类中数据成员都是基础数据类型时, 编译器都不会合成一个copy assignment operator, 但对应赋值操作会执行**

以上面的2为例子看看编译器合成的copy assignment operator是什么样子, 在为Point类显式定义一个copy assignment operator, 然后Point3d**虚继承**类Point, 但是不显式定义copy assignment operator:
```C++
inline Point& Point::operator=(const Point &p) {
	_x = p._x;
	_y = p._y;

	return *this;
}

class Point3d::virtual public Point {
public:
	Point3d(float x = 0.0, float y = 0.0, float z = 0.0);
protected:
	float _z;
};
``` 

编译器为Point3d合成的copy assignment operatro, 类似如下形式:
```C++
// C++伪码: 被合成的copy assignment operator
inline Point3d& Point3d::operator=(Point3d *const this, const Point3d &p) {
	// 调用base class的函数实例
	this->Point::operator=(p);		// 或者(*(Point*)this) = p;
	
	// memberwise copy the derived class member
	_z = p._z;
	return *this;
}
```

> copy assignment operator是一个非正交性情况, 它缺乏一个member assignment list(平行于member initialization list的东西)

**虚继承中的拷贝赋值**

假设编译器按上面的形式合成子类的copy assignment operator, 现在假设另一个类似Vertex, 和Point3d一样, **虚派生**自Point, 那么编译器为Vertex合成的copy assignment operator类似如下形式:
```C++
inline Vertex& Vertex::operator=(const Vertex &v) {
	this->Point::operator=(v);
	_next = v._next;
	
	return *this;
}
```

那么现在又从Point3d和Vertex中派生出Vertex3d. 编译器也会为Vertex3d合成copy assignment operator
```C++
inline Vertex3d& Vertex3d::operator=(const Vertex3d &v) {
	this->Point::operator=(v);
	this->Point3d::operator=(v);
	this->Vertex::operator=(v);
	// ...
}
```

在执行Point3d和Vertex的copy assignment operator时, <font color=#DC143C>**会重复调用Point的copy assignment operator**</font>

事实上, copy assignment operator在虚拟继承情况下行为不佳, 需要小心地设计和说明. 许多编译器甚至并不尝试取得正确的语意, 它们在每一个中间(调停用)的copy assignment operator中调用每一个base class instance, 于是造成virtual base class copy assignment的多个实例被调用. cfront, Edison Design Group的前端处理器, Borland C++ 4.5以及Symantec最新版C++编译器都这么做, 而C++标准对此其实也没有限制

省略一些内容, --> 见书上

建议: 尽可能不要允许一个virtual base class的拷贝操作. 甚至提供一个比较奇怪的建议: 不要再任何virtual base class中声明数据

## 5.4 对象的效能 ##

略

## 5.5 析构语意学 ##

如果class没有定义destructor, 那么只有在class内含的member object(抑或class自己的base class)拥有destructor的情况下, 编译器才会自动合成一个出来. 否则, destructor被视为不需要, 也就不需被合成(当然更不需被调用)

```C++
class Point {
public:
	Point(float x = 0.0, float y = 0.0);	
	point(const Point&);	
	
	virtual float z();
private:
	float _x, _y;
};

class Line {
public:
	Line(const Point&, const Point&);
	
	virtual draw();
protected:
	Point _begin, _end;
};
```
即使上述Point中有虚函数, Line中包含Point成员, 但都不会合成出来destructor, 因为Point并没有destror. 当从Point派生出Point3d(即使是一种虚拟派生关系)时, 如果没有声明一个destructor, 编译器就没有必要合成一个destructor

不论Point还是Point3d, 都不需要destructor, 为它们提供一个destructor反而是低效率的

当重Point3d和Vertex(有destructor)派生出Vertex3d时, Vertex destructor被调用有两种方法
(1) 如不提供一个explicit Vertex3d destructor, 此时编译器必须合成一个Vertex3d destructor, 其**唯一任务**就是调用Vertex destructor
(2) 如果提供一个Vertex3d destructor, 编译器会扩展它, 使它调用Vertex destructor(在我们所供应的程序代码之后)

一个destructor被扩展的方式类似constructor被扩展的方式, 但是顺序相反:
　　(1) destructor的函数本体首先被执行
　　(2) 如果class拥有member class object, 而后者拥有destructors, 那么它们会议其声明顺序的相反顺序被调用
　　(3) 如果object内含vptr, 现在被重新设定, 指向适当的base class的virtual table
　　(4) 如果有任何直接的(上一层)nontrivial base classes拥有destructors, 那么它们会以其声明顺序的相反顺序被调用
　　(5) 如果任何virtual base classes拥有destructor, 而目前讨论的这个class是最尾端的class, 那么它们会以其原来的构造顺序的相反顺序被调用

就像constructor一样, 目前对于destructor的一种最佳实现策略就是维护两份destructor实例:
　　一个complete object实例, 总是设定好vptr(s), 并调用virtual base class destructor
　　一个base class subobject实例; 除非在destructor函数中调用一个virtual function, 否则它绝不会调用virtual base class destructors并设定vptr(因为如果不调用虚函数就没必要修改vptr)

一个object的声明接收于其destructor开始执行之时. 由于每一个base class destructor都轮番被调用, 所以derived object实际上变成了一个完整的object. 例如一个PVertex对象归还其内存之前, 会依次编程一个Vertex3d对象, 一个Vertex对象, 一个Point3d对象, 最后成为一个Point对象. 当我们在destructor中调用member functions时, 对象的蜕变会因为vptr的重新设定(在每一个destructor中, 在程序员所提供的代码执行之前)而受到影响
