此库为coroutine注释版, 里面详细写了本人阅读代码的注释和心得。
总体coroutine的原理分析，可以查看博客[云风coroutine协程库源码分析](http://www.cyhone.com/articles/analysis-of-cloudwu-coroutine/)

It's an asymmetric coroutine library (like lua).

You can use coroutine_open to open a schedule first, and then create coroutine in that schedule. 

You should call coroutine_resume in the thread that you call coroutine_open, and you can't call it in a coroutine in the same schedule.

Coroutines in the same schedule share the stack , so you can create many coroutines without worry about memory.

But switching context will copy the stack the coroutine used.

Read source for detail.

Chinese blog : http://blog.codingnow.com/2012/07/c_coroutine.html