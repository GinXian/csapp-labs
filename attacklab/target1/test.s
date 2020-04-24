movq %rsp,%rax
movq %rax,%rdi
pop %rax
movl %eax,%edx
movl %edx,%ecx
movl %ecx,%esi
lea (%rdi,%rsi,1),%rax
movq %rax,%rdi
retq
