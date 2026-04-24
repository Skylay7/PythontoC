/* lexer.c */

#include "lexer.h"
#include <string.h>
#include <stdlib.h>

/* TODO: replace with real error handler module once implemented */
static void report_error(const char *message, int line, int column)
{
    /* Placeholder — will call errors_add() from errors.c later */
    (void)message;
    (void)line;
    (void)column;
}

/* ------------------------------------------------------------------ */
/* Position helpers                                                     */
/* ------------------------------------------------------------------ */

/* Returns the character at the current position, or '\0' at end. */
static char current_char(const Lexer *lexer)
{
    if (lexer->position >= lexer->length)
        return '\0';
    return lexer->source[lexer->position];
}

/* Returns the character one ahead of current, or '\0' at end. */
static char peek_next(const Lexer *lexer)
{
    if (lexer->position + 1 >= lexer->length)
        return '\0';
    return lexer->source[lexer->position + 1];
}

/* Returns 1 if the lexer has consumed all input. */
static int at_end(const Lexer *lexer)
{
    return lexer->position >= lexer->length;
}

/* Advances position by one, updating line/column tracking. */
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
/* Token construction                                                   */
/* ------------------------------------------------------------------ */

/* Builds a Token with the given type, value string, and position. */
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
/* Character classification helpers                                     */
/* ------------------------------------------------------------------ */

/* Returns 1 if c is an ASCII letter. */
static int is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Returns 1 if c is an ASCII decimal digit. */
static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* Returns 1 if c is a letter or digit. */
static int is_alphanumeric(char c)
{
    return is_letter(c) || is_digit(c);
}

/* ------------------------------------------------------------------ */
/* Identifier validation (DFA)                                          */
/* ------------------------------------------------------------------ */

/* Returns 1 if the string is a valid Python identifier:
   first char must be letter or '_', rest letters/digits/'_'. */
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
/* Whitespace / comment skipping                                        */
/* ------------------------------------------------------------------ */

/* Skips spaces (not newlines) and '#' line comments. */
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
            /* Skip to end of line (but don't consume the newline itself) */
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
/* Indentation handling (PDA logic)                                     */
/* ------------------------------------------------------------------ */

/* Counts leading spaces on the current line.
   Returns -1 and reports an error if a tab is encountered. */
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
            report_error("Tab character not allowed for indentation", lexer->line, lexer->column);
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

/* Handles the indentation PDA at the start of a logical line.
   Sets pending_dedents or returns an INDENT/DEDENT/nothing token.
   Returns a token if one should be emitted immediately, otherwise a
   TOKEN_EOF sentinel with type TOKEN_COUNT to signal "no token yet". */
static Token handle_indentation(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;

    int indent = count_indent(lexer);

    /* Blank line or comment-only line — skip it entirely */
    if (current_char(lexer) == '\n' || current_char(lexer) == '#' || at_end(lexer))
    {
        /* not a logical line, signal caller to skip */
        Token skip;
        skip.type = TOKEN_COUNT; /* sentinel: "nothing to emit" */
        return skip;
    }

    /* Tab error during indent count */
    if (indent < 0)
    {
        return make_token(TOKEN_ERROR, "tab in indentation", start_line, start_col);
    }

    int top = indent_stack_peek(&lexer->indent_stack);

    if (indent == top)
    {
        /* Same level — no structural token */
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
        /* Dedent: pop until we match or error */
        while (indent_stack_peek(&lexer->indent_stack) > indent)
        {
            indent_stack_pop(&lexer->indent_stack);
            lexer->pending_dedents++;
        }
        if (indent_stack_peek(&lexer->indent_stack) != indent)
        {
            report_error("Indentation level does not match any outer block", start_line, start_col);
            return make_token(TOKEN_ERROR, "bad dedent", start_line, start_col);
        }
        /* Return first DEDENT now; the rest are delivered via pending_dedents */
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", start_line, start_col);
    }
}

/* ------------------------------------------------------------------ */
/* Scanners                                                             */
/* ------------------------------------------------------------------ */

/* Scans a word (identifier or keyword), validates it, looks it up. */
static Token scan_word(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;
    int i = 0;
    char buf[LEXER_BUFFER_SIZE];

    while (!at_end(lexer) && (is_alphanumeric(current_char(lexer)) || current_char(lexer) == '_'))
    {
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }
    buf[i] = '\0';

    if (!is_valid_identifier(buf))
    {
        report_error("Invalid identifier", start_line, start_col);
        return make_token(TOKEN_ERROR, buf, start_line, start_col);
    }

    TokenType kw = keyword_table_lookup(&lexer->keyword_table, buf);
    return make_token(kw, buf, start_line, start_col);
}

