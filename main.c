#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strtok()

#include "cpu.h"
#include "print.h" // all printing functions

void print_usage() {
	printf("sim> ./sim <simulate/display> <number of cycles>\n");
}

int main(int argc, char* argv[]) {
	if(argc != 2) {
		printf("./sim <file.asm>\n");
		exit(1);
	}
	
	cpu_t* cpu = cpu_init(argv[1]);
	if(!cpu) {
		fprintf(stderr, "sim> Failed to initialize CPU\n");
		exit(1);
	}
	
	//cpu_run(cpu);
	//cpu_stop(cpu);

	int buf_size = 128;
	char command[buf_size];
	char* token;

	print_usage();
	while(1) {
		printf("sim> ");
		fgets(command, buf_size, stdin);
		token = strtok(command, " ");
		command[strcspn(command, "\r\n")] = 0; // removes new line, if present ; so that strcmp() will work

		if(strcmp(token, "simulate") == 0 || strcmp(token, "sim") == 0) {	
			if(!cpu) {
				fprintf(stderr, "sim> Did not initialize cpu.\n");
				continue;
			} 
			else if(cpu->done) {
				fprintf(stderr, "sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);
				continue;	
			}

			token = strtok(NULL, " "); // obtain number of cycles to simulate
			if(!token) {
				fprintf(stderr, "sim> Did not provide number of cycles to simulate.\n");
				continue;
			}
			token[strcspn(token, "\r\n")] = 0; // removes new line
			cpu->stop_cycle = cpu->clock + atoi(token);
			printf("sim> Simulating %s cycles.\n", token);		
			cpu_run(cpu, "simulate");	
			display(cpu);
			if(cpu->done) printf("sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);	

		} else if(strcmp(token, "display") == 0) {	
			token = strtok(NULL, " "); // obtain number of cycles to simulate
			if(!token || atoi(token) == 0) {
				display(cpu);	
				continue;
			}
			token[strcspn(token, "\r\n")] = 0; // removes new line
			cpu->stop_cycle = cpu->clock + atoi(token);
			printf("sim> Displaying %s cycles.\n", token);		
			cpu_run(cpu, "display");
			if(cpu->done) printf("sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);	

		} else if(strcmp(token, "quit") == 0 || strcmp(token, "q") == 0 ) {
 			printf("sim> Aufwiedersehen!\n");
			break;
		} else if(!token[0] || strcmp(token, "step") == 0) { // enter key was pressed
			if(!cpu) {
				fprintf(stderr, "sim> Did not initialize cpu.\n");
				continue;
			} 
			else if(cpu->done) {
				fprintf(stderr, "sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);
				continue;	
			}
			cpu->stop_cycle = cpu->clock + 1;
			printf("Displaying 1 cycle.\n");		
			cpu_run(cpu, "display");
			if(cpu->done) printf("sim> No more instructions to simulate. Completed at %i cycles.\n", cpu->clock);	

		} else {
			printf("sim> Invalid token: %s\n", token);
			print_usage();
		}
	}
		
	return 0;
}
