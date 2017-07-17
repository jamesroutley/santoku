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

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_TYPE(func, args, index, expected) \
    LASSERT(args, args->cell[index]->type == expected, \
        "function '%s' argument %d was type %s, expected %s", func, index, \
        ltype_name(args->cell[index]->type), ltype_name(expected))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
            "function '%s' passed incorrect number of arguments. Expected " \
            "%d, got %d", func, num, args->count)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
        "function '%s' passed {} for argument %d", func, index)

// Forward declarations

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// Enumeration of possible lval types
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_BOOL, LVAL_STR, LVAL_FUN, 
    LVAL_SEXPR, LVAL_QEXPR };

// Enumeration of possible lval errors
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;

    // Fields for basic LVAL types
    long num;
    char* err;
    char* sym;
    int bool;
    char* str;

    // Fields for funcion LVAL types
    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    // Fields for expression LVAL types
    int count;
    struct lval** cell;
};

char* ltype_name(int t);

lenv* lenv_new(void);
void lenv_del(lenv* e);
lval* lenv_get(lenv* e, lval* k);
void lenv_def(lenv*e, lval* k, lval* v);
void lenv_put(lenv* e, lval* k, lval* v);

lval* lval_num(long x);
lval* lval_err(char* fmt, ...);
lval* lval_sym(char* s);
lval* lval_bool(int b);
lval* lval_str(char* s);
lval* lval_fun(lbuiltin func);
lval* lval_sexpr(void);
lval* lval_qexpr(void);

lval* lval_lambda(lval* formals, lval* body);
void lval_del(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);
lval* lval_copy(lval* v);

lval* lval_read(mpc_ast_t* t);
lval* lval_read_str(mpc_ast_t* t);
lval* lval_read_num(mpc_ast_t* t);
lval* lval_add(lval* v, lval* x);
int lval_eq(lval* x, lval* y);
lval* lval_call(lenv* e, lval* f, lval* a);

void lval_println(lval* v);
void lval_print(lval* v);
void lval_print_str(lval* v);
void lval_expr_print(lval* v, char open, char close);

lval* lval_eval_sexpr(lenv* e, lval* v);
lval* lval_eval(lenv* e, lval* v);

void lenv_add_builtins(lenv* e);
void lenv_add_builtin(lenv* e, char* name, lbuiltin func);

lval* builtin(lval* a, char* func);
lval* builtin_add(lenv* e, lval* a);
lval* builtin_sub(lenv* e, lval* a);
lval* builtin_mul(lenv* e, lval* a);
lval* builtin_op(lenv* e, lval* a, char* op);

lval* builtin_eq(lenv* e, lval* a);
lval* builtin_neq(lenv* e, lval* a);
lval* builtin_cmp(lenv* e, lval* a, char* op);

lval* builtin_gt(lenv* e, lval* a);
lval* builtin_ge(lenv* e, lval* a);
lval* builtin_lt(lenv* e, lval* a);
lval* builtin_le(lenv* e, lval* a);
lval* builtin_div(lenv* e, lval* a);
lval* builtin_ord(lenv* e, lval* a, char* cmp);

