#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Tap {
	uint8_t* right;
	size_t right_cap;

	uint8_t* left;
	size_t left_cap;

	long pos;

	size_t limit;
};

enum OperationType {
	OperationType_INC_PTR,
	OperationType_DEC_PTR,
	OperationType_ADD_VAL,
	OperationType_SUB_VAL,
	OperationType_OUTPUT,
	OperationType_INPUT,
	OperationType_JUMP_ZERO,
	OperationType_JUMP_NONZERO,
	OperationType_SET_ZERO,
	OperationType_HALT
};

struct Instruction {
	enum OperationType type;
	size_t operand;
};

struct Program {
	struct Instruction* ops;
	size_t size;
	size_t capacity;
};

static inline uint8_t* tap_get_ptr(struct Tap* self)
{
	if (self->pos >= 0) {
		return &self->right[self->pos];
	} else {
		return &self->left[~self->pos];
	}
}

bool tap_init(struct Tap* self, size_t initial_size, size_t limit)
{
	self->right = calloc(initial_size, sizeof(uint8_t));
	self->left  = calloc(initial_size, sizeof(uint8_t));

	if (!self->right || !self->left) {
		if (self->right)
			free(self->right);
		if (self->left)
			free(self->left);
		return false;
	}

	self->right_cap = initial_size;
	self->left_cap	= initial_size;
	self->limit	= limit;
	self->pos	= 0;
	return true;
}

void tap_deinit(struct Tap* self)
{
	if (self->right)
		free(self->right);
	if (self->left)
		free(self->left);
	self->right = nullptr;
	self->left  = nullptr;
}

bool tap_move(struct Tap* self, long offset)
{
	self->pos += offset;

	if (self->pos >= 0) {
		if ((size_t)self->pos >= self->right_cap) {
			size_t new_cap = self->right_cap * 2;

			if (new_cap <= (size_t)self->pos)
				new_cap = (size_t)self->pos + 1;

			if (new_cap > self->limit) {
				fprintf(stderr, "Error: Tape limit exceeded "
						"(Right).\n");
				return false;
			}

			uint8_t* new_mem = realloc(self->right, new_cap);
			if (!new_mem)
				return false;

			memset(new_mem + self->right_cap, 0,
			       new_cap - self->right_cap);

			self->right	= new_mem;
			self->right_cap = new_cap;
		}
	} else {
		size_t index = ~self->pos;
		if (index >= self->left_cap) {
			size_t new_cap = self->left_cap * 2;
			if (new_cap <= index)
				new_cap = index + 1;

			if (new_cap > self->limit) {
				fprintf(stderr,
					"Error: Tape limit exceeded (Left).\n");
				return false;
			}

			uint8_t* new_mem = realloc(self->left, new_cap);
			if (!new_mem)
				return false;

			memset(new_mem + self->left_cap, 0,
			       new_cap - self->left_cap);

			self->left     = new_mem;
			self->left_cap = new_cap;
		}
	}
	return true;
}

bool program_init(struct Program* prog)
{
	prog->capacity = 1024;
	prog->size     = 0;
	prog->ops      = malloc(sizeof(struct Instruction) * prog->capacity);
	return prog->ops != nullptr;
}

void program_free(struct Program* prog)
{
	if (prog->ops)
		free(prog->ops);
	prog->ops      = nullptr;
	prog->size     = 0;
	prog->capacity = 0;
}

bool compile_source(const char* source, struct Program* prog)
{
	size_t len = strlen(source);
	size_t loop_stack[4096];
	int stack_top = -1;

	for (size_t i = 0; i < len; ++i) {
		char c			 = source[i];
		struct Instruction instr = {0};
		bool emit_instruction	 = true;

		switch (c) {
		case '>':
			instr.type    = OperationType_INC_PTR;
			instr.operand = 1;
			while (i + 1 < len && source[i + 1] == '>') {
				instr.operand++;
				i++;
			}
			break;
		case '<':
			instr.type    = OperationType_DEC_PTR;
			instr.operand = 1;
			while (i + 1 < len && source[i + 1] == '<') {
				instr.operand++;
				i++;
			}
			break;
		case '+':
			instr.type    = OperationType_ADD_VAL;
			instr.operand = 1;
			while (i + 1 < len && source[i + 1] == '+') {
				instr.operand++;
				i++;
			}
			break;
		case '-':
			instr.type    = OperationType_SUB_VAL;
			instr.operand = 1;
			while (i + 1 < len && source[i + 1] == '-') {
				instr.operand++;
				i++;
			}
			break;
		case '.':
			instr.type = OperationType_OUTPUT;
			break;
		case ',':
			instr.type = OperationType_INPUT;
			break;
		case '[':

			if (i + 2 < len &&
			    (source[i + 1] == '-' || source[i + 1] == '+') &&
			    source[i + 2] == ']') {
				instr.type = OperationType_SET_ZERO;
				i += 2;
			} else {
				instr.type = OperationType_JUMP_ZERO;
				stack_top++;
				if (stack_top >= 4096)
					return false;
				loop_stack[stack_top] = prog->size;
			}
			break;
		case ']':
			if (stack_top < 0)
				return false;
			size_t open_idx = loop_stack[stack_top];
			stack_top--;
			instr.type    = OperationType_JUMP_NONZERO;
			instr.operand = open_idx;
			prog->ops[open_idx].operand = prog->size;
			break;
		default:
			emit_instruction = false;
			break;
		}

		if (emit_instruction) {
			if (prog->size >= prog->capacity) {
				prog->capacity *= 2;
				struct Instruction* new_ops = realloc(
					prog->ops, sizeof(struct Instruction) *
							   prog->capacity);
				if (!new_ops)
					return false;
				prog->ops = new_ops;
			}
			prog->ops[prog->size++] = instr;
		}
	}

	if (stack_top >= 0) {
		fprintf(stderr, "Error: Unmatched '['\n");
		return false;
	}

	if (prog->size >= prog->capacity) {
		prog->capacity += 1;
		prog->ops = realloc(prog->ops, sizeof(struct Instruction) *
						       prog->capacity);
	}
	prog->ops[prog->size].type = OperationType_HALT;
	prog->size++;

	return true;
}

