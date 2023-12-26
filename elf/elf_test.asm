BITS 32
; nasm -f elf elf_test.asm && ld -m elf_i386 -o elf_test elf_test.o && ./elf_test

section .test
global _start

str_len:
	push ebp
	mov ebp, esp
	push ecx

	mov eax, [ebp + 8]
	
	mov ecx, 0
	str_len_loop:
		cmp byte [eax + ecx], 0
		je str_len_end
		inc ecx
		jmp str_len_loop
	str_len_end:
	mov eax, ecx

	pop ecx
	pop ebp
	ret

print:
	push ebp
	mov ebp, esp
	pushad

	mov ecx, [ebp + 8]

	push ecx
	call str_len
	add esp, 4
	mov edx, eax

	mov eax, 4
	mov ebx, 1
		
	int 0x80
	
	popad
	pop ebp
	ret


_start:
	push msg
	call print
	add esp, 4

	sub esp, 32

	mov eax, 3
	mov ebx, 0
	mov ecx, esp
	mov edx, 32
	int 0x80

	push esp
	call print
	add esp, 4

	add esp, 32

	mov eax, 1
	mov ebx, 0
	int 0x80

section .data
msg db "Type something: ", 0xa, 0x0

