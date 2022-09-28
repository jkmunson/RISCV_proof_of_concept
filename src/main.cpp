
#include <stdio.h>
#include <stdint.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_image.h>

#include <main.h>
#include <assert.h>

#include <iostream>

#pragma pack(1)


#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#include <time.h>
#include <stdlib.h>

union Instruction {
	uint32_t data;
	
	struct {
		uint32_t low3:3;
	} identity;
	
	struct {
		uint32_t op:7;
	} opcode;
	
	struct {
		uint32_t op:7;
		uint32_t rd:5;
		uint32_t funct3:3;
		uint32_t rs1:5;
		uint32_t rs2:5;
		uint32_t funct7:7;
	} R;
	
		struct {
		uint32_t op:7;
		uint32_t rd:5;
		uint32_t funct3:3;
		uint32_t rs1:5;
		uint32_t imm:12;
	} I;
	
		struct {
		uint32_t op:7;
		uint32_t rd:5;
		uint32_t imm:19;
		uint32_t sign:1;
	} J;
};

void EMU_ERROR(const char * const message){
	printf("%s",message);
	exit(0);
}

class memorydevice {
	private:
	uint32_t start;
	uint32_t length;
	char *mem;
	public:
	memorydevice(uint32_t map_addr, uint32_t size) {
		mem = (char *)malloc(size);
		start = map_addr;
		length = size;
	}
	~memorydevice() {
		free((void *)mem);
	}
	uint32_t read(uint32_t address) {
		if(address-start > length-4 || address < start) EMU_ERROR("Invalid Memory Read!\n");
		return *(uint32_t *)(mem + (address-start));
	}
	void write(uint32_t address, uint32_t data){
		if(address-start > length-4 || address < start) EMU_ERROR("Invalid Memory Write!\n");
		*(uint32_t *)(mem + (address-start)) = data;
		//printf("write completed data : location : readback | %X :%u :%X\n", data, (address-start), *(uint32_t *)(mem + (address-start)));
		return;
	}
};

//A simple RISC core
class hart_RV32E_BASE {
	protected:
	//registers
	uint32_t x[32];
	
	uint32_t PC;
	
	memorydevice *memoryHandle;
	public:
	class {
		private:
		uint32_t top_held = 0;			//Highest memory address currently cached
		uint32_t bottom_held = 0;		//Lowest memory address currently cached
		uint32_t index = 0;				//Currently Indexed memory address
		uint32_t next_fetch = 0; 		//Low address for next cache_load()
		uint32_t const cache_size = 1024;	//Should match the size of the line below. %32 = 0 must be true.
		unsigned char cache[1024] = {0};	//Should match the size of the line above
		
		//RISING ONLY
		public:
		uint32_t fetch(uint32_t location) {
			uint32_t ret;
			if(location%4)	EMU_ERROR("Invalid instruction alignment!\n");
			if(!location)	EMU_ERROR("Invalid instruction location (0x0000)\n");
			if(location == index) {
				//The instruction immediately followed the last
				ret = *(uint32_t *)(cache+index-bottom_held);
				if ((index += 4) >= top_held) index = 0;
				return ret;
			} else if(location >= bottom_held && location < top_held-4) {
				ret = *(uint32_t *)(cache+location-bottom_held);
				if ((index = location + 4) >= top_held) index = 0;
			} else {
				//Instruction not in cache! Processor must wait for cache - refill. TODO
				printf("Cache Error\n");
			}
			return 0;
		}
		void setIndex(uint32_t location) {
			index = location;
		}
		void print(void) {
			printf(GRN "\n#### INSTRUCTION CACHE ####\n" RESET);
			printf("top_held: %08X \t| bottom_held: %08X\t | index: %08X \t | next_fetch: %08X\n", top_held, bottom_held, index, next_fetch);
			for(uint32_t i=0; i < cache_size; i+=8*4) {
				printf("%08X : ", bottom_held+i);
				for(uint32_t j=0; j<8; j++) {
					bool is_next_op = bottom_held + i + 4*j == index;
					if(is_next_op) printf(GRN);
					printf("%08X \t", *(uint32_t *)(cache + i + 4*j));
					if(is_next_op) printf(RESET);
				}
				printf("\n");
			}
		}
		void load(uint32_t target, memorydevice *memory){
			bottom_held = target;
			top_held = target + cache_size;
			for(unsigned int i=0; i<cache_size/4; i++){
				*(uint32_t*)(cache+4*i) = memory->read(target+i*4);
				//printf("From Memory: %X:%X\n", (target+i*4), memory->read(target+i*4));
			}
		}
	} instruction_cache;
	
