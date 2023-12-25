BITS 32
; nasm -f elf elf_test.asm && ld -m elf_i386 -o elf_test elf_test.o && ./elf_test

section .test
global _start

_start:
	push dword msg

	mov eax, 4
	mov ebx, 1
	pop ecx
	mov edx, 14
	int 0x80

	mov eax, 1
	mov ebx, 0
	int 0x80

section .data
msg db "Hello, world!", 0xa
