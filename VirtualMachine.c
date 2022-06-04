#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* MEMORY MAPPED REGISTERS */
enum
{
	MR_KBSR = 0xFE00;	/* keyboard status */
	MR_KBDR = 0xFE02;	/* keyboard data */	
};
/* TRAP CODES */
enum 
{
	TRAP_GETC = 0x20, 	/* get character from keyboard, not echoed onto the terminal */
	TRAP_OUT = 0x21,	/* output a character */
	TRAP_PUTS = 0x22, 	/* output a word string */
	TRAP_IN = 0x23,		/* get character from keyboar, echoed onto the terminal */
	TRAP_PUTSP = 0x24,	/* output a byte string */
	TRAP_HALT = 0x25,	/* halt the program */
};

/* MEMORY STORAGE */
#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];	/* 65536 locations */

/* REGISTERS and REGISTER STORAGE*/
enum 
{
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC,		/* program counter */
	R_COND,	
	R_COUNT
};
uint16_t reg[R_COUNT];

/* OPCODES */
enum 
{
	OP_BR = 0,	// 	branch
	OP_ADD,		// 	add
	OP_LD,		// 	load
	OP_ST,		// 	store
	OP_JSR,		//	jump register
	OP_AND,		// 	bitwise and
	OP_LDR,		// 	load register
	OP_STR,		//	store register
	OP_RTI,		//	unused
	OP_NOT,		//	bitwise not
	OP_LDI,		//	load indirect
	OP_STI,		//	store indirect
	OP_JMP,		//	jump
	OP_RES,		//	reserved (unused)
	OP_LEA,		//	load effective address
	OP_TRAP		//	execute trap
};
/* FLAGS */
enum 
{
	FL_POS = 1 << 0; 	/* P */
	FL_ZRO = 1 << 1;	/* Z */
	FL_NEG = 1 << 2;	/* N */
};

/* SIGN EXTENSION */
uint16_t sign_extend(uint16_t x, int bit_count) 
{
	if ((x >> (bit_count - 1)) & 1) 
	{
		x |= (0xFFFF << bit_count);
	}
	return x;
}

/* SWAP LITTLE-ENDIAN TO BIG-ENDIAN */
uint16_t swap16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}

void update_flags(uint16_t r) 
{
	if (reg[r] == 0)
	{
		reg[R_COND] = FL_ZRO;
	}
	else if (reg[r] >> 15)		/* a 1 in the left-most bit indicates negative */
	{
		reg[R_COND] = FL_NEG;	
	}
	else 
	{
		reg[R_COND] = FL_POS;
	}
}

void read_image_file(FILE* file)
{
	/* the origin tells us where in memory to place the image */
	uint16_t origin;
	fread(&origin, sizeof(origin), 1, file);
	origin = swap16(origin);
	
	/* we know the maximum file size so we only need one fread */
	uint16_t max_read = MEMORY_MAX - origin;
	uint16_t* p = memory + origin;
	size_t read = fread(p, sizeof(uint16_t), max_read, file);	//	maybe make it read until EOF 
	
	while (read-- > 0)
	{
		*p = swap16(*p);
		++p;
	}
}

int read_image(const char* image_path)
{
	FILE* file = fopen(image_path, "rb");
	if (!file) { return 0; }
	read_image_file(file);
	fclose(file);
	return 1;
}

/* CHECK KEY */

/* MEMORY ACCESS */
void mem_write(uint16_t address, uint16_t val)
{
	memory[address] = val;
}
uint16_t mem_read(uint16_t address)
{	
	if (address == MR_KBSR)
	{
		if (check_key())
		{
			memory[MR_KBSR] = (1 << 15);
			memory[MR_KBDR] = getchar();
		}
		else 
		{
			memory[MR_KBSR] = 0;
		}
	}
	return memory[address];
}

/* INPUT BUFFERING AND HANDLE INTERRUPT */

