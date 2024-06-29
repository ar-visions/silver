.section .data
.section .text

.globl dyncall
dyncall:
    pushq %rbp          # Save the current base pointer
    movq %rsp, %rbp     # Set up a new base pointer

    # Arguments:
    # %rdi = func (pointer to function)
    # %rsi = args (pointer to array of void*)
    # %rdx = arg_count (number of arguments)

    # Initialize loop counter
    movq %rsi, %rcx     # Copy args array pointer to rcx
    xorq %r8, %r8       # Clear r8, will use as index counter

    # Loop through arguments and load them into registers
.Lloop_start:
    cmpq %r8, %rdx      # Compare loop counter with arg_count
    je .Lcall_func      # Exit loop if all arguments processed

    # Load argument from args[r8] and place in appropriate register
    movq (%rcx), %rax   # Load the pointer from the args array

    # Use r8 as the loop counter to determine which register to load
    cmpq $0, %r8
    je .Lset_rdi
    cmpq $1, %r8
    je .Lset_rsi
    cmpq $2, %r8
    je .Lset_rdx
    cmpq $3, %r8
    je .Lset_rcx
    cmpq $4, %r8
    je .Lset_r8
    cmpq $5, %r8
    je .Lset_r9
    cmpq $6, %r8
    je .Lset_r10
    cmpq $7, %r8
    je .Lset_r11

.Lset_rdi:
    movq %rax, %rdi
    jmp .Lnext_arg

.Lset_rsi:
    movq %rax, %rsi
    jmp .Lnext_arg

.Lset_rdx:
    movq %rax, %rdx
    jmp .Lnext_arg

.Lset_rcx:
    movq %rax, %rcx
    jmp .Lnext_arg

.Lset_r8:
    movq %rax, %r8
    jmp .Lnext_arg

.Lset_r9:
    movq %rax, %r9
    jmp .Lnext_arg

.Lset_r10:
    movq %rax, %r10
    jmp .Lnext_arg

.Lset_r11:
    movq %rax, %r11
    jmp .Lnext_arg

.Lnext_arg:
    addq $8, %rcx       # Move to the next pointer in the args array
    incq %r8            # Increment loop counter
    jmp .Lloop_start    # Repeat the loop

.Lcall_func:
    # Call the function pointed by %rdi
    call *%rdi

    # Clean up stack and restore base pointer
    movq %rbp, %rsp
    popq %rbp
    ret

    # Note: This implementation assumes a maximum of 8 arguments.