void tap_run(struct Tap* self, struct Program* prog)
{
	size_t pc = 0;

	uint8_t* curr_ptr = tap_get_ptr(self);

	while (pc < prog->size) {
		struct Instruction instr = prog->ops[pc];

		switch (instr.type) {
		case OperationType_INC_PTR:

			if (!tap_move(self, (long)instr.operand))
				return;
            // flush
			curr_ptr = tap_get_ptr(self);
			break;

		case OperationType_DEC_PTR:
			if (!tap_move(self, -(long)instr.operand))
				return;
            
            // flush
			curr_ptr = tap_get_ptr(self);
			break;

		case OperationType_ADD_VAL:

			*curr_ptr += (uint8_t)instr.operand;
			break;

		case OperationType_SUB_VAL:
			*curr_ptr -= (uint8_t)instr.operand;
			break;

		case OperationType_OUTPUT:
			putchar(*curr_ptr);
			break;

		case OperationType_INPUT: {
			int c = getchar();
			if (c != EOF)
				*curr_ptr = (uint8_t)c;
			break;
		}

		case OperationType_JUMP_ZERO:

			if (*curr_ptr == 0) {
				pc = instr.operand;
			}
			break;

		case OperationType_JUMP_NONZERO:
			if (*curr_ptr != 0) {
				pc = instr.operand;
			}
			break;

		case OperationType_SET_ZERO:
			*curr_ptr = 0;
			break;

		case OperationType_HALT:
			return;
		}
		pc++;
	}
}

char* read_file(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if (!f)
		return nullptr;
	fseek(f, 0, SEEK_END);
	long length = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buffer = malloc(length + 1);
	if (buffer) {
		fread(buffer, 1, length, f);
		buffer[length] = '\0';
	}
	fclose(f);
	return buffer;
}

struct Config {
	size_t tape_size;
	bool verbose;
	const char* filename;
	size_t max_cells_limit;
};

void print_usage(const char* prog_name)
{
	printf("Usage: %s [options] <file>\n", prog_name);
	printf("  -h, --help           Show help\n");
	printf("  -v, --verbose        Verbose output\n");
	printf("  -s, --size <cells>   Initial tape size (1024 cells "
	       "default)\n");
	printf("  -m, --max <cells>    Set max tape length limit (30000 cells "
	       "default)\n");
}

int main(int argc, char* argv[])
{
	struct Config config = {.tape_size	 = 1024,
				.verbose	 = false,
				.filename	 = nullptr,
				.max_cells_limit = 30000};

	static const struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"size", required_argument, 0, 's'},
		{"max", required_argument, 0, 'm'},
		{0}};

	int opt;
	while ((opt = getopt_long(argc, argv, "hvs:m:", long_options,
				  nullptr)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'v':
			config.verbose = true;
			break;
		case 's': {
			char* endptr;
			unsigned long long size = strtoull(optarg, &endptr, 10);
			if (*endptr != '\0' || size == 0)
				return EXIT_FAILURE;
			config.tape_size = (size_t)size;
			break;
		}
		case 'm': {
			char* endptr;
			unsigned long long limit =
				strtoull(optarg, &endptr, 10);
			if (*endptr != '\0' || limit == 0)
				return EXIT_FAILURE;
			config.max_cells_limit = (size_t)limit;
			break;
		}
		default:
			return EXIT_FAILURE;
		}
	}

	if (optind < argc) {
		config.filename = argv[optind];
	} else {
		fprintf(stderr, "Error: No input file.\n");
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	char* source_code = read_file(config.filename);
	if (!source_code) {
		perror("Failed to read file");
		return EXIT_FAILURE;
	}

	if (config.verbose)
		printf("Compiling...\n");

	struct Program program;
	if (!program_init(&program)) {
		fprintf(stderr, "Failed to init program memory.\n");
		free(source_code);
		return EXIT_FAILURE;
	}

	if (!compile_source(source_code, &program)) {
		fprintf(stderr, "Compilation Failed.\n");
		free(source_code);
		program_free(&program);
		return EXIT_FAILURE;
	}

	free(source_code);

	if (config.verbose)
		printf("Compilation success. Ops count: %zu\n", program.size);

	struct Tap tap;
	if (!tap_init(&tap, config.tape_size, config.max_cells_limit)) {
		fprintf(stderr, "Failed to initialize tap.\n");
		program_free(&program);
		return EXIT_FAILURE;
	}

	if (config.verbose)
		printf("Running...\n");
	tap_run(&tap, &program);

	tap_deinit(&tap);
	program_free(&program);
	return EXIT_SUCCESS;
}