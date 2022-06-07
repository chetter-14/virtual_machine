# LC-3 Virtual Machine
## What it is

A custom virtual machine written in C to execute LC-3 executable programs.

## Implementation details

Virtual machine core consists of:
- *Memory*
- *Registers*
- *State machine* (something like opcodes interpreter)

There are some other little things like: platform dependent console handling, reading an executable file into vm memory, etc.

## Resources 

link to the tutorial - https://www.jmeiners.com/lc3-vm/

link to the docs for LC-3 architecture - https://www.jmeiners.com/lc3-vm/supplies/lc3-isa.pdf
