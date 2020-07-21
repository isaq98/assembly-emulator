	.global sum_array_a
	.func sum_array_a

/* r0 - int* array */
/* r1 - int n */
/* r2 - int i */
/* r3 - int total */
sum_array_a:
	mov r2, #0
	mov r3, #0
loop:
	//loop check
	cmp r2, r1
	beq endloop

	//loop body
	ldr r12, [r0]
	add r3, r12, r3
	str r12, [r0]

	//increment variables
	add r2, r2, #1
	add r0, r0, #4
	b loop
endloop:
	mov r0, r3
	bx lr
