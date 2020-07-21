	.global fib_rec_a
	.func fib_rec_a

fib_rec_a:
	//Allocate space for 3 words
	sub sp, sp, #16
	str lr, [sp]
	str r2, [sp, #4]
	str r3, [sp, #8]

	//Check if x > 0
	cmp r0, #0
	bne base_case_step
	mov r0, #0
	b end

base_case_step:
	//Check if x == 1
	sub r4, r0, #1
	cmp r4, #0
	
	//if not, branch to recursive step
	bne fib_rec_step
	mov r0, #1
	b end

fib_rec_step:
	//Put in save register
	mov r3, r0
	
	//Calculate n - 2
	sub r0, r0, #2
	bl fib_rec_a

	//Calculate n - 1
	mov r2, r0
	sub r0, r3, #1
	bl fib_rec_a
	
	//Get the next number in sequence by adding
	add r0, r0, r2

end:
	ldr r3, [sp, #8]
	ldr r2, [sp, #4]
	ldr lr, [sp]
	add sp, sp, #16
	bx lr