/* MAIN LOOP */
int main(int argc, const char* argv[])
{
	/* setup */
	
	if (argc < 2)
	{
		/* show how to run application */
		printf("lc3 [image-file] ...\n");	// 	expect a path to image
		exit(2);
	}
	
	for (int j = 1; j < argc; j++) 
	{
		if (!read_image(argv[j]))
		{
			printf("failed to load image: %s\n", argv[j]);
			exit(2);
		}
	}
	
	/* one condition flag should be set at any given time, set the Z flag */
	reg[R_COND] = FL_ZRO;
	
	/* set the PC to starting position */
	/* 0x3000 is the default */
	enum { PC_START = 0x3000; }
	reg[R_PC] = PC_START;
	
	int running = 1;
	while (running) 
	{
		/* FETCH */
		uint16_t instr = mem_read(reg[R_PC]++);
		uint16_t op = instr >> 12;
		
		switch (op) 
		{
			case OP_ADD:
			{
				/* destination register (DR) */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* first operand */
				uint16_t r1 = (instr >> 6) & 0x7;
				/* whether we are in immediate mode */
				uint16_t imm_flag = (instr >> 5) & 0x1;
				
				if (imm_flag) 
				{
					uint16_t imm5 = sign_extend(instr & 0x1F, 5);
					reg[r0] = reg[r1] + imm5;
				}
				else 
				{
					uint16_t r2 = instr & 0x7;
					reg[r0] = reg[r1] + reg[r2];
				}
				
				update_flags(r0);
				break;
			}
			case OP_AND:
			{
				/* destination register */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* first operand */
				uint16_t r1 = (instr >> 6) & 0x7;
				/* whether we are in immediate mode */
				uint16_t imm_flag = (instr >> 5) & 0x1;
				
				if (imm_flag) 
				{
					uint16_t imm5 = sign_extend(intsr & 0x1F, 5);
					reg[r0] = reg[r1] & imm5;
				}
				else 
				{
					uint16_t r2 = instr & 0x7;
					reg[r0] = reg[r1] & reg[r2];
				}
				update_flags(r0);
				break;
			}
			case OP_NOT:
			{
				/* destination register */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* source register */
				uint16_t r1 = (instr >> 6) & 0x7;
				
				reg[r0] = ~reg[r1];			/* not sure on ~ operator */
				update_flags(r0);
				break;
			}
			case OP_BR:
			{
				/* condition flag (3 bits) in instruction */
				uint16_t cond_flag = (instr >> 9) & 0x7;
				
				if (cond_flag & reg[R_COND]) 
				{
					/* add PCoffset 9 to program counter */
					reg[R_PC] += sign_extend(instr & 0x1FF, 9);
				}
				break;
			}
			case OP_JMP:
			{
				/* the register to read content of */
				uint16_t r0 = (instr >> 6) & 0x7;
				reg[R_PC] = reg[r0];
				break;
			}
			case OP_JSR:
			{
				/* save */
				reg[R_R7] = reg[R_PC];
				/* whether offset or read from register */
				uint16_t offset_flag = (instr >> 11) & 0x1;
				
				if (offset_flag)
				{
					reg[R_PC] += sign_extend(instr & 0x7FF, 11);
				}
				else 
				{
					/* register to read address from */
					uint16_t r0 = (instr >> 6) & 0x7;
					reg[R_PC] = reg[r0];
				}
				break;
			}
			case OP_LD:
			{
				/* register to write content to */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* an address to read from */
				uint16_t addr = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
				
				reg[r0] = mem_read(addr);
				update_flags(r0);
				break;
			}
			case OP_LDI:
			{
				/* destination register */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* PCoffset 9 */
				uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
				/* add pc_offset to current PC, look at that address and 
					read from that address to get a new address to read from */
				reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
				
				update_flags(r0);
				break;
			}
			case OP_LDR:
			{
				/* a register to write content to */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* a register to get content from */
				uint16_t r1 = (instr >> 6) & 0x7;
				/* offset 6 */
				uint16_t offset = sign_extend(instr & 0x3F, 6);
				
				reg[r0] = mem_read(reg[r1] + offset);
				update_flags(r0);
				break;
			}
			case OP_LEA:
			{
				/* a register to write address to */
				uint16_t r0 = (instr >> 9) & 0x7;
				
				reg[r0] = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
				update_flags(r0);
				break;
			}
			case OP_ST:
			{
				/* a register to read content of */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* an address to which we're going to write */
				uint16_t addr = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
				
				mem_write(addr, reg[r0]);
				break;
			}
			case OP_STI:
			{
				/* a register to read content of */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* an address whose content will be the address to write to */
				uint16_t addr = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
				
				mem_write(mem_read(addr), reg[r0]);
				break;
			}
			case OP_STR:
			{
				/* a register which content is going to be stored somewhere */
				uint16_t r0 = (instr >> 9) & 0x7;
				/* a base register */
				uint16_t r1 = (instr >> 6) & 0x7;
				/* offset 6 */
				uint16_t offset = sign_extend(instr & 0x3F, 6)
				
				mem_write(reg[r1] + offset, reg[r0]);
				break;
			}
			case OP_TRAP:
			{
				reg[R_R7] = reg[R_PC];
				
				switch (instr & 0xFF) 
				{
					case TRAP_GETC:
					{
						/* read a single ASCII character */
						reg[R_R0] = (uint16_t)getchar();
						update_flags(R_R0);
						break;
					}
					case TRAP_OUT:
					{
						putc((char)reg[R_R0], stdout);
						fflush(stdout);
						break;
					}
					case TRAP_PUTS:
					{
						uint16_t* c = memory + reg[R_R0];
						while (*c)
						{
							putc((char)*c, stdout);
							++c;
						}
						fflush(stdout);
						break;
					}
					case TRAP_IN:
					{
						printf("Enter a character: ");
						char c = getchar();
						putc(c, stdout);
						fflush(stdout);
						reg[R_R0] = (uint16_t)c;
						update_flags(R_R0);
						break;
					}
					case TRAP_PUTSP:
					{
						/* one char per byte (2 bytes per word) */
						uint16_t* c = memory + reg[R_R0];
						while (*c)
						{
							char ch1 = (*c) & 0xFF;
							putc(ch1, stdout);
							char ch2 = (*c) >> 8;
							if (ch2) putc(ch2, stdout);
							++c;
						}
						fflush(stdout);
						break;
					}
					case TRAP_HALT:
					{
						puts("HALT");
						fflush(stdout);
						running = 0;
						break;
					}
				}
				break;
			}
			case OP_RES:
			case OP_RTI:
				abort();
			default:
				break;
		}
	}
	
	return 0;
}
