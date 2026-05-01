/* parser.c */

#include "parser.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Statement dispatch table — flat array, indexed directly by TokenType */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Position helpers                                                     */
/* ------------------------------------------------------------------ */

static const Token *current_token(const Parser *parser)
{
    return &parser->tokens[parser->position];
}

static int at_end(const Parser *parser)
{
    return current_token(parser)->type == TOKEN_EOF;
}

static TokenType current_type(const Parser *parser)
{
    return current_token(parser)->type;
}

static int check(const Parser *parser, TokenType type)
{
    return current_type(parser) == type;
}

static const Token *advance(Parser *parser)
{
    const Token *t = current_token(parser);
    if (!at_end(parser))
        parser->position++;
    return t;
}

static int match(Parser *parser, TokenType type)
{
    if (check(parser, type))
    {
        advance(parser);
        return 1;
    }
    return 0;
}

static const Token *expect(Parser *parser, TokenType type, const char *message)
{
    if (check(parser, type))
        return advance(parser);
    const Token *t = current_token(parser);
    error_list_add(parser->errors, message, t->line, t->column);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_block(Parser *parser);

/* ------------------------------------------------------------------ */
/* Expression parser — shunting-yard algorithm                          */
/* ------------------------------------------------------------------ */

#define EXPR_STACK_MAX 64

/* One slot on the operator stack */
typedef struct
{
    TokenType type;
    char symbol[8]; /* string stored in the resulting AST node    */
    int is_unary;
    int line, col;
} OpEntry;

/* All operator metadata in one place — indexed directly by TokenType. */
typedef struct
{
    const char *symbol;
    int precedence;
    int is_binary;
    int is_right_assoc;
} OpInfo;

static const OpInfo op_info[TOKEN_COUNT] = {
    [TOKEN_OR] = {"or", 1, 1, 0},
    [TOKEN_AND] = {"and", 2, 1, 0},
    [TOKEN_NOT] = {"not", 3, 0, 0}, /* unary — not binary */
    [TOKEN_EQ] = {"==", 4, 1, 0},
    [TOKEN_NEQ] = {"!=", 4, 1, 0},
    [TOKEN_LT] = {"<", 4, 1, 0},
    [TOKEN_GT] = {">", 4, 1, 0},
    [TOKEN_LTE] = {"<=", 4, 1, 0},
    [TOKEN_GTE] = {">=", 4, 1, 0},
    [TOKEN_IN] = {"in", 4, 1, 0},
    [TOKEN_PLUS] = {"+", 5, 1, 0},
    [TOKEN_MINUS] = {"-", 5, 1, 0},
    [TOKEN_STAR] = {"*", 6, 1, 0},
    [TOKEN_SLASH] = {"/", 6, 1, 0},
    [TOKEN_DOUBLE_SLASH] = {"//", 6, 1, 0},
    [TOKEN_PERCENT] = {"%", 6, 1, 0},
    [TOKEN_DOUBLE_STAR] = {"**", 7, 1, 1}, /* right-associative */
};

/* Pop the top operator and fold one or two operands into a new node. */
static int apply_operator(OpEntry *ops, int *op_top,
                          ASTNode **out, int *out_top,
                          ErrorList *errors)
{
    if (*op_top < 0)
        return -1;

    OpEntry op = ops[(*op_top)--];
    ASTNode *node;

    if (op.is_unary)
    {
        if (*out_top < 0)
        {
            error_list_add(errors, "Missing operand for unary operator",
                           op.line, op.col);
            return -1;
        }
        ASTNode *operand = out[(*out_top)--];
        node = ast_node_create(NODE_UNARY_OP, op.symbol, op.line, op.col);
        if (!node || ast_node_add_child(node, operand) != 0)
        {
            ast_node_free(node);
            return -1;
        }
    }
    else
    {
        if (*out_top < 1)
        {
            error_list_add(errors, "Missing operand for binary operator",
                           op.line, op.col);
            return -1;
        }
        ASTNode *right = out[(*out_top)--];
        ASTNode *left = out[(*out_top)--];
        node = ast_node_create(NODE_BINARY_OP, op.symbol, op.line, op.col);
        if (!node)
            return -1;
        ast_node_add_child(node, left);
        ast_node_add_child(node, right);
    }

    out[++(*out_top)] = node;
    return 0;
}

/* Initialised once at startup; all slots set to NODE_COUNT (sentinel). */
static NodeType operand_node_table[TOKEN_COUNT];
static int operand_table_ready = 0;

static void init_operand_table(void)
{
    for (int i = 0; i < TOKEN_COUNT; i++)
        operand_node_table[i] = NODE_COUNT; /* sentinel = not an operand */

    operand_node_table[TOKEN_INT_LITERAL] = NODE_INT_LITERAL;
    operand_node_table[TOKEN_FLOAT_LITERAL] = NODE_FLOAT_LITERAL;
    operand_node_table[TOKEN_STRING_LITERAL] = NODE_STRING_LITERAL;
    operand_node_table[TOKEN_IDENTIFIER] = NODE_IDENTIFIER;
    operand_node_table[TOKEN_TRUE] = NODE_BOOL_LITERAL;
    operand_node_table[TOKEN_FALSE] = NODE_BOOL_LITERAL;
    operand_node_table[TOKEN_NONE] = NODE_NONE_LITERAL;
}

/* Parses one expression using the shunting-yard algorithm.
   Stops at newline, colon, comma, or closing bracket. */
static ASTNode *parse_expression(Parser *parser)
{
    if (!operand_table_ready)
    {
        init_operand_table();
        operand_table_ready = 1;
    }

    ASTNode *out_stack[EXPR_STACK_MAX];
    OpEntry op_stack[EXPR_STACK_MAX];
    int out_top = -1;
    int op_top = -1;
    int expect_operand = 1; /* 1 = waiting for operand, 0 = waiting for operator */

    while (!at_end(parser))
    {
        const Token *t = current_token(parser);
        TokenType type = t->type;

        /* Expression terminators */
        if (type == TOKEN_NEWLINE || type == TOKEN_COLON ||
            type == TOKEN_COMMA || type == TOKEN_RPAREN ||
            type == TOKEN_RBRACKET || type == TOKEN_RBRACE ||
            type == TOKEN_EOF)
            break;

        if (expect_operand)
        {
            NodeType node_type = operand_node_table[type];

            if (node_type != NODE_COUNT)
            {
                /* Simple operand: one table lookup replaces all if-else chains */
                out_stack[++out_top] = ast_node_create(node_type, t->value,
                                                       t->line, t->column);
                advance(parser);
                expect_operand = 0;
            }
            else if (type == TOKEN_LPAREN)
            {
                /* Parenthesised sub-expression — recurse */
                advance(parser);
                ASTNode *inner = parse_expression(parser);
                expect(parser, TOKEN_RPAREN, "Expected ')'");
                if (out_top < EXPR_STACK_MAX - 1)
                    out_stack[++out_top] = inner;
                expect_operand = 0;
            }
            else if (type == TOKEN_NOT || type == TOKEN_MINUS)
            {
                /* Unary operator — push on operator stack, stay in operand mode */
                OpEntry op;
                op.type = type;
                op.is_unary = 1;
                op.line = t->line;
                op.col = t->column;
                strncpy(op.symbol, op_info[type].symbol, sizeof(op.symbol) - 1);
                op.symbol[sizeof(op.symbol) - 1] = '\0';
                advance(parser);
                if (op_top < EXPR_STACK_MAX - 1)
                    op_stack[++op_top] = op;
                /* remain in expect_operand = 1 */
            }
            else if (type == TOKEN_LBRACKET || type == TOKEN_LBRACE)
            {
                /* Lists, tuples, and dictionaries are out of scope */
                error_list_add(parser->errors,
                    "List/tuple/dictionary literals are not supported "
                    "in this version of the compiler",
                    t->line, t->column);
                /* skip to the next newline to recover */
                while (!at_end(parser) && current_type(parser) != TOKEN_NEWLINE)
                    advance(parser);
                break;
            }
            else
            {
                break; /* unexpected token — stop */
            }
        }
        else
        {
            /* ── Operator branch (lookahead to decide precedence) ── */
            if (!op_info[type].is_binary)
                break;

            int prec = op_info[type].precedence;

            /* Pop operators with higher precedence (or equal for left-assoc) */
            while (op_top >= 0 &&
                   !op_stack[op_top].is_unary &&
                   (op_info[op_stack[op_top].type].precedence > prec ||
                    (op_info[op_stack[op_top].type].precedence == prec &&
                     !op_info[type].is_right_assoc)))
            {
                if (apply_operator(op_stack, &op_top, out_stack, &out_top,
                                   parser->errors) != 0)
                    goto expr_error;
            }
            /* Pop pending unary operators of higher precedence */
            while (op_top >= 0 &&
                   op_stack[op_top].is_unary &&
                   op_info[op_stack[op_top].type].precedence > prec)
            {
                if (apply_operator(op_stack, &op_top, out_stack, &out_top,
                                   parser->errors) != 0)
                    goto expr_error;
            }

            /* Push new binary operator */
            OpEntry op;
            op.type = type;
            op.is_unary = 0;
            op.line = t->line;
            op.col = t->column;
            strncpy(op.symbol, op_info[type].symbol, sizeof(op.symbol) - 1);
            op.symbol[sizeof(op.symbol) - 1] = '\0';
            advance(parser);
            if (op_top < EXPR_STACK_MAX - 1)
                op_stack[++op_top] = op;

            expect_operand = 1;
        }
    }

    /* Flush remaining operators */
    while (op_top >= 0)
    {
        if (apply_operator(op_stack, &op_top, out_stack, &out_top,
                           parser->errors) != 0)
            goto expr_error;
    }

    if (out_top >= 0)
        return out_stack[out_top];

expr_error:
{
    const Token *t = current_token(parser);
    error_list_add(parser->errors, "Expected expression", t->line, t->column);
    return ast_node_create(NODE_INT_LITERAL, "0", t->line, t->column);
}
}

/* ------------------------------------------------------------------ */
/* Block parser                                                         */
/* ------------------------------------------------------------------ */

/* Parses an indented block: INDENT { statement } DEDENT → NODE_BLOCK  */
static ASTNode *parse_block(Parser *parser)
{
    const Token *t = current_token(parser);
    ASTNode *block = ast_node_create(NODE_BLOCK, "", t->line, t->column);
    if (!block)
        return NULL;

    if (!expect(parser, TOKEN_INDENT, "Expected indented block"))
    {
        ast_node_free(block);
        return NULL;
    }

    while (!at_end(parser) && !check(parser, TOKEN_DEDENT))
    {
        /* Skip blank lines inside block */
        while (check(parser, TOKEN_NEWLINE))
            advance(parser);

        if (check(parser, TOKEN_DEDENT) || at_end(parser))
            break;

        ASTNode *stmt = parse_statement(parser);
        if (!stmt)
            break;

        if (ast_node_add_child(block, stmt) != 0)
        {
            error_list_add(parser->errors, "Memory allocation failed", 0, 0);
            ast_node_free(stmt);
            break;
        }
    }

    match(parser, TOKEN_DEDENT); /* consume closing DEDENT */
    return block;
}

/* ------------------------------------------------------------------ */
/* Statement parsers                                                    */
/* ------------------------------------------------------------------ */

/* IF [ELIF]* [ELSE] */
static ASTNode *parse_if(Parser *parser)
{
    const Token *t = advance(parser); /* consume IF */
    ASTNode *node = ast_node_create(NODE_IF, "", t->line, t->column);

    /* Condition */
    ASTNode *cond = parse_expression(parser);
    if (cond)
        ast_node_add_child(node, cond);

    expect(parser, TOKEN_COLON, "Expected ':' after if condition");
    match(parser, TOKEN_NEWLINE);

    /* Body */
    ASTNode *body = parse_block(parser);
    if (body)
        ast_node_add_child(node, body);

    /* elif chains */
    while (check(parser, TOKEN_ELIF))
    {
        const Token *et = advance(parser); /* consume ELIF */
        ASTNode *elif = ast_node_create(NODE_ELIF, "", et->line, et->column);

        ASTNode *ec = parse_expression(parser);
        if (ec)
            ast_node_add_child(elif, ec);

        expect(parser, TOKEN_COLON, "Expected ':' after elif condition");
        match(parser, TOKEN_NEWLINE);

        ASTNode *eb = parse_block(parser);
        if (eb)
            ast_node_add_child(elif, eb);

        ast_node_add_child(node, elif);
    }

    /* else */
    if (check(parser, TOKEN_ELSE))
    {
        const Token *et = advance(parser); /* consume ELSE */
        ASTNode *els = ast_node_create(NODE_ELSE, "", et->line, et->column);

        expect(parser, TOKEN_COLON, "Expected ':' after else");
        match(parser, TOKEN_NEWLINE);

        ASTNode *eb = parse_block(parser);
        if (eb)
            ast_node_add_child(els, eb);

        ast_node_add_child(node, els);
    }

    return node;
}

/* WHILE expr : block */
static ASTNode *parse_while(Parser *parser)
{
    const Token *t = advance(parser); /* consume WHILE */
    ASTNode *node = ast_node_create(NODE_WHILE, "", t->line, t->column);

    ASTNode *cond = parse_expression(parser);
    if (cond)
        ast_node_add_child(node, cond);

    expect(parser, TOKEN_COLON, "Expected ':' after while condition");
    match(parser, TOKEN_NEWLINE);

    ASTNode *body = parse_block(parser);
    if (body)
        ast_node_add_child(node, body);

    return node;
}

/* Parses range(EXPR) as the only legal for-loop iterable.
   Returns a NODE_RANGE node with the upper-bound expression as child[0]. */
static ASTNode *parse_for_iterable(Parser *parser)
{
    const Token *t = current_token(parser);

    /* Must start with the identifier "range" */
    if (t->type != TOKEN_IDENTIFIER || strcmp(t->value, "range") != 0)
    {
        error_list_add(parser->errors,
                       "Expected 'range(...)' as for-loop iterable",
                       t->line, t->column);
        /* recover: skip to colon */
        while (!at_end(parser) &&
               current_type(parser) != TOKEN_COLON &&
               current_type(parser) != TOKEN_NEWLINE)
            advance(parser);
        return ast_node_create(NODE_RANGE, "", t->line, t->column);
    }

    advance(parser); /* consume 'range' */
    expect(parser, TOKEN_LPAREN, "Expected '(' after 'range'");

    ASTNode *upper = parse_expression(parser);

    expect(parser, TOKEN_RPAREN, "Expected ')' after range argument");

    ASTNode *range_node = ast_node_create(NODE_RANGE, "", t->line, t->column);
    if (upper)
        ast_node_add_child(range_node, upper);

    return range_node;
}

/* FOR identifier IN range(EXPR) : block */
static ASTNode *parse_for(Parser *parser)
{
    const Token *t    = advance(parser); /* consume FOR */
    ASTNode     *node = ast_node_create(NODE_FOR, "", t->line, t->column);

    /* child[0]: loop variable */
    const Token *var = expect(parser, TOKEN_IDENTIFIER,
                              "Expected variable name after 'for'");
    if (var)
        ast_node_add_child(node,
            ast_node_create(NODE_IDENTIFIER, var->value, var->line, var->column));

    expect(parser, TOKEN_IN, "Expected 'in' after loop variable");

    /* child[1]: range node — only range(...) is allowed */
    ASTNode *iter = parse_for_iterable(parser);
    if (iter)
        ast_node_add_child(node, iter);

    expect(parser, TOKEN_COLON,  "Expected ':' after for iterable");
    match(parser,  TOKEN_NEWLINE);

    /* child[2]: body block */
    ASTNode *body = parse_block(parser);
    if (body)
        ast_node_add_child(node, body);

    return node;
}

/* DEF name ( params ) : block */
static ASTNode *parse_def(Parser *parser)
{
    const Token *t = advance(parser); /* consume DEF */
    const Token *name = expect(parser, TOKEN_IDENTIFIER,
                               "Expected function name after 'def'");

    ASTNode *node = ast_node_create(NODE_DEF,
                                    name ? name->value : "<error>",
                                    t->line, t->column);

    expect(parser, TOKEN_LPAREN, "Expected '(' after function name");

    ASTNode *params = ast_node_create(NODE_PARAM_LIST, "", t->line, t->column);

    if (!check(parser, TOKEN_RPAREN))
    {
        const Token *p = expect(parser, TOKEN_IDENTIFIER, "Expected parameter name");
        if (p)
            ast_node_add_child(params,
                               ast_node_create(NODE_IDENTIFIER, p->value, p->line, p->column));

        while (match(parser, TOKEN_COMMA))
        {
            p = expect(parser, TOKEN_IDENTIFIER, "Expected parameter name");
            if (p)
                ast_node_add_child(params,
                                   ast_node_create(NODE_IDENTIFIER, p->value, p->line, p->column));
        }
    }

    expect(parser, TOKEN_RPAREN, "Expected ')' after parameters");
    ast_node_add_child(node, params);

    expect(parser, TOKEN_COLON, "Expected ':' after def signature");
    match(parser, TOKEN_NEWLINE);

    ASTNode *body = parse_block(parser);
    if (body)
        ast_node_add_child(node, body);

    return node;
}

/* RETURN [expr] */
static ASTNode *parse_return(Parser *parser)
{
    const Token *t = advance(parser); /* consume RETURN */
    ASTNode *node = ast_node_create(NODE_RETURN, "", t->line, t->column);

    if (!check(parser, TOKEN_NEWLINE) && !at_end(parser))
    {
        ASTNode *val = parse_expression(parser);
        if (val)
            ast_node_add_child(node, val);
    }

    match(parser, TOKEN_NEWLINE);
    return node;
}

/* PRINT ( args ) */
static ASTNode *parse_print(Parser *parser)
{
    const Token *t = advance(parser); /* consume PRINT */
    ASTNode *node = ast_node_create(NODE_PRINT, "", t->line, t->column);

    expect(parser, TOKEN_LPAREN, "Expected '(' after print");

    if (!check(parser, TOKEN_RPAREN))
    {
        ASTNode *arg = parse_expression(parser);
        if (arg)
            ast_node_add_child(node, arg);

        while (match(parser, TOKEN_COMMA))
        {
            arg = parse_expression(parser);
            if (arg)
                ast_node_add_child(node, arg);
        }
    }

    expect(parser, TOKEN_RPAREN, "Expected ')' after print arguments");
    match(parser, TOKEN_NEWLINE);
    return node;
}

/* BREAK */
static ASTNode *parse_break(Parser *parser)
{
    const Token *t = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_node_create(NODE_BREAK, "", t->line, t->column);
}

/* CONTINUE */
static ASTNode *parse_continue(Parser *parser)
{
    const Token *t = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_node_create(NODE_CONTINUE, "", t->line, t->column);
}

/* PASS */
static ASTNode *parse_pass(Parser *parser)
{
    const Token *t = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_node_create(NODE_PASS, "", t->line, t->column);
}

/* identifier (= | += | -=) expr */
static ASTNode *parse_assign(Parser *parser)
{
    const Token *id = advance(parser); /* consume identifier */
    NodeType ntype;

    if (check(parser, TOKEN_ASSIGN))
    {
        advance(parser);
        ntype = NODE_ASSIGN;
    }
    else if (check(parser, TOKEN_PLUS_ASSIGN))
    {
        advance(parser);
        ntype = NODE_ASSIGN_PLUS;
    }
    else if (check(parser, TOKEN_MINUS_ASSIGN))
    {
        advance(parser);
        ntype = NODE_ASSIGN_MINUS;
    }
    else
    {
        /* Not an assignment — bare identifier or call (unsupported here) */
        error_list_add(parser->errors, "Expected assignment operator",
                       id->line, id->column);
        match(parser, TOKEN_NEWLINE);
        return ast_node_create(NODE_IDENTIFIER, id->value, id->line, id->column);
    }

    ASTNode *node = ast_node_create(ntype, "", id->line, id->column);
    ASTNode *target = ast_node_create(NODE_IDENTIFIER, id->value,
                                      id->line, id->column);
    ast_node_add_child(node, target);

    ASTNode *value = parse_expression(parser);
    if (value)
        ast_node_add_child(node, value);

    match(parser, TOKEN_NEWLINE);
    return node;
}

/* ------------------------------------------------------------------ */
/* Dispatch table initialisation                                        */
/* ------------------------------------------------------------------ */

static void dispatch_table_init(ParseFn *table)
{
    for (int i = 0; i < TOKEN_COUNT; i++)
        table[i] = NULL;

    table[TOKEN_IF] = parse_if;
    table[TOKEN_WHILE] = parse_while;
    table[TOKEN_FOR] = parse_for;
    table[TOKEN_DEF] = parse_def;
    table[TOKEN_RETURN] = parse_return;
    table[TOKEN_PRINT] = parse_print;
    table[TOKEN_BREAK] = parse_break;
    table[TOKEN_CONTINUE] = parse_continue;
    table[TOKEN_PASS] = parse_pass;
    table[TOKEN_IDENTIFIER] = parse_assign;
}

/* ------------------------------------------------------------------ */
/* Statement dispatcher — direct array lookup                          */
/* ------------------------------------------------------------------ */

static ASTNode *parse_statement(Parser *parser)
{
    while (check(parser, TOKEN_NEWLINE))
        advance(parser);

    if (at_end(parser))
        return NULL;

    ParseFn fn = parser->stmt_dispatch[current_type(parser)];
    if (fn)
        return fn(parser);

    /* No registered handler — wrap in error node and recover */
    const Token *t = current_token(parser);
    error_list_add(parser->errors, "Invalid syntax", t->line, t->column);
    advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_node_create(NODE_IDENTIFIER, "<error>", t->line, t->column);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void parser_init(Parser *parser,
                 const Token *tokens, int count,
                 ErrorList *errors)
{
    parser->tokens = tokens;
    parser->position = 0;
    parser->count = count;
    parser->errors = errors;

    dispatch_table_init(parser->stmt_dispatch);
}

/* Parses the full token stream into a NODE_PROGRAM tree. */
ASTNode *parse_program(Parser *parser)
{
    ASTNode *program = ast_node_create(NODE_PROGRAM, "", 1, 1);
    if (!program)
        return NULL;

    while (!at_end(parser))
    {
        ASTNode *stmt = parse_statement(parser);
        if (!stmt)
            break;
        if (ast_node_add_child(program, stmt) != 0)
        {
            error_list_add(parser->errors, "Memory allocation failed in parser", 0, 0);
            ast_node_free(stmt);
            break;
        }
    }

    return program;
}
