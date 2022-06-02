#include <stdio.h>
#include <stdint.h>


#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX];	/* 65536 locations */

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

enum 
{
	FL_POS = 1 << 0; 	/* P */
	FL_ZRO = 1 << 1;	/* Z */
	FL_NEG = 1 << 2;	/* N */
};


uint16_t sign_extend(uint16_t x, int bit_count);
void update_flags(uint16_t r);


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
				break;
			}
			case OP_BR:
			{
				/* negative flag (bit) in instruction */
				uint16_t neg_flag = (instr >> 9) & 0x4;
				/* zero flag (bit) in instruction */
				uint16_t zer_flag = (instr >> 9) & 0x2;
				/* positive flag (bit) in instruction */
				uint16_t pos_flag = (instr >> 9) & 0x1;
				
				/* register that stores flags */
				uint16_t flags = reg[R_COND];
				
				if ((neg_flag & flags) | (zer_flag & flags) | (pos_flag & flags)) 
				{
					/* add PCoffset 9 to program counter */
					reg[R_PC] = reg[R_PC] + sign_extend(instr & 0x1FF, 9);
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
					reg[R_PC] = reg[R_PC] + sign_extend(instr & 0x7FF, 11);
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
				// st
				break;
			case OP_STI:
				// sti
				break;
			case OP_STR:
				// str
				break;
			case OP_TRAP:
				// trap
				break;
			case OP_RES:
			case OP_RTI:
			default:
				//	bad opcode
				break;
		}
	}
	
	return 0;
}

uint16_t sign_extend(uint16_t x, int bit_count) 
{
	if ((x >> (bit_count - 1)) & 1) 
	{
		x |= (0xFFFF << bit_count);
	}
	return x;
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




