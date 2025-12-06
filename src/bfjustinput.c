#include <stdio.h>

#define TAPE_SIZE 30000

void brainfuck(const char* code) {
    unsigned char tape[TAPE_SIZE] = {0};
    int ptr = 0;

    for (int i = 0; code[i] != '\0'; i++) {
        switch (code[i]) {
            case '>':
                ptr++;
                if (ptr >= TAPE_SIZE) {
                    printf("Error: Pointer out of bounds (right)\n");
                    return;
                }
                break;
            case '<':
                ptr--;
                if (ptr < 0) {
                    printf("Error: Pointer out of bounds (left)\n");
                    return;
                }
                break;
            case '+':
                tape[ptr]++;
                break;
            case '-':
                tape[ptr]--;
                break;
            case '.':
                putchar(tape[ptr]);
                break;
            case ',':
                tape[ptr] = getchar();
                break;
            case '[':
                if (tape[ptr] == 0) {
                    int depth = 1;
                    while (depth > 0) {
                        i++;
                        if (code[i] == '[') depth++;
                        if (code[i] == ']') depth--;
                    }
                }
                break;
            case ']':
                if (tape[ptr] != 0) {
                    int depth = 1;
                    while (depth > 0) {
                        i--;
                        if (code[i] == '[') depth--;
                        if (code[i] == ']') depth++;
                    }
                }
                break;
            default:
                // 忽略非指令字符
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <brainfuck_code>\n", argv[0]);
        return 1;
    }

    brainfuck(argv[1]);
    return 0;
}