	void rising(hart_RV32E_BASE *core) {
		Instruction inst;
		inst.data = instruction_cache.fetch(PC);
		if(inst.identity.low3 == 7) { //J Type
			if(inst.J.op == 0b1101111) {//JAL - Jump Always
				x[inst.J.rd] = PC+4; //Return Address
				if(inst.J.sign) {
					PC	-=inst.J.imm;
				} else {
					PC	+= inst.J.imm;
				}
				printf("Jumping by %X", inst.J.imm);
			}
		}
		
		if(inst.identity.low3 == 0b011) { //I Type NOT CORRECT
			if((inst.I.op == 0b0010011) && inst.I.funct3 == 0b000) {//ADDI - Add Immediate
				x[inst.I.rs1] = x[inst.I.rs1] + inst.I.imm;
				printf("Adding!\n");
			}
		}
	}
	
	void falling(hart_RV32E_BASE *core) {
		PC+=4;
		instruction_cache.setIndex(PC);
	}
	
	hart_RV32E_BASE(uint32_t stack_pointer, uint32_t program_counter, memorydevice *memory) {
		memset(x, 0, 32*4);
		x[2] = stack_pointer;
		PC = program_counter;
		memoryHandle = memory;
		instruction_cache.load(PC, memory);
	}
	
	public:
	void print(void) {
		printf(YEL "#### REGISTERS ####\n");
		printf(BLU "PC: %X \n" RESET, PC);
		printf(YEL " Return Address \t\t Stack Pointer \t\t\t Global Pointer \t\t Thread Pointer\n" RESET);
		for(int i = 1; i<=0xF; i++) {
			printf("x%02X : %08X (%010u) \t", i, x[i], x[i]);
			if(i==4) printf(YEL "\n Temporary 1 \t\t\t Temporary 2 \t\t\t Temporary 3 \t\t\t Frame Pointer\n" RESET);
			if(i==8) printf(YEL "\n Saved Register \t\t Arg or Ret 0 \t\t\t Arg or Ret 1 \t\t\t Argument 2\n" RESET);
			if(i==12)printf(YEL "\n Argument 3 \t\t\t Argument 4 \t\t\t Argument 5\n" RESET);
		}
		instruction_cache.print();
	}
};

int main(int argc, char *argv[]) 
{
	printf("%c\b%c\b",argv[argc-1][0],' ');
	memorydevice ram(0x20000000, 1024);
	ram.write(0x20000000, ((1<<20) | (5<<15) | (0<<12) | (5<<7) | (0b0010011 << 0))); //ADDI x5, x5, 1
	ram.write(0x20000004, ((4<<20) | (6<<15) | (0<<12) | (6<<7) | (0b0010011 << 0))); //ADDI x6, x6, 4
	ram.write(0x20000008, (1<<31) | (12<<12) | (1<<7) | (0b1101111 << 0)); //JAL -12 (Relative jump, back two)
	hart_RV32E_BASE core1(0x20000000+512, 0x20000000, &ram);

	while(1) {
		printf("\e[1;1H\e[2J");
		core1.print();
		core1.rising(&core1);
		core1.falling(&core1);
		//printf("%X\n", ((1<<20) | (5<<15) | (0<<12) | (5<<7) | (0b0010011 << 0)));
		getchar();
	}
	delete(&core1);
	return 0;
}
