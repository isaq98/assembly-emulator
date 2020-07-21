	.global fib_iter_a
	.func fib_iter_a

/* r0 - int n */
/* r1 - int i */
/* r2 - int prev_prev_num */
/* r3 - int prev_num */
/* r12 - int curr_num */
fib_iter_a:
	cmp r0, #0
	beq end
	
	// Initializing
	mov r1, #1
	mov r2, #0
	mov r3, #0
	mov r12, #1
loop:
	cmp r1, r0
	beq endloop
	
	// Performing addition & moving values
	mov r2, r3
	mov r3, r12
	add r12, r3, r2
	
	// Incrementing counters	
	add r1, r1, #1
	b loop
endloop:
	mov r0, r12
	b end
end:
	bx lr
	