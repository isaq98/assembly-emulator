.global addsub_a
.func addsub_a
 
addsub_a:
    add r0, r1, r2
    sub r0, r1, #77
    bx lr
    