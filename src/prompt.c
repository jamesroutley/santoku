#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

// Compile our own readline function on Windows
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

// Fake add_history function. History is implemented by default on Windows
void add_history(char* unusued) {}

#else
// On *nix, include editline, which implements these functions for us.
#include <editline/readline.h>
#endif

int number_of_nodes(mpc_ast_t* t) {
    if (t->children_num == 0) { return 1; }
    if (t->children_num == 1) {
        int total = 1;
        for (int i = 0; i < t->children_num; i++) {
            total += number_of_nodes(t->children[i]);
        }
        return total;
    }
    return 0;
}

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    return 0;
}

long eval(mpc_ast_t* t) {
    // Base case. If a node is tagged as a number, return it directly
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    // Else, node is an expression. It's operator is the second child. The
    // first child is '('
    char* op = t->children[1]->contents;

    // Store the third child in 'x'
    long x = eval(t->children[2]);

    // Iterate the remaining children and combine
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv) {
    // Create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
        " \
            number : /-?[0-9]+/ ; \
            operator : '+' | '-' | '*' | '/' ; \
            expr : <number> | '(' <operator> <expr>+ ')' ; \
            lispy : /^/ <operator> <expr>+ /$/ ; \
        ",
        Number, Operator, Expr, Lispy);

    puts("Lispy version 0.0.1");
    puts("Press ctrl+c to exit");

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
