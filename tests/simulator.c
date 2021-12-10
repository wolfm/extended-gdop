/**
 * Project 1
 * EECS 370 LC-2K Instruction-level simulator
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000

typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;

void printState(stateType *);
int convertNum(int);
void addCmd(stateType *, int, int, int);
void norCmd(stateType *, int, int, int);
void lwAndswCmd(stateType *, int, int, int, int);
void beqCmd(stateType *, int, int, int);
void jalrCmd(stateType *, int, int);

int
main(int argc, char *argv[])
{
    char line[MAXLINELENGTH];
    stateType state;
    FILE *filePtr;

	const char *s = "hello";
	char ss[20];
	int length = strlen(s);
	for (int i = 0; i < length; ++i)
		ss[i] = s[length - i];
	printf("%s", ss);

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    filePtr = fopen(argv[1], "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", argv[1]);
        perror("fopen");
        exit(1);
    }

    /* read the entire machine-code file into memory */
    for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
            state.numMemory++) {

        if (sscanf(line, "%d", state.mem+state.numMemory) != 1) {
            printf("error in reading address %d\n", state.numMemory);
            exit(1);
        }
        printf("memory[%d]=%d\n", state.numMemory, state.mem[state.numMemory]);
    }

	//should check to see if state.numMemory is equal to the # of lines

	//resets all registers to zero, the program counter to 0
	for (int i = 0; i < 8; ++i) {
		state.reg[i] = 0;
	}
	state.pc = 0;
	
	//now we will be going into each function and performing their function
	//we will be using callee save to ensure that each function only needs
	//to store the variables it uses (i.e. save registers before using them in
	//a certain problem)
	int halt = 0;
	
	while (!halt) {
		int currMachineCode = state.mem[state.pc]; //current line of code
		int regA = ((currMachineCode >> 19));
		regA = regA & 0b00000111;
		int regB = ((currMachineCode >> 16));
		regB = regB & 0b111;
		int destReg = (currMachineCode & 7);
		int offsetField = currMachineCode & 0xffff;
		printState(&state);
		
		if ((currMachineCode >> 22) == 0) { //add
			addCmd(&state, regA, regB, destReg);
		}
		else if ((currMachineCode >> 22) == 1) { //nor
			norCmd(&state, regA, regB, destReg);
		}
		else if ((currMachineCode >> 22) == 2)
			lwAndswCmd(&state, regA, regB, offsetField, 1);
		else if ((currMachineCode >> 22) == 3)
			lwAndswCmd(&state, regA, regB, offsetField, 0);
		else if ((currMachineCode >> 22) == 4) 
			beqCmd(&state, regA, regB, offsetField);
		else if ((currMachineCode >> 22) == 5) 
			jalrCmd(&state, regA, regB);
		else if ((currMachineCode >> 22) == 6) {
			++state.pc;
			halt = 1;
		}
		else ++state.pc; //increment for noop
	}//for
	
	printState(&state);
    return(0);
}

void addCmd(stateType *statePtr, int regA, int regB, int destReg) {
	/*add the two desired regs (should be passed in as vars) and store
	it in destReg and also increment the pc*/
	statePtr->reg[destReg] = statePtr->reg[regA] + statePtr->reg[regB];
	++statePtr->pc;
}

void norCmd(stateType *statePtr, int regA, int regB, int destReg) {
	/*or the two registers together, then pass it to the convertNum function
	to be "nor'd". Then store the new value in destReg and also increment the pc*/
	int temp = statePtr->reg[regA] | statePtr->reg[regB];
	statePtr->reg[destReg] = ~temp;
	++statePtr->pc;
}

void lwAndswCmd(stateType *statePtr, int regA, int regB, int offsetField, int lw) {
	/*add regA & offset field together, then load or store the value at that 
	address into regB. also increment the pc*/
	int temp = statePtr->reg[regA] + convertNum(offsetField);
	if (lw) statePtr->reg[regB] = statePtr->mem[temp];
	else statePtr->mem[temp] = statePtr->reg[regB];
	++statePtr->pc;
}

void beqCmd(stateType *statePtr, int regA, int regB, int offsetField) {
	/*check to make sure that the contents of regA and regB are the same. if so,
	set the pc to pc+1+offsetField, where PC is the address of the beq instruction*/
	if (statePtr->reg[regA] == statePtr->reg[regB]) {
		statePtr->pc += 1 + convertNum(offsetField);
	}
	else ++statePtr->pc;
}

void jalrCmd(stateType *statePtr, int regA, int regB) {
	/*store pc+1 into regB (where pc is the address of the jalr instruction). then
	branch to the address in regA (if regA == regB, this means jumping to pc+1)*/
	statePtr->reg[regB] = statePtr->pc + 1;
	if (regA == regB) ++statePtr->pc;
	else statePtr->pc = statePtr->reg[regA];
}

void
printState(stateType *statePtr)
{
    int i;
    printf("\n@@@\nstate:\n");
    printf("\tpc %d\n", statePtr->pc);
    printf("\tmemory:\n");
    for (i=0; i<statePtr->numMemory; i++) {
              printf("\t\tmem[ %d ] %d\n", i, statePtr->mem[i]);
    }
    printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
              printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
    printf("end state\n");
}

int
convertNum(int num)
{
    /* convert a 16-bit number into a 32-bit Linux integer */
    if (num & (1<<15) ) {
        num -= (1<<16);
    }
    return(num);
}

