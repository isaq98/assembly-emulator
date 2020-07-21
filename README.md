# ARM Assembly Emulator

## Description

This program emulates ARM Assembly instructions in C Code. The emulation code is tested through using ARM Aeesmbly and C versions of the following programs: (fib_iter_main, fib_rec_main, find_max_main, quadratic_main, strlen_main, sum_array_main).

## Compile

Run `make all` in order to compile all the files. This will compile all the emulated code and make it ready to run.

## Running the application

```
./armenu [options]
    options: 
        -c cache_size - Sets the cache size. Must be a power of 2, less than 1024 and above 8. Default 8.
```
