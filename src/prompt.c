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

// Enumeration of possible lval types
enum { LVAL_NUM, LVAL_ERR };

// Enumeration of possible lval errors
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
    int type;
    long num;
    int err;
} lval;

lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lval v) {
    switch (v.type) {

        case LVAL_NUM:
            printf("%li", v.num);
            break;

        case LVAL_ERR:
            switch (v.err) {
                case LERR_DIV_ZERO:
                    printf("Error: division by zero");
                    break;
                case LERR_BAD_OP:
                    printf("Error: invalid operator");
                    break;
                case LERR_BAD_NUM:
                    printf("Error: invalid number");
                    break;
            }
            break;
    }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }
    if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num); 
    }
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    // Base case. If a node is tagged as a number, return it directly
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // Else, node is an expression. It's operator is the second child. The
    // first child is '('
    char* op = t->children[1]->contents;

    // Store the third child in 'x'
    lval x = eval(t->children[2]);

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
            lval result = eval(r.output);
            lval_println(result);
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