/* Scans an integer or float literal. */
static Token scan_number(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;
    int i = 0;
    char buf[LEXER_BUFFER_SIZE];
    int is_float = 0;

    while (!at_end(lexer) && is_digit(current_char(lexer)))
    {
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }

    if (!at_end(lexer) && current_char(lexer) == '.' && is_digit(peek_next(lexer)))
    {
        is_float = 1;
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = '.';
        advance(lexer); /* consume '.' */

        while (!at_end(lexer) && is_digit(current_char(lexer)))
        {
            if (i < LEXER_BUFFER_SIZE - 1)
                buf[i++] = current_char(lexer);
            advance(lexer);
        }
    }

    buf[i] = '\0';
    return make_token(is_float ? TOKEN_FLOAT_LITERAL : TOKEN_INT_LITERAL, buf, start_line, start_col);
}

/* Scans a single- or double-quoted string literal. */
static Token scan_string(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;
    char quote = current_char(lexer);
    advance(lexer); /* consume opening quote */

    int i = 0;
    char buf[LEXER_BUFFER_SIZE];

    while (!at_end(lexer) && current_char(lexer) != quote)
    {
        if (current_char(lexer) == '\n')
        {
            report_error("Unterminated string literal", start_line, start_col);
            buf[i] = '\0';
            return make_token(TOKEN_ERROR, buf, start_line, start_col);
        }
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }

    if (at_end(lexer))
    {
        report_error("Unterminated string literal", start_line, start_col);
        buf[i] = '\0';
        return make_token(TOKEN_ERROR, buf, start_line, start_col);
    }

    advance(lexer); /* consume closing quote */
    buf[i] = '\0';
    return make_token(TOKEN_STRING_LITERAL, buf, start_line, start_col);
}

/* Scans operators, using one character of lookahead for two-char forms. */
static Token scan_operator(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;
    char c = current_char(lexer);
    advance(lexer);
    char next = current_char(lexer);

    switch (c)
    {
    case '+':
        if (next == '=') { advance(lexer); return make_token(TOKEN_PLUS_ASSIGN,  "+=", start_line, start_col); }
        return make_token(TOKEN_PLUS,  "+", start_line, start_col);
    case '-':
        if (next == '=') { advance(lexer); return make_token(TOKEN_MINUS_ASSIGN, "-=", start_line, start_col); }
        return make_token(TOKEN_MINUS, "-", start_line, start_col);
    case '*':
        if (next == '*') { advance(lexer); return make_token(TOKEN_DOUBLE_STAR,  "**", start_line, start_col); }
        return make_token(TOKEN_STAR,  "*", start_line, start_col);
    case '/':
        if (next == '/') { advance(lexer); return make_token(TOKEN_DOUBLE_SLASH, "//", start_line, start_col); }
        return make_token(TOKEN_SLASH, "/", start_line, start_col);
    case '%':
        return make_token(TOKEN_PERCENT, "%", start_line, start_col);
    case '=':
        if (next == '=') { advance(lexer); return make_token(TOKEN_EQ,  "==", start_line, start_col); }
        return make_token(TOKEN_ASSIGN, "=", start_line, start_col);
    case '!':
        if (next == '=') { advance(lexer); return make_token(TOKEN_NEQ, "!=", start_line, start_col); }
        break;
    case '<':
        if (next == '=') { advance(lexer); return make_token(TOKEN_LTE, "<=", start_line, start_col); }
        return make_token(TOKEN_LT, "<", start_line, start_col);
    case '>':
        if (next == '=') { advance(lexer); return make_token(TOKEN_GTE, ">=", start_line, start_col); }
        return make_token(TOKEN_GT, ">", start_line, start_col);
    default:
        break;
    }

    char val[2] = {c, '\0'};
    report_error("Unrecognized operator", start_line, start_col);
    return make_token(TOKEN_ERROR, val, start_line, start_col);
}

