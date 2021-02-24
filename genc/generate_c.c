#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main();
void gen_statement();
void gen_block();
void gen_expr();

int local_ints;
int local_ptrs;
int depth = 0;
int iterator_count = 0;
int last_assigned = 0;

int main(int argc, char** argv)
{
	int seed = time(NULL);
	if(argc == 2) {
		seed = atoi(argv[1]);
	}
	fprintf(stderr, "seed is: %d\n", seed);
	srand(seed);
	
	printf("#pragma GCC diagnostic ignored \"-Woverflow\"\n");
	printf("#ifdef __x86_64__\n");
	printf("int main() {\n");
	printf("#else\n");
	printf("int _start() {\n");
	printf("#endif\n");
	printf("int divtemp;");
	local_ints = 1 + rand() % 32;
	for(int i = 0; i < local_ints; i++) {
		printf("int l_%d = %d;\n", i, rand());
	}
	local_ptrs = 1 + rand() % 32;
	for(int i = 0; i < local_ptrs; i++) {
		printf("int* p_%d = &l_%d;", i, rand() % local_ints);
	}
	for(int i = 0; i < 10; i++) {
		gen_statement();
	}
	printf("return l_%d;\n", last_assigned);
	printf("}\n");
	return 0;
}

void gen_statement()
{
	depth++;
	switch(rand() % 3) {
		case 0: {
			if(depth < 3) {
				gen_block();
				break;
			}
		}
		case 1: { // assign integer
			last_assigned = rand() % local_ints;
			printf("l_%d = ", last_assigned);
			gen_expr();
			printf(";\n");
			break;
		}
		case 2: { // assign pointer
			printf("p_%d = &l_%d;\n", rand() % local_ptrs, rand() % local_ints);
			break;
		}
	}
	depth--;
}

void gen_block()
{
	depth++;
	switch(rand() % 2) {
		case 0: {
			printf("for(int i_%d = 0; i_%d < %d; i_%d++)",
				iterator_count, iterator_count, rand() % 1024, iterator_count);
			iterator_count++;
			break;
		}
		case 1: {
			printf("if(");
			gen_expr();
			printf(")\n");
			break;
		}
	}
	printf(" {\n");
	for(int i = 0; i < 3 - depth; i++) {
		gen_statement();
	}
	printf("}\n");
	depth--;
}

void gen_expr()
{
	depth++;
	if(depth > 5) {
		switch(rand() % 3) {
			case 0: printf("l_%d", rand() % local_ints); break;
			case 1: printf("*p_%d", rand() % local_ptrs); break;
			case 2: printf("%d", rand()); break;
		}
	} else {
		switch(rand() % 3) {
			case 0: {
				printf("(");
				gen_expr();
				printf(")+(");
				gen_expr();
				printf(")");
				break;
			}
			case 1: {
				printf("(");
				gen_expr();
				printf(")-(");
				gen_expr();
				printf(")");
				break;
			}
			case 2: {
				printf("(");
				gen_expr();
				printf(")*(");
				gen_expr();
				printf(")");
				break;
			}
			case 3: {
				printf("(divtemp=(");
				gen_expr();
				printf("), (");
				gen_expr();
				printf(")/(divtemp==0?1:divtemp))");
				break;
			}
		}
	}
	depth--;
}
