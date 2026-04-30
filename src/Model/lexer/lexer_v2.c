/* lexer_1.c
 *
 * Alternative DFA-based implementation of the lexer scanner.
 * The high-level driver (lexer_init / lexer_next_token) is identical to
 * lexer.c.  The difference is that all the individual scan_* functions are
 * replaced by a single scan_next_token_dfa() that runs an explicit state
 * machine matching the pseudocode:
 *
 *   START → IN_WORD | IN_NUMBER | IN_STRING | SAW_OPERATOR | FINAL | ERROR
 *   IN_WORD   → IN_WORD (accumulate) | FINAL (emit identifier/keyword)
 *   IN_NUMBER → IN_NUMBER (accumulate) | IN_FLOAT (dot seen) | FINAL (int)
 *   IN_FLOAT  → IN_FLOAT (accumulate) | FINAL (float)
 *   IN_STRING → IN_STRING (accumulate) | FINAL (string) | ERROR (unterminated)
 *   SAW_OPERATOR → FINAL (single or double operator)
 *   ERROR     → FINAL (error token)
 */

#include "lexer.h"
#include <string.h>
#include <stdlib.h>

static void report_error(Lexer *lexer, const char *message, int line, int column)
{
    error_list_add(lexer->errors, message, line, column);
}

/* ------------------------------------------------------------------ */
/* DFA state enum                                                       */
/* ------------------------------------------------------------------ */

typedef enum
{
    DFA_START,        /* initial state — inspect first character             */
    DFA_IN_WORD,      /* accumulating letters / digits / underscores         */
    DFA_IN_NUMBER,    /* accumulating decimal digits                         */
    DFA_IN_FLOAT,     /* accumulating digits after the decimal point         */
    DFA_IN_STRING,    /* accumulating characters inside a string literal     */
    DFA_SAW_OPERATOR, /* consumed one operator char, peeking for second      */
    DFA_FINAL,        /* accepting state — token value and type are ready    */
    DFA_ERROR         /* error state — call report_error, emit TOKEN_ERROR   */
} DFAState;

/* ------------------------------------------------------------------ */
/* Position helpers  (identical to lexer.c)                            */
/* ------------------------------------------------------------------ */

static char current_char(const Lexer *lexer)
{
    if (lexer->position >= lexer->length)
        return '\0';
    return lexer->source[lexer->position];
}

static char peek_next(const Lexer *lexer)
{
    if (lexer->position + 1 >= lexer->length)
        return '\0';
    return lexer->source[lexer->position + 1];
}

static int at_end(const Lexer *lexer)
{
    return lexer->position >= lexer->length;
}

static void advance(Lexer *lexer)
{
    if (at_end(lexer))
        return;
    if (lexer->source[lexer->position] == '\n')
    {
        lexer->line++;
        lexer->column = 1;
    }
    else
    {
        lexer->column++;
    }
    lexer->position++;
}

/* ------------------------------------------------------------------ */
/* Token construction  (identical to lexer.c)                          */
/* ------------------------------------------------------------------ */

static Token make_token(TokenType type, const char *value, int line, int col)
{
    Token t;
    t.type = type;
    t.line = line;
    t.column = col;
    strncpy(t.value, value, MAX_TOKEN_VALUE - 1);
    t.value[MAX_TOKEN_VALUE - 1] = '\0';
    return t;
}

/* ------------------------------------------------------------------ */
/* Character classification helpers  (identical to lexer.c)            */
/* ------------------------------------------------------------------ */

static int is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_alphanumeric(char c)
{
    return is_letter(c) || is_digit(c);
}

/* ------------------------------------------------------------------ */
/* Identifier validation DFA  (identical to lexer.c)                   */
/* ------------------------------------------------------------------ */

