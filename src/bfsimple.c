#include <stdio.h>
#include <stdlib.h>

#define TAPE_SIZE 30000

void brainfuck(const char* code) {
    unsigned char tape[TAPE_SIZE] = {0};
    int ptr = 0;

    // 使用 int 而不是 unsigned char，防止某些极端情况下的溢出
    for (int i = 0; code[i] != '\0'; i++) {
        switch (code[i]) {
            case '>':
                ptr++;
                if (ptr >= TAPE_SIZE) {
                    fprintf(stderr, "Error: Pointer out of bounds (right)\n");
                    return;
                }
                break;
            case '<':
                ptr--;
                if (ptr < 0) {
                    fprintf(stderr, "Error: Pointer out of bounds (left)\n");
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
            case ',': {
                int c = getchar();
                if (c != EOF) {
                    tape[ptr] = (unsigned char)c;
                }
                // 如果是 EOF，通常做法是不改变 tape[ptr]，或者置 0
                // 这里选择不改变，兼容性更好
                break;
            }
            case '[':
                if (tape[ptr] == 0) {
                    int depth = 1;
                    while (depth > 0) {
                        i++;
                        // 安全检查：如果代码结束了还没找到匹配的 ]
                        if (code[i] == '\0') {
                            fprintf(stderr, "Error: Unmatched '[' at index %d\n", i);
                            return;
                        }
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
                        // 安全检查：如果倒回到开头还没找到匹配的 [
                        if (i < 0) {
                            fprintf(stderr, "Error: Unmatched ']'\n");
                            return;
                        }
                        if (code[i] == '[') depth--;
                        if (code[i] == ']') depth++;
                    }
                    // 这里不需要 i--，因为外层循环结束后会 i++，
                    // 下一次迭代会执行 [ 后面的一条指令。
                    // 逻辑：回跳到 [ 的位置，循环结束 i++，执行 [ 里面的第一个字符。
                    // 注意：这实际上跳过了 [ 的再次判断，但在 Brainfuck 逻辑中，
                    // ] 跳转发生意味着 tape[ptr] != 0，所以 [ 的条件必然满足。
                }
                break;
            default:
                break;
        }
    }
}

// 辅助函数：读取文件内容
char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char* code = read_file(argv[1]);
    if (!code) {
        fprintf(stderr, "Error: Could not read file %s\n", argv[1]);
        return 1;
    }

    brainfuck(code);
    
    free(code);
    return 0;
}