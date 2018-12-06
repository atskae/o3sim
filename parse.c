#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

static int str_to_int(char* buffer) {
	char str[16];
	int j = 0;
	for(int i = 1; buffer[i] != '\0'; ++i) {
		str[j] = buffer[i];
		j++;
	}
	str[j] = '\0';
	return atoi(str);
}

static void creat_insn(insn_t* ins, char* buffer) {
	
	char* token = strtok(buffer, ",");
	int token_num = 0;
	char tokens[6][128];
	while (token != NULL) {
		strcpy(tokens[token_num], token);
		token_num++;
		token = strtok(NULL, ",");
	}
	
	strcpy(ins->opcode, tokens[0]);
	
	if(strcmp(ins->opcode, "MOVC") == 0) {
		ins->rd = str_to_int(tokens[1]);
		ins->imm = str_to_int(tokens[2]);
	}
	
	if (strcmp(ins->opcode, "STORE") == 0) {
		ins->rs1 = str_to_int(tokens[1]);
		ins->rs2 = str_to_int(tokens[2]);
		ins->imm = str_to_int(tokens[3]);
	}
	
}

/* Parses .asm file */
insn_t* create_code(const char* filename, int* size) {

	if(!filename) return NULL;
	
	FILE* fd = fopen(filename, "r");
	if(!fd) return NULL;
	
	char* line = NULL;
	size_t len = 0;
	ssize_t nread;
	int code_size = 0;
	
	while((nread = getline(&line, &len, fd)) != -1) {
		code_size++;
	}
	*size = code_size;
	if(!code_size) {
		fclose(fd);
		return NULL;
	}
	
	insn_t* code = malloc(sizeof(*code) * code_size);
	if(!code) {
		fclose(fd);
		return NULL;
	}
	
	rewind(fd);
	int insn = 0;
	while( (nread = getline(&line, &len, fd)) != -1 ) {
		creat_insn(&code[insn], line);
		insn++;
	}
	
	free(line);
	fclose(fd);
	return code;
}