/* Scans a single-character delimiter. */
static Token scan_delimiter(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col  = lexer->column;
    char c = current_char(lexer);
    advance(lexer);

    char val[2] = {c, '\0'};

    switch (c)
    {
    case '(': return make_token(TOKEN_LPAREN,   val, start_line, start_col);
    case ')': return make_token(TOKEN_RPAREN,   val, start_line, start_col);
    case '[': return make_token(TOKEN_LBRACKET, val, start_line, start_col);
    case ']': return make_token(TOKEN_RBRACKET, val, start_line, start_col);
    case '{': return make_token(TOKEN_LBRACE,   val, start_line, start_col);
    case '}': return make_token(TOKEN_RBRACE,   val, start_line, start_col);
    case ':': return make_token(TOKEN_COLON,    val, start_line, start_col);
    case ',': return make_token(TOKEN_COMMA,    val, start_line, start_col);
    case '.': return make_token(TOKEN_DOT,      val, start_line, start_col);
    default:  break;
    }

    report_error("Unrecognized delimiter", start_line, start_col);
    return make_token(TOKEN_ERROR, val, start_line, start_col);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

/* Initializes the lexer state for the given null-terminated source string. */
void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source       = source;
    lexer->position     = 0;
    lexer->length       = (int)strlen(source);
    lexer->line         = 1;
    lexer->column       = 1;
    lexer->pending_dedents = 0;
    lexer->at_line_start   = 1;
    lexer->buffer_length   = 0;
    lexer->buffer[0]       = '\0';

    indent_stack_init(&lexer->indent_stack);
    indent_stack_push(&lexer->indent_stack, 0); /* base indentation level */

    keyword_table_init(&lexer->keyword_table);
}

/* Returns the next token from the source stream.
   Must be called repeatedly until TOKEN_EOF is returned. */
Token lexer_next_token(Lexer *lexer)
{
    /* Deliver any queued DEDENTs before reading new input */
    if (lexer->pending_dedents > 0)
    {
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
    }

    /* Emit remaining DEDENTs at end of file */
    if (at_end(lexer))
    {
        if (indent_stack_peek(&lexer->indent_stack) > 0)
        {
            indent_stack_pop(&lexer->indent_stack);
            return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
        }
        return make_token(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    /* Handle newline: emit NEWLINE token and set at_line_start */
    if (current_char(lexer) == '\n')
    {
        int tok_line = lexer->line;
        int tok_col  = lexer->column;
        advance(lexer); /* consume '\n', line/col updated inside advance */
        lexer->at_line_start = 1;
        return make_token(TOKEN_NEWLINE, "\\n", tok_line, tok_col);
    }

    /* Handle tab outside of indentation position */
    if (current_char(lexer) == '\t')
    {
        int tok_line = lexer->line;
        int tok_col  = lexer->column;
        report_error("Tab character not allowed", tok_line, tok_col);
        advance(lexer);
        return make_token(TOKEN_ERROR, "\t", tok_line, tok_col);
    }

    /* At start of a logical line: handle indentation */
    if (lexer->at_line_start)
    {
        lexer->at_line_start = 0;
        Token ind = handle_indentation(lexer);

        /* TOKEN_COUNT is our "nothing to emit" sentinel */
        if (ind.type != TOKEN_COUNT)
            return ind;

        /* Blank/comment line: consume the newline and stay in loop */
        if (current_char(lexer) == '#' || current_char(lexer) == '\n' || at_end(lexer))
        {
            lexer->at_line_start = 1;
            return lexer_next_token(lexer);
        }
    }

    /* Skip spaces and comments mid-line */
    skip_whitespace_and_comments(lexer);

    /* Re-check after skipping (e.g. comment consumed rest of line) */
    if (at_end(lexer))
        return lexer_next_token(lexer);

    if (current_char(lexer) == '\n')
        return lexer_next_token(lexer);

    char c = current_char(lexer);

    /* Route to the appropriate scanner */
    if (is_letter(c) || c == '_')
        return scan_word(lexer);

    if (is_digit(c))
        return scan_number(lexer);

    if (c == '"' || c == '\'')
        return scan_string(lexer);

    if (c == '(' || c == ')' || c == '[' || c == ']' ||
        c == '{' || c == '}' || c == ':' || c == ',' || c == '.')
        return scan_delimiter(lexer);

    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
        c == '=' || c == '!' || c == '<' || c == '>')
        return scan_operator(lexer);

    /* Unknown character */
    int tok_line = lexer->line;
    int tok_col  = lexer->column;
    char val[2]  = {c, '\0'};
    report_error("Unrecognized character", tok_line, tok_col);
    advance(lexer);
    return make_token(TOKEN_ERROR, val, tok_line, tok_col);
}