lval* builtin_head(lenv* e, lval* a);
lval* builtin_tail(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_join(lenv* e, lval* a);
lval* builtin_def(lenv* e, lval* a);
lval* builtin_put(lenv* e, lval* a);
lval* builtin_var(lenv* e, lval* a, char* func);
lval* builtin_lambda(lenv* e, lval* a);
lval* builtin_if(lenv* e, lval*a);

int main(int argc, char** argv) {
    // Create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Bool = mpc_new("bool");
    mpc_parser_t* String = mpc_new("string");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // Define parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
        " \
            number  : /-?[0-9]+/ ;                                  \
            symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;            \
            bool    : /#[tf]/ ;                                     \
            string  : /\"(\\\\.|[^\"])*\"/ ;                        \
            sexpr   : '(' <expr>* ')' ;                             \
            qexpr   : '{' <expr>* '}' ;                             \
            expr    : <number> | <symbol> | <bool> | <string>       \
                    | <sexpr> | <qexpr> ;                           \
            lispy   : /^/ <expr>* /$/ ;                             \
        ",
        Number, Symbol, Bool, String, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy version 0.0.1");
    puts("Press ctrl+c to exit");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1) {
        char* input = readline("lispy> ");
        add_history(input);

        // Attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(5, Number, Symbol, Bool, String, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_BOOL: return "Boolean";
        case LVAL_STR: return "String";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

/*
 * Conjure a new lenv
 */
lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

/*
 * Delete the lenv e
 */
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/*
 * Return a copy of the value associated with the symbol k->sym
 */
lval* lenv_get(lenv* e, lval* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("unbound symbol '%s'", k->sym);
    }
}

/*
 * Put a sym, val pair in the top-level global environment
 */
void lenv_def(lenv*e, lval* k, lval* v) {
    while (e->par) { e = e->par; }
    lenv_put(e, k, v);
}

/*
 * Put a new sym, val pair in the lenv e
 *
 * If sym k previously existed, replace it's val with a copy of v. If it
 * didn't, add it.
 */
void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count-1], k->sym);
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    // Allocate 152 bytes of space for the error message
    v->err = malloc(512);

    // Print the error string (max 511 chars)
    vsnprintf(v->err, 511, fmt, va);

    // Reallocate to number of bytes acutally used
    v->err = realloc(v->err, strlen(v->err) + 1);

    // Clean up va list
    va_end(va); 

    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_bool(int b) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_BOOL;
    v->bool = b;
    return v;
}

lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_STR;
    v->str = malloc(strlen(s) + 1);
    strcpy(v->str, s);
    return(v);
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
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

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    v->builtin = NULL;

    v->env = lenv_new();

    v->formals = formals;
    v->body = body;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: break;
        case LVAL_SYM: break;
        case LVAL_BOOL: break;
        case LVAL_STR: break;
        case LVAL_FUN:
           if (!v->builtin) {
               lenv_del(v->env);
               lval_del(v->formals);
               lval_del(v->body);
           }
           break;
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

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_NUM: x->num = v->num; break;
        case LVAL_BOOL: x->bool = v->bool; break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        
        case LVAL_STR:
            x->str = malloc(strlen(v->str) + 1);
            strcpy(x->str, v->str);
            break;
        
        case LVAL_FUN: 
            if (v->builtin) {
               x->builtin = v->builtin; 
            } else {
               x->builtin = NULL;
               x->env = lenv_copy(v->env);
               x->formals = lval_copy(v->formals);
               x->body = lval_copy(v->body);
            }
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

/*
 * Recursively read the AST into a tree of LVAL nodes.
 */
lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
    if (strstr(t->tag, "bool")) {
        if (strcmp(t->contents, "#t") == 0) {
            return lval_bool(1);
        } else {
            return lval_bool(0);
        }
    }
    if (strstr(t->tag, "string")) { return lval_read_str(t); }

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
 * Create an LVAL string node
 */
lval* lval_read_str(mpc_ast_t* t) {
    // Remove quote characters (") from around raw string
    t->contents[strlen(t->contents) - 1] = '\0';
    char* unescaped = malloc(strlen(t->contents+1) + 1);
    strcpy(unescaped, t->contents+1);
    // Pass though unescape function
    unescaped = mpcf_unescape(unescaped);

    // Construct new LVAL
    lval* str = lval_str(unescaped);
    free(unescaped);
    return str;
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
 * Returns an 1 or 0 indicating whether x and y are equal.
 */
int lval_eq(lval* x, lval* y) {
    if (x->type != y->type) { return 0; }
    switch (x->type) {
        case LVAL_NUM: { return x->num == y->num; }
        case LVAL_BOOL: { return x->bool == y->bool; }
        case LVAL_ERR: { return strcmp(x->err, y->err) == 0; }
        case LVAL_SYM: { return strcmp(x->sym, y->sym) == 0; }
        case LVAL_STR: { return strcmp(x->str, y->str) == 0; }

        case LVAL_FUN: {
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            } else {
                return lval_eq(x->formals, y->formals) && 
                    lval_eq(x->body, y->body);
            }
        }
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            return 1;
    }
    return 0;
}

