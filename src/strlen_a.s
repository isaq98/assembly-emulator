	.global strlen_a
	.func strlen_a

/* r0 - char *s */
/* r1 - int i */
strlen_a:
	mov r1, #0
	cmp r0, #0
	beq endloop
while:
	// load the first val into r12
	ldrb r12, [r0]
	
	// check to see if you reached the end of the string
	cmp r12, #0
	beq endloop
	
	// increment length counter
	add r1, r1, #1
	
	// increment index in array
	add r0, r0, #1
	b while
endloop:
	mov r0, r1
	bx lr
    