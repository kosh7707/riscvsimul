#include <stdio.h>
#include <math.h>
#include <stdlib.h>

//clock cycles
long long cycles = 0;

// registers
long long int regs[32];

// program counter
unsigned long pc = 0;

// memory
#define INST_MEM_SIZE (32*1024)
#define DATA_MEM_SIZE (32*1024)
unsigned long inst_mem[INST_MEM_SIZE]; //instruction memory
unsigned long long data_mem[DATA_MEM_SIZE]; //data memory

//misc. function
int init(char* filename);

void print_cycles();
void print_reg();
void print_pc();

// pipeline register
long instruction_reg,
      opcode_reg,
      rd, rs1, rs2,
      func3, func7,
      immed,
      alu_result_reg,
      PrePC;
void pipeline_register_clear() {
    instruction_reg = 0;
    opcode_reg = 0;
    rd = 0; rs1 = 0; rs2 = 0;
    func3 = 0; func7 = 0;
    immed = 0;
    alu_result_reg = 0;
    PrePC = 0;
}
// control unit
struct {
    int Branch;
    int MemRead;
    int MemWrite;
    int ALUOP1, ALUOP0;
    int ALUSrc;
    int RegWrite;
} control_unit;
void control_unit_reset() {
    control_unit.Branch = 0;
    control_unit.MemRead = 0;
    control_unit.ALUOP1 = 0;
    control_unit.ALUOP0 = 0;
    control_unit.MemWrite = 0;
    control_unit.ALUSrc = 0;
    control_unit.RegWrite = 0;
}
// ALU Control_Enum;
enum ALU_CONTROL {
    AND = 0b0000,
    OR  = 0b0001,
    ADD = 0b0010,
    XOR = 0b0100,
    SUB = 0b0110
};
int alu_control;
// Zero Flag Bit
int zero_flag = 0;

//fetch an instruction from an instruction memory
void fetch() {
    pipeline_register_clear();
    control_unit_reset();
    zero_flag = 0;
    instruction_reg = inst_mem[pc/4];
    PrePC = pc;
    pc += 4;
}

//decode the instruction and read data from register file
void decode() {
    opcode_reg = instruction_reg & 0b1111111;
    if (opcode_reg == 0b0110011) {
        // r-type
        // add, sub, and, or, xor
        rd      = (instruction_reg>>7)  & 0b11111;
        func3   = (instruction_reg>>12) & 0b111;
        rs1     = (instruction_reg>>15) & 0b11111;
        rs2     = (instruction_reg>>20) & 0b11111;
        func7   = (instruction_reg>>25) & 0b1111111;
        control_unit.ALUOP1 = 1;
        control_unit.RegWrite = 1;
    }
    else if (opcode_reg == 0b0010011 || opcode_reg == 0b1100111 || opcode_reg == 0b0000011) {
        // i-type
        // addi, andi, ori, xori, jalr, lw
        rd      = (instruction_reg>>7)  & 0b11111;
        func3   = (instruction_reg>>12) & 0b111;
        rs1     = (instruction_reg>>15) & 0b11111;
        immed   = instruction_reg>>20;
        control_unit.ALUSrc = 1;
        if (opcode_reg == 0b0010011 || opcode_reg == 0b1100111) control_unit.RegWrite = 1;
        if (opcode_reg == 0b1100111) {
            control_unit.Branch = 1;
            zero_flag = 1;
        }
        if (opcode_reg == 0b0000011) control_unit.MemRead = 1;
    }
    else if (opcode_reg == 0b0100011) {
        // s-type
        // sw
        immed   = (instruction_reg>>7)    & 0b11111;
        func3   = (instruction_reg>>12)   & 0b111;
        rs1     = (instruction_reg>>15)   & 0b11111;
        rs2     = (instruction_reg>>20)   & 0b11111;
        immed  += ((instruction_reg>>25) & 0b1111111) << 5;
        control_unit.ALUSrc     = 1;
        control_unit.MemWrite   = 1;
    }
    else if (opcode_reg == 0b1101111) {
        // j-type
        // jal
        rd      = (instruction_reg>>7)   & 0b11111;
        immed   = ((instruction_reg>>21) & 0b1111111111) << 1;   // 10:1
        immed  += ((instruction_reg>>20) & 0b1)          << 11;  // 11
        immed  += ((instruction_reg>>12) & 0b11111111)   << 12;  // 19:12
        immed  += ((instruction_reg>>31) & 0b1)          << 20;  // 20
        if ((immed >> 20) == 1)
            immed = (immed << 24) >> 24;
        control_unit.Branch     = 1;
        control_unit.RegWrite   = 1;
    }
    else {
        // b-type
        // beq
        immed   = ((instruction_reg>>8)     & 0b1111)   << 1; // 4:1
        immed  += ((instruction_reg>>25)    & 0b111111) << 5; // 10:5
        immed  += ((instruction_reg>>7)     & 0b1)      << 11; // 11
        immed  += ((instruction_reg>>31)    & 0b1)      << 12; // 12
        if ((immed >> 12) == 1)
            immed = (immed << 24) >> 24;
        func3   = (instruction_reg>>12)     & 0b111;
        rs1     = (instruction_reg>>15)     & 0b11111;
        rs2     = (instruction_reg>>20)     & 0b11111;
        control_unit.Branch = 1;
        control_unit.ALUOP0 = 1;
    }

    // set ALU control
    if (control_unit.ALUOP1 == 0 && control_unit.ALUOP0 == 0) alu_control = ADD;
    else if (control_unit.ALUOP1 == 0 && control_unit.ALUOP0 == 1) alu_control = SUB;
    else {
        if (func3 == 0b000) {
            if (((func7>>5) & 0b1) == 0) alu_control = ADD;
            else alu_control = SUB;
        }
        else if (func3 == 0b100) alu_control = XOR;
        else if (func3 == 0b110) alu_control = OR;
        else if (func3 == 0b111) alu_control = AND;
    }
}