/*
 * Call the function f with arguments a
 *
 * If the number of arguments < number of f's formals, partially evaluate
 * f and return the partially evaluated function.
 */
lval* lval_call(lenv* e, lval* f, lval* a) {
    // If function is builtin, call it
    if (f->builtin) { return f->builtin(e, a); }

    // Record arg counts
    int given = a->count;
    int total = f->formals->count;

    while (a->count) {
        // If we run out of formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a);
            return lval_err(
                "Function passed too many arguments. "
                "Expected %d, got %d", total, given);
        }

        // Pop the first symbol from the formals
        lval* sym = lval_pop(f->formals, 0);

        // Check for &, which indicates a function with a variable number of
        // arguments
        if (strcmp(sym->sym, "&") == 0) {
            // Ensure '&' is followed by another symbol
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("function format invalid. "
                    "Symbol '&' not followed by a single symbol");
            }

            // Next formal should be bound to the remaining arguments
            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        // Pop the next argument from the list
        lval* val = lval_pop(a, 0);

        // Bind a copy into the function's environment
        lenv_put(f->env, sym, val);

        // Delete symbol and value
        lval_del(sym); lval_del(val);
    }

    // Argument list is now bound, so it can be cleaned up
    lval_del(a);

    // If '&' remains in the formal list, bind to an empty list
    if (f->formals->count > 0 &&
            strcmp(f->formals->cell[0]->sym, "&") == 0) {

        // Check to ensure '&' is not passed invalidly
        if (f->formals->count != 2) {
            return lval_err("function format invalid. "
                "Symbol '&' not followed by a single symbol");
        }

        // Pop and delete '&'
        lval_del(lval_pop(f->formals, 0));

        // Pop next symbol and create empty list
        lval* sym = lval_pop(f->formals, 0);
        lval* val = lval_qexpr();

        // Bind to environment and delete
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    // If all formals have been bound, evaluate
    if (f->formals->count == 0) {
        f->env->par = e;
        return builtin_eval(
            f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        // Otherwise return the partially evaluated function
        return lval_copy(f);
    }
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
        case LVAL_STR: lval_print_str(v); break;
        case LVAL_BOOL:
           if (v->bool) {
               printf("#t"); break;
           } else {
               printf("#f"); break;
           }
        case LVAL_FUN: 
            if (v->builtin) {
                printf("<builtin>");
            } else {
                printf("(\\ ");
                lval_print(v->formals);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
    }
}

void lval_print_str(lval* v) {
    char* escaped = malloc(strlen(v->str) + 1);
    strcpy(escaped, v->str);

    escaped = mpcf_escape(escaped);

    printf("\"%s\"", escaped);

    free(escaped);
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
lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    // Evaluate s expressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
    return v;
}

/*
 * Recursively evaluate an LVAL s-expression and its children
 */
lval* lval_eval_sexpr(lenv* e, lval* v) {
    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // Empty expression
    if (v->count == 0) { return v; }

    // Single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // Ensure first element is a function 
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "s-expression starts with incorrect type. "
            "Expected %s, got %s", ltype_name(LVAL_FUN), ltype_name(f->type));
        lval_del(f);
        lval_del(v);
        return err;
    }

    // Call function
    lval* result = lval_call(e, f, v);
    lval_del(f);
    return result;
}

