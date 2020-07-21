	.global find_max_a
	.func find_max_a

/* r0 - int* array */
/* r1 - int n (length) */
/* r2 - int i */
/* r3 - int max */
find_max_a:
	mov r2, #0
	ldr r3, [r0]
loop:
	cmp r2, r1
	beq endloop
	
	//load value at i in array into r12 & compare
	ldr r12, [r0]
	cmp r3, r12
	blt r12_max
	str r12, [r0]
	
	//increment values
	add r2, r2, #1
	add r0, r0, #4
	b loop
r12_max:
	//swap if needed
	mov r3, r12
	b loop
endloop:
	//move value & return
	mov r0, r3
	bx lr