static int is_valid_identifier(const char *s)
{
    if (!s || *s == '\0')
        return 0;
    if (!is_letter(*s) && *s != '_')
        return 0;
    s++;
    while (*s)
    {
        if (!is_alphanumeric(*s) && *s != '_')
            return 0;
        s++;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Whitespace / comment skipping  (identical to lexer.c)               */
/* ------------------------------------------------------------------ */

static void skip_whitespace_and_comments(Lexer *lexer)
{
    while (!at_end(lexer))
    {
        char c = current_char(lexer);
        if (c == ' ')
        {
            advance(lexer);
        }
        else if (c == '#')
        {
            while (!at_end(lexer) && current_char(lexer) != '\n')
                advance(lexer);
        }
        else
        {
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Indentation handling (PDA)  (identical to lexer.c)                  */
/* ------------------------------------------------------------------ */

static int count_indent(Lexer *lexer)
{
    int spaces = 0;
    while (!at_end(lexer))
    {
        char c = current_char(lexer);
        if (c == ' ')
        {
            spaces++;
            advance(lexer);
        }
        else if (c == '\t')
        {
            report_error(lexer, "Tab character not allowed for indentation", lexer->line, lexer->column);
            advance(lexer);
            return -1;
        }
        else
        {
            break;
        }
    }
    return spaces;
}

static Token handle_indentation(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;

    int indent = count_indent(lexer);

    if (current_char(lexer) == '\n' || current_char(lexer) == '#' || at_end(lexer))
    {
        Token skip;
        skip.type = TOKEN_COUNT;
        return skip;
    }

    if (indent < 0)
        return make_token(TOKEN_ERROR, "tab in indentation", start_line, start_col);

    int top = indent_stack_peek(&lexer->indent_stack);

    if (indent == top)
    {
        Token skip;
        skip.type = TOKEN_COUNT;
        return skip;
    }
    else if (indent > top)
    {
        indent_stack_push(&lexer->indent_stack, indent);
        return make_token(TOKEN_INDENT, "", start_line, start_col);
    }
    else
    {
        while (indent_stack_peek(&lexer->indent_stack) > indent)
        {
            indent_stack_pop(&lexer->indent_stack);
            lexer->pending_dedents++;
        }
        if (indent_stack_peek(&lexer->indent_stack) != indent)
        {
            report_error(lexer, "Indentation level does not match any outer block", start_line, start_col);
            return make_token(TOKEN_ERROR, "bad dedent", start_line, start_col);
        }
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", start_line, start_col);
    }
}

/* ------------------------------------------------------------------ */
/* DFA token scanner                                                    */
/*                                                                      */
/* This function replaces all the individual scan_* functions from      */
/* lexer.c.  It drives the state machine described in the pseudocode:   */
/*                                                                      */
/*   Receive lexer → init state = START, empty buffer, save position   */
/*   Loop until FINAL:                                                  */
/*     read current char                                                */
/*     switch(state): transition + optionally accumulate               */
/*   Return token with saved start position                            */
/* ------------------------------------------------------------------ */

static Token scan_next_token_dfa(Lexer *lexer)
{
    /* ── Initialise the DFA ── */
    DFAState state = DFA_START;
    int start_line = lexer->line;
    int start_col = lexer->column;
    char buf[LEXER_BUFFER_SIZE];
    int buf_len = 0;
    char op_char = '\0';   /* first char of a potential two-char operator */
    char str_quote = '\0'; /* opening quote for a string literal          */
    TokenType result = TOKEN_ERROR;

    /* ── Main DFA loop ── */
    while (state != DFA_FINAL)
    {
        char c = current_char(lexer);

        switch (state)
        {
        /* ---------------------------------------------------------- */
        case DFA_START:
            /*
             * Inspect the first character and decide which state to enter.
             *
             * Branches (fill in the body of each):
             *   letter / '_'          → consume, append to buf, → DFA_IN_WORD
             *   digit                 → consume, append to buf, → DFA_IN_NUMBER
             *   '"' or '\''           → save quote, consume,    → DFA_IN_STRING
             *   operator char         → save op_char, consume,  → DFA_SAW_OPERATOR
             *   single delimiter      → consume, set result,    → DFA_FINAL
             *   '\t' or unknown char  → report_error,           → DFA_ERROR
             */
            /* TODO */
            (void)op_char;
            (void)str_quote;
            (void)result;
            state = DFA_FINAL; /* placeholder — remove when implemented */
            break;

        /* ---------------------------------------------------------- */
        case DFA_IN_WORD:
            /*
             * Accumulation state for identifiers and keywords.
             *
             * If c is alphanumeric or '_':
             *   append to buf, advance, stay in DFA_IN_WORD
             * Otherwise (end of word):
             *   null-terminate buf
             *   validate with is_valid_identifier()
             *   look up in keyword table → result type
             *   → DFA_FINAL  (do NOT advance — char belongs to next token)
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* ---------------------------------------------------------- */
        case DFA_IN_NUMBER:
            /*
             * Accumulation state for integer literals.
             *
             * If c is a digit:
             *   append to buf, advance, stay in DFA_IN_NUMBER
             * If c is '.' AND peek_next() is a digit:
             *   append '.' to buf, advance, → DFA_IN_FLOAT
             * Otherwise:
             *   null-terminate buf, result = TOKEN_INT_LITERAL, → DFA_FINAL
             *   (do NOT advance)
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* ---------------------------------------------------------- */
        case DFA_IN_FLOAT:
            /*
             * Extended accumulation state after the decimal point.
             *
             * If c is a digit:
             *   append to buf, advance, stay in DFA_IN_FLOAT
             * Otherwise:
             *   null-terminate buf, result = TOKEN_FLOAT_LITERAL, → DFA_FINAL
             *   (do NOT advance)
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* ---------------------------------------------------------- */
        case DFA_IN_STRING:
            /*
             * Accumulation state for string literals.
             *
             * If at_end() or c == '\n':
             *   report_error("Unterminated string literal", ...)
             *   null-terminate buf, → DFA_ERROR
             * If c == str_quote (closing quote):
             *   advance (consume closing quote)
             *   null-terminate buf, result = TOKEN_STRING_LITERAL, → DFA_FINAL
             * Otherwise:
             *   append c to buf, advance, stay in DFA_IN_STRING
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* ---------------------------------------------------------- */
        case DFA_SAW_OPERATOR:
            /*
             * Lookahead state: op_char holds the first operator character.
             * Check whether c forms a valid two-character operator with op_char.
             *
             * For each op_char value, handle:
             *   '+' : '=' → TOKEN_PLUS_ASSIGN,  else TOKEN_PLUS
             *   '-' : '=' → TOKEN_MINUS_ASSIGN, else TOKEN_MINUS
             *   '*' : '*' → TOKEN_DOUBLE_STAR,  else TOKEN_STAR
             *   '/' : '/' → TOKEN_DOUBLE_SLASH, else TOKEN_SLASH
             *   '%' :        TOKEN_PERCENT  (no two-char form)
             *   '=' : '=' → TOKEN_EQ,           else TOKEN_ASSIGN
             *   '!' : '=' → TOKEN_NEQ,          else report_error + DFA_ERROR
             *   '<' : '=' → TOKEN_LTE,          else TOKEN_LT
             *   '>' : '=' → TOKEN_GTE,          else TOKEN_GT
             *
             * If c completes a two-char operator:
             *   append c to buf, advance, set result → DFA_FINAL
             * Otherwise:
             *   null-terminate buf, set result → DFA_FINAL
             *   (do NOT advance — c belongs to next token)
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* ---------------------------------------------------------- */
        case DFA_ERROR:
            /*
             * Error state — token has already been reported via report_error().
             * Set result = TOKEN_ERROR and transition to DFA_FINAL so the
             * caller receives a TOKEN_ERROR and scanning can continue.
             */
            /* TODO */
            state = DFA_FINAL; /* placeholder */
            break;

        /* DFA_FINAL is the loop exit condition — never reached inside loop */
        default:
            state = DFA_FINAL;
            break;
        }
    }

    buf[buf_len < LEXER_BUFFER_SIZE ? buf_len : LEXER_BUFFER_SIZE - 1] = '\0';
    return make_token(result, buf, start_line, start_col);
}

/* ------------------------------------------------------------------ */
/* Public API  (identical to lexer.c except the final dispatch calls   */
/* scan_next_token_dfa instead of the individual scan_* functions)     */
/* ------------------------------------------------------------------ */

void lexer_init(Lexer *lexer, const char *source, ErrorList *errors)
{
    lexer->source = source;
    lexer->position = 0;
    lexer->length = (int)strlen(source);
    lexer->line = 1;
    lexer->column = 1;
    lexer->pending_dedents = 0;
    lexer->at_line_start = 1;
    lexer->buffer_length = 0;
    lexer->buffer[0] = '\0';
    lexer->errors = errors;

    indent_stack_init(&lexer->indent_stack);
    indent_stack_push(&lexer->indent_stack, 0);

    keyword_table_init(&lexer->keyword_table);
}

Token lexer_next_token(Lexer *lexer)
{
    /* 1. Deliver any queued DEDENTs */
    if (lexer->pending_dedents > 0)
    {
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
    }

    /* 2. At EOF: flush remaining indent levels, then TOKEN_EOF */
    if (at_end(lexer))
    {
        if (indent_stack_peek(&lexer->indent_stack) > 0)
        {
            indent_stack_pop(&lexer->indent_stack);
            return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
        }
        return make_token(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    /* 3. Newline → emit NEWLINE, set at_line_start for next call */
    if (current_char(lexer) == '\n')
    {
        int tok_line = lexer->line;
        int tok_col = lexer->column;
        advance(lexer);
        lexer->at_line_start = 1;
        return make_token(TOKEN_NEWLINE, "\\n", tok_line, tok_col);
    }

    /* 4. Tab character (outside indentation phase) */
    if (current_char(lexer) == '\t')
    {
        int tok_line = lexer->line;
        int tok_col = lexer->column;
        report_error(lexer, "Tab character not allowed", tok_line, tok_col);
        advance(lexer);
        return make_token(TOKEN_ERROR, "\t", tok_line, tok_col);
    }

    /* 5. Handle indentation at the start of a logical line */
    if (lexer->at_line_start)
    {
        lexer->at_line_start = 0;
        Token ind = handle_indentation(lexer);
        if (ind.type != TOKEN_COUNT)
            return ind;

        if (current_char(lexer) == '#' || current_char(lexer) == '\n' || at_end(lexer))
        {
            skip_whitespace_and_comments(lexer); /* consume '#...' up to '\n' */
            lexer->at_line_start = 1;
            return lexer_next_token(lexer);
        }
    }

    /* 6. Skip inline whitespace and comments */
    skip_whitespace_and_comments(lexer);

    /* 7. Re-check after skipping */
    if (at_end(lexer) || current_char(lexer) == '\n')
        return lexer_next_token(lexer);

    /* 8. Delegate to the DFA scanner for the actual token */
    return scan_next_token_dfa(lexer);
}
