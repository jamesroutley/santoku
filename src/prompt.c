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

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

// Enumeration of possible lval types
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

// Enumeration of possible lval errors
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct lval {
    int type;
    long num;
    // Error and Symbol types have some string data
    char* err;
    char* sym;
    // Count and pointer to a list of 'lval*'
    int count;
    struct lval** cell;
} lval;

lval* lval_num(long x);
lval* lval_err(char* m);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
void lval_del(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);

lval* lval_read(mpc_ast_t* t);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_add(lval* v, lval* x);

void lval_println(lval* v);
void lval_print(lval* v);
void lval_expr_print(lval* v, char open, char close);

lval* lval_eval_sexpr(lval* v);
lval* lval_eval(lval* v);

lval* builtin(lval* a, char* func);
lval* builtin_op(lval* a, char* op);
lval* builtin_head(lval* a);
lval* builtin_tail(lval* a);
lval* builtin_list(lval* a);
lval* builtin_eval(lval* a);
lval* builtin_join(lval* a);

int main(int argc, char** argv) {
    // Create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
        " \
            number  : /-?[0-9]+/ ;                                  \
            symbol  : \"list\" | \"head\" | \"tail\" | \"join\"     \
                    | \"eval\" | '+' | '-' | '*' | '/' ;            \
            sexpr   : '(' <expr>* ')' ;                             \
            qexpr   : '{' <expr>* '}' ;                             \
            expr    : <number> | <symbol> | <sexpr> | <qexpr> ;     \
            lispy   : /^/ <expr>* /$/ ;                             \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy version 0.0.1");
    puts("Press ctrl+c to exit");

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}


lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: break;
        case LVAL_SYM: break;
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
               lval_del(v->cell[i]);
            }
            // also free memory allocated to contain the pointers
            free(v->cell);
            break;
    }
    // Free memory allocated to the lval struct itself
    free(v);
}

/*
 * Pop the LVAL at v's ith cell
 */
lval* lval_pop(lval* v, int i) {
    // Find item at i
    lval* x = v->cell[i];

    // Shift memory after the item at i over the top
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    // Decrease count of items in the list
    v->count--;

    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

/*
 * Return the LVAL at v's ith cell. Delete v.
 */
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

/*
 * Add each cell in y to x, delete y
 */
lval* lval_join(lval*x, lval*y) {
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

/*
 * Recursively read the AST into a tree of LVAL nodes.
 */
lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // If root ('>') or sexpr, create an empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    // Fill in the list with any valid expressions
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

/*
 * Create an LVAL number node
 *
 * Converts the number string at t->contents to a c long.
 * If the number can't be parsed, returns an LVAL error node.
 */
lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

/*
 * Add x to v's cell array.
 */
lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

/*
 * Print an LVAL and a newline
 */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

/*
 * Print an LVAL
 */
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

/*
 * Print an LVAL s expression
 */
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/*
 * Recursively evaluate the LVAL AST
 */
lval* lval_eval(lval* v) {
    // Evaluate s expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    return v;
}

/*
 * Recursively evaluate an LVAL s-expression and its children
 */
lval* lval_eval_sexpr(lval* v) {
    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // Empty expression
    if (v->count == 0) { return v; }

    // Single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // Ensure first element is a symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("s-expression does not start with symbol");
    }

    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-*/", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("unknown function");
}

/*
 * Apply the builtin operation op to each of the nodes in a->cell
 */
lval* builtin_op(lval* a, char* op) {
    // Ensure all arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("cannot operate on a non-number");
        }
    }

    // If operation is subtract and there's one argument, negate it
    lval* x = lval_pop(a, 0);
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    // While there are remaining elements, pop them and combine with the first
    // using the operation op
    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("division by zero");
                break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

/*
 * Return the first element of a q-expression
 */
lval* builtin_head(lval* a) {
    LASSERT(a, a->count == 1, "function 'head' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "function 'head' passed incorrect type");
    LASSERT(a, a->cell[0]->count != 0, "function 'head' passed {}");

    lval* v = lval_take(a, 0);
    // Delete all elements which aren't the head and return
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/*
 * Return all but the first element of a q-expression
 */
lval* builtin_tail(lval* a) {
    LASSERT(a, a->count == 1, "function 'tail' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "function 'tail' passed incorrect type");
    LASSERT(a, a->cell[0]->count != 0, "function 'tail' passed {}");

    lval* v = lval_take(a, 0);
    // Delete first element and return
    lval_del(lval_pop(v, 0));
    return v;
}

/*
 * Convert an s-expression to a q-expression
 */
lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/*
 * Evaluate a q-expression as an s-expression
 */
lval* builtin_eval(lval* a) {
    LASSERT(a, a->count == 1, "function 'eval' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "function 'eval' passed incorrect type");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

/*
 * Join two or more q-expressions
 */
lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, 
            "function 'join' passed incorrect type");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}