//perform the appropriate operation 
void exe() {
    switch (alu_control) {
        case ADD:
            if (control_unit.ALUSrc == 1) alu_result_reg = regs[rs1] + immed;
            else alu_result_reg = regs[rs1] + regs[rs2];
        break;
        case SUB:
            if (control_unit.ALUSrc == 1) alu_result_reg = regs[rs1] - immed;
            else alu_result_reg = regs[rs1] - regs[rs2];
            break;
        case XOR:
            if (control_unit.ALUSrc == 1) alu_result_reg = regs[rs1] ^ immed;
            else alu_result_reg = regs[rs1] ^ regs[rs2];
            break;
        case OR:
            if (control_unit.ALUSrc == 1) alu_result_reg = regs[rs1] | immed;
            else alu_result_reg = regs[rs1] | regs[rs2];
            break;
        case AND:
            if (control_unit.ALUSrc == 1) alu_result_reg = regs[rs1] & immed;
            else alu_result_reg = regs[rs1] & regs[rs2];
            break;
    }
    if (alu_result_reg == 0) zero_flag = 1;
    if (control_unit.Branch == 1 && zero_flag == 1) {
        if (opcode_reg == 0b1100111) pc = regs[rs1] + immed;
        else pc = PrePC + immed;
    }
}

//access the data memory
void mem() {
    if (control_unit.MemWrite == 1)
        data_mem[alu_result_reg] = regs[rs2];
    if (control_unit.MemRead == 1)
        regs[rd] = data_mem[alu_result_reg];
}

//write result of arithmetic operation or data read from the data memory if required
void wb() {
    if (control_unit.RegWrite == 1) {
        if (control_unit.Branch == 1) regs[rd] = PrePC+4;
        else regs[rd] = alu_result_reg;
    }
    regs[0] = 0;
}

int main(int ac, char* av[]) {
	if (ac < 3) {
		printf("./riscv_sim filename mode\n");
		return -1;
	}
	if (init(av[1]) != 0) return -1;

    char done = 0;
	while (!done) {
		fetch();
		decode();
		exe();
		mem();
		wb();

		cycles++;    //increase clock cycle

		//if debug mode, print clock cycle, pc, reg
		if (*av[2] == '0') {
			print_cycles();  //print clock cycles
			print_pc();		 //print pc
			print_reg();	 //print registers
		}

		// check the exit condition, do not delete!!
		if (regs[9] == 10)  //if value in $t1 is 10, finish the simulation
			done = 1;
	}

	if (*av[2] == '1') {
		print_cycles();  //print clock cycles
		print_pc();		 //print pc
		print_reg();	 //print registers
	}

	return 0;
}

/* initialize all datapath elements
//fill the instruction and data memory
//reset the registers
*/
int init(char* filename) {
	FILE* fp = fopen(filename, "r");
	int i;
	long inst;

	if (fp == NULL) {
		fprintf(stderr, "Error opening file.\n");
		return -1;
	}

	/* fill instruction memory */
	i = 0;
	while (fscanf(fp, "%lx", &inst) == 1) {
		inst_mem[i++] = inst;
	}

	/*reset the registers*/
	for (i = 0; i < 32; i++) {
		regs[i] = 0;
	}

	/*reset pc*/
	pc = 0;
	/*reset clock cycles*/
	cycles = 0;
	return 0;
}

void print_cycles() {
	printf("---------------------------------------------------\n");
	printf("Clock cycles = %d\n", cycles);
}

void print_pc() {
	printf("PC	   = %ld\n\n", pc);
}

void print_reg() {
	printf("x0   = %d\n", regs[0]);
	printf("x1   = %d\n", regs[1]);
	printf("x2   = %d\n", regs[2]);
	printf("x3   = %d\n", regs[3]);
	printf("x4   = %d\n", regs[4]);
	printf("x5   = %d\n", regs[5]);
	printf("x6   = %d\n", regs[6]);
	printf("x7   = %d\n", regs[7]);
	printf("x8   = %d\n", regs[8]);
	printf("x9   = %d\n", regs[9]);
	printf("x10  = %d\n", regs[10]);
	printf("x11  = %d\n", regs[11]);
	printf("x12  = %d\n", regs[12]);
	printf("x13  = %d\n", regs[13]);
	printf("x14  = %d\n", regs[14]);
	printf("x15  = %d\n", regs[15]);
	printf("x16  = %d\n", regs[16]);
	printf("x17  = %d\n", regs[17]);
	printf("x18  = %d\n", regs[18]);
	printf("x19  = %d\n", regs[19]);
	printf("x20  = %d\n", regs[20]);
	printf("x21  = %d\n", regs[21]);
	printf("x22  = %d\n", regs[22]);
	printf("x23  = %d\n", regs[23]);
	printf("x24  = %d\n", regs[24]);
	printf("x25  = %d\n", regs[25]);
	printf("x26  = %d\n", regs[26]);
	printf("x27  = %d\n", regs[27]);
	printf("x28  = %d\n", regs[28]);
	printf("x29  = %d\n", regs[29]);
	printf("x30  = %d\n", regs[30]);
	printf("x31  = %d\n", regs[31]);
	printf("\n");
}
