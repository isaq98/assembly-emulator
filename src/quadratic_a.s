    .global quadratic_a
    .func quadratic_a

quadratic_a:
	mul r2, r0, r2
	add r2, r2, r3
	mul r12, r0, r0
	mul r0, r1, r0
	add r0, r12, r2
	bx lr
