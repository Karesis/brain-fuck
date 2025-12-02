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

static struct Node TOMP = {.prev = nullptr, .next = nullptr, .data = INT_MAX};

bool node_is_tomp(struct Node* node)
{
	return node == &TOMP;
}

struct Tap {
	struct Node* cell;
	const char* codes;
	size_t code_length;
	size_t pc;
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

bool tap_init(struct Tap* self, const char* codes, int initial_size)
{
	struct Node* cells = create_chunk(initial_size);
	if (!cells)
		return false;

	cells[0].prev		     = &TOMP;
	cells[initial_size - 1].next = &TOMP;

	self->cell	  = &cells[initial_size / 2];
	self->codes	  = codes;
	self->code_length = strlen(codes);
	self->pc	  = 0;
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

	self->cell	  = nullptr;
	self->codes	  = nullptr;
	self->code_length = 0;
	self->pc	  = 0;
}

bool tap_resize_right(struct Tap* self)
{
	size_t size	       = 1024;
	struct Node* new_chunk = create_chunk(size);
	if (!new_chunk)
		return false;

	struct Node* old_tail = self->cell;

	old_tail->next	  = &new_chunk[0];
	new_chunk[0].prev = old_tail;

	new_chunk[size - 1].next = &TOMP;

	return true;
}

bool tap_resize_left(struct Tap* self)
{
	size_t size	       = 1024;
	struct Node* new_chunk = create_chunk(size);
	if (!new_chunk)
		return false;

	struct Node* old_head = self->cell;

	old_head->prev		 = &new_chunk[size - 1];
	new_chunk[size - 1].next = old_head;

	new_chunk[0].prev = &TOMP;

	return true;
}

bool tap_move_right(struct Tap* self)
{
	if (node_is_tomp(self->cell->next)) {
		if (!tap_resize_right(self))
			return false;
	}
	self->cell = self->cell->next;
	return true;
}

bool tap_move_left(struct Tap* self)
{
	if (node_is_tomp(self->cell->prev)) {
		if (!tap_resize_left(self))
			return false;
	}
	self->cell = self->cell->prev;
	return true;
}

bool tap_increment(struct Tap* self)
{
	self->cell->data++;
	return true;
}

bool tap_decrement(struct Tap* self)
{
	self->cell->data--;
	return true;
}

bool tap_output(struct Tap* self)
{
	if (putchar(self->cell->data) == EOF)
		return false;
	return true;
}

bool tap_input(struct Tap* self)
{
	int c = getchar();
	if (c != EOF)
		self->cell->data = (uint8_t)c;
	return true;
}

enum LoopResult {
	LoopResult_UNCLOSED,
	LoopResult_OK,
};

enum LoopResult tap_jump_forward(struct Tap* self)
{
	int nesting = 1;
	while (nesting > 0) {
		self->pc++;
		if (self->pc >= self->code_length)
			return LoopResult_UNCLOSED;

		char c = self->codes[self->pc];
		if (c == '[')
			nesting++;
		else if (c == ']')
			nesting--;
	}
	return LoopResult_OK;
}

enum LoopResult tap_jump_back(struct Tap* self)
{
	int nesting = 1;
	while (nesting > 0) {
		if (self->pc == 0)
			return LoopResult_UNCLOSED;
		self->pc--;

		char c = self->codes[self->pc];
		if (c == ']')
			nesting++;
		else if (c == '[')
			nesting--;
	}
	return LoopResult_OK;
}

void tap_run(struct Tap* self)
{
	while (self->pc < self->code_length) {
		char c	= self->codes[self->pc];
		bool ok = true;

		switch (c) {
		case '>':
			ok = tap_move_right(self);
			break;
		case '<':
			ok = tap_move_left(self);
			break;
		case '+':
			ok = tap_increment(self);
			break;
		case '-':
			ok = tap_decrement(self);
			break;
		case '.':
			ok = tap_output(self);
			break;
		case ',':
			ok = tap_input(self);
			break;
		case '[':
			if (self->cell->data == 0) {
				if (tap_jump_forward(self) != LoopResult_OK) {
					fprintf(stderr,
						"Error: Unclosed loop '[' at "
						"pos %zu\n",
						self->pc);
					return;
				}
			}
			break;
		case ']':
			if (self->cell->data != 0) {
				if (tap_jump_back(self) != LoopResult_OK) {
					fprintf(stderr,
						"Error: Unclosed loop ']' at "
						"pos %zu\n",
						self->pc);
					return;
				}
			}
			break;
		default:
			break;
		}

		if (!ok) {
			fprintf(stderr, "Runtime Error at pos %zu\n", self->pc);
			return;
		}
		self->pc++;
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
	int tape_size;
	bool verbose;
	const char* filename;
};

void print_usage(const char* prog_name)
{
	printf("Usage: %s [options] <file>\n", prog_name);
	printf("  -h, --help           Show help\n");
	printf("  -v, --verbose        Verbose output\n");
	printf("  -s, --size <bytes>   Initial tape size\n");
}

int main(int argc, char* argv[])
{
	struct Config config = {
		.tape_size = 1024, .verbose = false, .filename = nullptr};

	static const struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"size", required_argument, 0, 's'},
		{0}};

	int opt;
	while ((opt = getopt_long(argc, argv, "hvs:", long_options, nullptr)) !=
	       -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'v':
			config.verbose = true;
			break;
		case 's':
			config.tape_size = atoi(optarg);
			break;
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

	if (config.verbose)
		printf("Loading %s...\n", config.filename);

	char* codes = read_file(config.filename);
	if (!codes) {
		perror("Failed to read file");
		return EXIT_FAILURE;
	}

	struct Tap tap;
	if (!tap_init(&tap, codes, config.tape_size)) {
		fprintf(stderr, "Failed to initialize tap.\n");
		free(codes);
		return EXIT_FAILURE;
	}

	if (config.verbose)
		printf("Running...\n");
	tap_run(&tap);

	free(codes);
	tap_deinit(&tap);
	return EXIT_SUCCESS;
}