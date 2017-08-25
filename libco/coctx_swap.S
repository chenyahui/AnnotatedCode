/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

.globl coctx_swap
#if !defined( __APPLE__ ) && !defined( __FreeBSD__ )
.type  coctx_swap, @function
#endif
coctx_swap:

// coctx_swap(coctx_t curr,coctx_t pending)

#if defined(__i386__)
   
	leal 4(%esp), %eax //sp 
	movl 4(%esp), %esp 
	leal 32(%esp), %esp //parm a : &regs[7] + sizeof(void*)

	pushl %eax //esp ->parm a 

	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl -4(%eax)

	
	movl 4(%eax), %esp //parm b -> &regs[0]

	popl %eax  //ret func addr
	popl %ebx  
	popl %ecx
	popl %edx
	popl %edi
	popl %esi
	popl %ebp
	popl %esp
	pushl %eax //set ret func addr

	xorl %eax, %eax
	ret

#elif defined(__x86_64__)
    
	leaq 8(%rsp),%rax   // 父函数栈帧中除返回地址外栈帧顶的位置
	
	// rdi寄存器存的是第一个参数的地址，其112个字节后，是regs数组后的位置。难道是要把ss_size的首地址放进去？显然不是的，因为栈的地址从高位到低位
    // 所以，再度push的时候，将会首先填充regs[13] ，一直填充到reg[0]  
	leaq 112(%rdi),%rsp 
	
	// 将寄存器保存到入栈,因为此时栈的地址指向数组，因此实际上就是讲各个寄存器填充到数组中
	pushq %rax  // rax -> regs[13]，也就是当前的rsp -> regs[13]
	pushq %rbx  // rbx -> regs[12]
	pushq %rcx  // rcx -> regs[11]
	pushq %rdx  // rdx -> regs[10]

    
	pushq -8(%rax) // ret func addr  返回地址 -> regs[9]

	pushq %rsi  // rsi -> regs[8]
	pushq %rdi  // rdi -> regs[7]
	pushq %rbp  // rbp -> regs[6]
	pushq %r8   // r8 -> regs[5]
	pushq %r9   // r9 -> regs[4]
	pushq %r12   // r12 -> regs[3]
	pushq %r13    // r13 -> regs[2]
	pushq %r14   // r14 -> regs[1]
	pushq %r15   // r15 -> regs[0]
	
	// 截止到此，所有的协程上下文保存成功 

	// rsi中是第二个参数，我们需要将第二个参数的上下文载入到寄存器和栈里面
	// rsi的首地址就是整个regs[]参数的地址,从0开始，所以当pop的时候，将从0将参数pop出来。
	movq %rsi, %rsp

    // 以下为倒序还原
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r9
	popq %r8
	popq %rbp
	popq %rdi
	popq %rsi
	popq %rax //ret func addr
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rsp

	pushq %rax // 将返回地址入栈
	
	// 将eax寄存器清零，eax是rax的低32位，也就是将rax的低32位清零。也就是return 0的意思。
	xorl %eax, %eax
	// 返回函数
	ret
#endif
