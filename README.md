
## Statement evaluation

**Statement**:

```
+ (- 1)
```

**AST**:

printed:
```
>
  regex
  expr|symbol|char:1:1 '+'
  expr|sexpr|>
    char:1:3 '('
    expr|symbol|char:1:4 '-'
    expr|number|regex:1:6 '1'
    char:1:7 ')'
  regex


```

expressed as yaml:
```yaml
- 
    tag: >
    contents: ""
    num_children: 4
    children:
        -
            tag: regex
            contents: ""
            num_children: 0
        -
            tag: expr|symbol|char
            contents: "+"
            num_children: 0
        -   
            tag: expr|symbol|>
            contents: ""
            num_children: 4
            children:
                - 
                    tag: char
                    contents: "("
                    num_children: 0
                -
                    tag: expr|symbol|char
                    contents: "-"
                    num_children: 0
                -
                    tag: expr|number|regex
                    contents: "1"
                    num_children: 0
                -
                    tag: char
                    contents: ")"
                    num_children: 0
        -
            tag: regex
            contents: ""
            num_children: 0
```

**AST parsing**

`lval_read` is used to parse the generic AST received from `mpc` into the AST
of `lval` nodes which we'll evaluate.

The output of `lval_println(lval_read)` is:

```
(+ (- 1))
```

TODO: write a YAML representation of the lval ast

```yaml
type: LVAL_SEXPR
count: 2
cell:
    -
        type: LVAL_SYM
        sym: "+"
        count: 0
    -
        type: LVAL_SEXPR
        count: 2
        cell:
            -
                type: LVAL_SYM
                sym: -
                count: 0
            -
                type: LVAL_NUM
                num: 1
                count: 0
```

**Evaluation**

This LVAL AST, `v` is then passed to `lval_eval()`, which evaluates the
expression. For clarity, let's name the five LVALs a-e.

```c
lval_eval(a)
// a is an s-expression
lval_eval_sexpr(a)
// evaluate a's cells, replace original lval with evaluated lval
    // cell 0
    lval_eval(b)
    // b is not an s-expression, return b
a->cell[0] = b  // unchanged
    // cell 1
    lval_eval(c)
    // c is an s-expression
    lval_eval_sexpr(c)
    // evaluate a's cells, replace original lval with evaluated lval
        // cell 0
        lval_eval(d)
        // d is not an s-expression, return d
    c->cell[0] = d // unchanged 
        // cell 1
        lval_eval(e)
        // e is not an s-expression, return d
    c->cell[1] = e  // unchanged
    // check each cell in c for errors - there are none
    // check if c is an empty or single expression - it is not
    // pop the first element, d, into a variable, f
    // ensure the first element, d, is a symbol - it is
    // evaluate the expression
    lval* result = builtin_op(v, "-")
        // ensure all arguments are numbers - they are
        // pop first element into a variable, x
        // if operation is subtract (it is) and there's one argument (there is)
        // negate it
        x->num = -x->num
        // while loop over remaining elements (there are none), and combine
        // x using the operator op
        return x
    // delete f
    return result  // An LVAL_NUM node with value -1
// check each cell in a for error - there are none
// check if a is an empty or single expression - it is not
// Pop the first element, b, into a variable, f
// Ensure the first element, b, is a symbol - it is
// Evaluate expression
lval* result = builtin_op(v, "+")
    // Ensure all arguments are numbers - they are
    // Pop first argument into a variable, x
    // If operation is subtract (it isn't) and there's one argument...
    // While loop over remaining elements (there are none), and combine
    // x using the operator op
    return x
// delete f
return result  // An LVAL_NUM node with value -1
```
