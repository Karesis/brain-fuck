#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Node {
	struct Node* prev;
	struct Node* next;
	uint8_t data;
};

static struct Node TOMP = {.prev = nullptr, .next = nullptr, .data = UINT8_MAX};

bool node_is_tomp(struct Node* node)
{
	return node == &TOMP;
}

struct Tap {
	struct Node* cell;
	size_t cells_limit;
	size_t current_cells_size;
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

struct Node* create_chunk(size_t count)
{
	struct Node* cells = malloc(sizeof(struct Node) * count);
	if (cells == nullptr)
		return nullptr;

	memset(cells, 0, sizeof(struct Node) * count);

	for (size_t i = 0; i < count; ++i) {
		cells[i].prev = (i == 0) ? nullptr : &cells[i - 1];
		cells[i].next = (i == count - 1) ? nullptr : &cells[i + 1];
		cells[i].data = 0;
	}
	return cells;
}

bool tap_init(struct Tap* self, size_t initial_size, size_t max_cells)
{
	struct Node* cells = create_chunk(initial_size);
	if (!cells)
		return false;

	cells[0].prev		     = &TOMP;
	cells[initial_size - 1].next = &TOMP;

	self->cell		 = &cells[initial_size / 2];
	self->cells_limit	 = max_cells;
	self->current_cells_size = initial_size;
	return true;
}

void tap_deinit(struct Tap* self)
{
	if (self == nullptr || self->cell == nullptr)
		return;

	struct Node* head = self->cell;
	while (!node_is_tomp(head->prev)) {
		head = head->prev;
	}

	struct Node* current_chunk_start = head;
	while (!node_is_tomp(current_chunk_start)) {
		struct Node* scanner = current_chunk_start;
		while (!node_is_tomp(scanner->next) &&
		       (scanner->next == scanner + 1)) {
			scanner = scanner->next;
		}
		struct Node* next_chunk_start = scanner->next;
		free(current_chunk_start);
		current_chunk_start = next_chunk_start;
	}
	self->cell = nullptr;
}

bool tap_resize_right(struct Tap* self)
{
	if (self->current_cells_size >= self->cells_limit) {
		fprintf(stderr, "Error: Tape limit exceeded!\n");
		return false;
	}
	size_t size	       = 1024;
	struct Node* new_chunk = create_chunk(size);
	if (!new_chunk)
		return false;

	struct Node* old_tail	 = self->cell;
	old_tail->next		 = &new_chunk[0];
	new_chunk[0].prev	 = old_tail;
	new_chunk[size - 1].next = &TOMP;
	self->current_cells_size += 1024;
	return true;
}

bool tap_resize_left(struct Tap* self)
{
	if (self->current_cells_size >= self->cells_limit) {
		fprintf(stderr, "Error: Tape limit exceeded!\n");
		return false;
	}
	size_t size	       = 1024;
	struct Node* new_chunk = create_chunk(size);
	if (!new_chunk)
		return false;

	struct Node* old_head	 = self->cell;
	old_head->prev		 = &new_chunk[size - 1];
	new_chunk[size - 1].next = old_head;
	new_chunk[0].prev	 = &TOMP;
	self->current_cells_size += 1024;
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

	while (pc < prog->size) {
		struct Instruction instr = prog->ops[pc];

		switch (instr.type) {
		case OperationType_INC_PTR:
			for (size_t k = 0; k < instr.operand; k++) {
				if (node_is_tomp(self->cell->next)) {
					if (!tap_resize_right(self)) {
						fprintf(stderr,
							"Runtime Error: Memory "
							"limit exceeded\n");
						return;
					}
				}
				self->cell = self->cell->next;
			}
			break;
		case OperationType_DEC_PTR:
			for (size_t k = 0; k < instr.operand; k++) {
				if (node_is_tomp(self->cell->prev)) {
					if (!tap_resize_left(self)) {
						fprintf(stderr,
							"Runtime Error: Memory "
							"limit exceeded\n");
						return;
					}
				}
				self->cell = self->cell->prev;
			}
			break;
		case OperationType_ADD_VAL:
			self->cell->data += (uint8_t)instr.operand;
			break;
		case OperationType_SUB_VAL:
			self->cell->data -= (uint8_t)instr.operand;
			break;
		case OperationType_OUTPUT:
			putchar(self->cell->data);
			break;
		case OperationType_INPUT: {
			int c = getchar();
			if (c != EOF)
				self->cell->data = (uint8_t)c;
			break;
		}
		case OperationType_JUMP_ZERO:
			if (self->cell->data == 0) {
				pc = instr.operand;
			}
			break;
		case OperationType_JUMP_NONZERO:
			if (self->cell->data != 0) {
				pc = instr.operand;
			}
			break;
		case OperationType_SET_ZERO:
			self->cell->data = 0;
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