void lenv_add_builtins(lenv* e) {
    // List Functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    // Mathematical Functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    // Comarison functions
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_neq);
    lenv_add_builtin(e, ">", builtin_gt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<", builtin_lt);
    lenv_add_builtin(e, "<=", builtin_le);

    // Branching
    lenv_add_builtin(e, "if", builtin_if);

    // Variable functions
    lenv_add_builtin(e, "def", builtin_def);

    // Lambdas
    lenv_add_builtin(e, "\\", builtin_lambda);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

/*
 * Apply the builtin operation op to each of the nodes in a->cell
 */
lval* builtin_op(lenv* e, lval* a, char* op) {
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
        if (strcmp(op, "==") == 0) {  }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_neq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    if (strcmp(op, "==") == 0) { 
        return lval_bool(lval_eq(a->cell[0], a->cell[1])); }
    if (strcmp(op, "!=") == 0) { 
        return lval_bool(!lval_eq(a->cell[0], a->cell[1])); }
    return lval_err("unrecognised comparison operator: '%s'", op);
}

lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}

lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}

lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(op, a, i, LVAL_NUM);
    }

    lval* x = lval_pop(a, 0);
    lval* y = lval_pop(a, 0);

    if (strcmp(op, "<") == 0) { return lval_bool(x->num < y->num); }
    if (strcmp(op, ">") == 0) { return lval_bool(x->num > y->num); }
    if (strcmp(op, "<=") == 0) { return lval_bool(x->num <= y->num); }
    if (strcmp(op, ">=") == 0) { return lval_bool(x->num >= y->num); }
    if (strcmp(op, "==") == 0) { return lval_bool(lval_eq(x, y)); }
    if (strcmp(op, "!=") == 0) { return lval_bool(!lval_eq(x, y)); }
    return lval_err("undefined comparison operator '%s'", op);
}

/*
 * Return the first element of a q-expression
 */
lval* builtin_head(lenv* e, lval* a) {
    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    lval* v = lval_take(a, 0);
    // Delete all elements which aren't the head and return
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

/*
 * Return all but the first element of a q-expression
 */
lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v = lval_take(a, 0);
    // Delete first element and return
    lval_del(lval_pop(v, 0));
    return v;
}

/*
 * Convert an s-expression to a q-expression
 */
lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

/*
 * Evaluate a q-expression as an s-expression
 */
lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

/*
 * Join two or more q-expressions
 */
lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

/*
 * Bind a list of symbols in a q-expression to values
 */
lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval* syms = a->cell[0];
    
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "function '%s' can only define items of type %s. Argument %d is "
            "type %s", func,
            ltype_name(LVAL_SYM), i, ltype_name(syms->cell[i]->type));
    }

    LASSERT(a, syms->count == a->count-1, 
        "function '%s' cannot define an incorrect number of values to symbols. "
        "Num values: %d, num symbols: %d", func, syms->count, a->count-1);

    for (int i = 0; i < syms->count; i++) {
        // Def defines globally, '=' defines locally
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        }

        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i+1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    // Check the first q-expression only contains symbols
    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "cannot define a non-symbol. Got %s, expected %s",
            ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // Pop the first two arguments and pass them to lval_lambda
    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* builtin_if(lenv* e, lval*a) {
    LASSERT(a, a->count == 2 || a->count == 3, "'if' statements require 2 or "
        "3 arguments. Got %d", a->count);
    LASSERT_TYPE("if", a, 0, LVAL_BOOL);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    if (a->count == 3) {
        LASSERT_TYPE("if", a, 2, LVAL_QEXPR);
    }

    lval* cond = lval_pop(a, 0);
    lval* if_expr = lval_pop(a, 0);
    lval* else_expr;
    if (a->count == 1) {
        else_expr = lval_pop(a, 0);
    } else {
        else_expr = lval_sexpr();
    }

    // Mark expressions as executable
    if_expr->type=LVAL_SEXPR;
    else_expr->type=LVAL_SEXPR;

    lval_del(a);

    if (cond->bool) {
        return lval_eval(e, if_expr);
    } else {
        return lval_eval(e, else_expr);
    }
}
