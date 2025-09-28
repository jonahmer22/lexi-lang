# lexi-lang
A custom interpreted assembly language project. Includes a parser, compiler, and virtual machine for running user defined assembly code.

## Registers
- **R0–R7**: 8 general-purpose 16-bit registers  
- **ACC**: Accumulator (all arithmetic/logic ops target this)  
- **SP**: Stack pointer (full-descending)  
- **PC**: Program counter (accessible for computed jumps, self-modifying tricks)  

**Total: 11 registers**

---

## Memory Model
- **Address space**: 64 KiB, word-addressable (16-bit words)  
- **Stack**: Located in memory, managed via `SP`  
    - Push = decrement `SP`, write value  
    - Pop = read value, increment `SP`  
- **Instruction format**: word-aligned (16-bit opcodes + optional extra words)  

## Memory Map
- `0x0000 – 0xFEFF`: General-purpose RAM (program managed data)
- `0xFF00`: Output device (print port)
    - Writing here prints the ASCII character of the value
    - `PRN ACC` is shorthand for `ST ACC, [0xFF00]`

---

## Instruction Set

### Data Movement
- `MOV Rd, Rs` - copy register to register  
- `MOV Rd, #imm` - load immediate into register  
- `LD Rd, [addr]` - load from memory address (LD R0, [0x2000])
    - 16-bit addressable range of memory
    - addresses are in 4 digit hexidecimal
- `ST Rs, [addr]` - store to memory address (ST R0, [0x2000])
- `PUSH Rs` - push register onto stack  
- `POP Rd` - pop from stack into register  

### Arithmetic (operate on `ACC`)
- `ADD Rs` - `ACC = ACC + Rs`  
- `SUB Rs` - `ACC = ACC - Rs`  
- `MUL Rs` - `ACC = ACC * Rs`  
- `DIV Rs` - `ACC = ACC / Rs` (integer divide)  
- `INC` - increment `ACC`  
- `DEC` - decrement `ACC`  
- `CLR` - clear `ACC` (set to 0)  

### Logic
- `AND Rs` - `ACC = ACC & Rs`  
- `OR Rs` - `ACC = ACC | Rs`  
- `XOR Rs` - `ACC = ACC ^ Rs`  
- `NOT` - `ACC = ~ACC`  

### Control Flow
- `JMP label` - jump to label  
- `JEZ label` - jump if `ACC == 0`  
- `JLZ label` - jump if `ACC < 0`  
- `JGZ label` - jump if `ACC > 0`  
- (advanced: `MOV PC, Rs` allows computed jumps)  

### I/O
- `PRN ACC` – print the ASCII character in `ACC`  
    - Equivalent to `ST ACC, [0xFF00]`  

### Special
- `HLT` - halt CPU  
- `NOP` - no operation  

---

## Example Program: Countdown

```asm
    MOV R0, #5      ; start value
    MOV ACC, R0

@loop:
    DEC             ; ACC--
    JLZ done        ; if ACC < 0, jump to done
    JMP loop        ; repeat

@done:
    HLT
```

## Example Program: Print "HI"

```asm
    MOV ACC, #72    ; ASCII 'H'
    PRN ACC         ; prints H
    MOV ACC, #73    ; ASCII 'I'
    PRN ACC         ; prints I
    HLT
```
