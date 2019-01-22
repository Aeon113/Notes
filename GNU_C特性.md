# GNU C 特性

### 1. 零长度和变量长度数组
实例:
``` cpp
struct var_data {
	int len;
    char data[0];
};
```
这里的data数组为零长度数组，编译器不为data分配内存，只使用data表示len后的数据。
GNU C可以在栈上使用变量来定义数组，这样定义的数组，其长度在运行期决定:
``` c
int main(int argc, char *argv[]) {
	int n = argc;
    double x[n];
    for(int i = 0; i < n; ++i)
    	x[i] = i;
    return 0;
}
```

### 2. case范围
GNU C支持case x...y的语法(x和y同类型, 且在次类型的定义域里x<=y)，表示此case 可以match [x, y]内的所有值:
``` c
switch(ch) {
	case '0'...'9': c -= '0'; break;
    case 'a'...'f': c-= 'a' - 10; break;
    case 'A'...'F': c-= 'A' - 10; break;
}
```
此处的 `case '0'...'9'`相当于 `case '0': case'1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
case '9':`

### 3. 语句表达式
GNU C将括号中的复合语句视作一个表达式，其可以出现在任何表达式可以出现的地方：
``` c
#include <stdio.h>

int main() {
	printf("%d\n", ({puts("pre1"); printf("pre%d\n", 2); 3;}));
	return 0;
}
```
输出结果是
>pre1   
>pre2   
>3

### 4. typeof
类似于C++11中的auto
```c
#include <stdio.h>

int main() {
	int a = 2;
	typeof(a) b = 3;
	typeof(b) c = 4;
	printf("%d %d %d\n", a, b, c);

	return 0;
}
```

### 5. 可变参数宏
GNU C中，宏也可以接受不定数量个参数:
``` c
#define pr_debug(fmt, arg...) \
    printk(fmt, ##arg)
```

使用"##"，可以使当arg为空时，fmt后的","会被正确地隐藏。

### 6. 标号元素
```c
unsigned char data[MAX] = {[0...MAX-1]= 0};
```

### 7. 特殊属性声明
在函数的声明后添加 `__attribute__((ATTRIBUTE))`即可为函数添加属性。 其中的`ATTRIBUTE`可为`noreturn`、`format`、`section`、`aligned`、`packed`.
   
+ noreturn: 表示此函数从不返回，可让编译器优化代码，并消除不必要的警告信息。
  
+ format: 用于接受不定数量个参数的函数，会按照printf等函数的格式化规则对参数进行类型检查。其语法为:
  ``` c
  __attribute__(format (archetype, string-index, first-to-check))
  ```
  其中的archetype为printf或scanf，string-index指format-string为第几个参数(从1开始), first-to-check表示此参数和之后的参数将按照format-string制定的规则来检查。
  示例:
  ``` c
  #include <stdio.h>
  #include <stdarg.h>
 
  #if 1
  #define CHECK_FMT(a, b)	__attribute__((format(printf, a, b)))
  #else
  #define CHECK_FMT(a, b)
  #endif
 
  void TRACE(const char *fmt, ...) CHECK_FMT(1, 2);
 
  void TRACE(const char *fmt, ...)
  {
	va_list ap;
	va_start(ap, fmt);
	(void)printf(fmt, ap);
	va_end(ap);
  }
 
  int main(void)
  {
	TRACE("iValue = %d\n", 6);
	TRACE("iValue = %d\n", "test");
 
	return 0;
  }
  ```
  gcc会对第二个TRACE语句生成警告信息:
  >main.cpp: In function ‘int main()’:   
  >main.cpp:23:31: warning: format ‘%d’ expects argument of type ‘int’, but argument 2 has type ‘const char*’ [-Wformat=]   
  >   TRACE("iValue = %d\n", "test");

 + unused: 作用于函数和变量，表示其可能不会被用到，此属性可避免编译器产生警告信息。   

 + aligned: 用于变量、结构体、联合体，表示其对其方式(以字节为单位)。   示例:
   ``` c
   struct example {
       char a;
       int b;
       long c;
   } __attribute__((aligned(4)));
   ```
   
 + packed: 作用于变量和类型，表示其以最小可能对齐:
   ``` c
   struct example {
       char a;
       int b;
       long c __attribute__((packed));
   }
   ```

### 8. 内建函数
   + __builtin_return_address(LEVEL), 表示当前函数或其调用者的返回地址，LEVEL为层级，0为当前函数，1为当前函数的调用者，2为当前函数调用者的调用者，以此类推。
   + __builtin_constant_p(EXP)用于判断EXP是否为编译期常量，是返回1，否返回0
   + __builtin_expect(EXP, C)为编译器优化分支语句时提供预测信息，其也是gcc内建的likely()和unlikely的底层实现:
   ``` c
   #define likely_notrace(x) __builtin_expect(!!(x), 1)
   #define unlikely_notrace(x) __builtin_expect(!!(x), 0)
   ```
   其用法如下:
   ``` c
   //test_builtin_expect.c
   #define LIKELY(x) __builtin_expect(!!(x), 1)
   #define UNLIKELY(x) __builtin_expect(!!(x), 0)
 
   int test_likely(int x)
   {
       if(likely(x))
       {
           x = 5;
       }
       else
       {
           x = 6;
       }
 
       return x;
   }
 
   int test_unlikely(int x)
   {
       if(unlikely(x))
       {
           x = 5;
       }
       else
       {
           x = 6;
       }
 
       return x;
   }
   ```

### 9. GNU C扩展语法的开关
在编译时制定 -ansi-pedantic会使在使用GNU扩展语法时产生警告。