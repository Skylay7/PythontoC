/* lexer.c */

#include "lexer.h"
#include <string.h>
#include <stdlib.h>

static void report_error(Lexer *lexer, const char *message, int line, int column)
{
    error_list_add_staged(lexer->errors, message, line, column, STAGE_LEXER);
}

// Position helpers — read-only queries on the current lexer state.

// Returns '\0' at end of input.
static char current_char(const Lexer *lexer)
{
    if (lexer->position >= lexer->length)
        return '\0';
    return lexer->source[lexer->position];
}

// Returns '\0' if there is no next character.
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

// Advances one character and updates line/column tracking.
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

// Character classification helpers.

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

/* Returns 1 if s is a valid Python identifier:
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

// Skips spaces (not newlines) and '#' line comments.
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
            // skip to end of line but don't consume the newline
            while (!at_end(lexer) && current_char(lexer) != '\n')
                advance(lexer);
        }
        else
        {
            break;
        }
    }
}

// Counts leading spaces on the current line.
// Returns -1 and reports an error if a tab is found.
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

/* Handles indentation at the start of a logical line (the PDA part).
   Returns TOKEN_COUNT as a sentinel when there is nothing to emit yet
   (blank line, same indent level). Otherwise returns INDENT or DEDENT. */
static Token handle_indentation(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;

    int indent = count_indent(lexer);

    // blank line or comment-only line — not a logical line, nothing to emit
    if (current_char(lexer) == '\n' || current_char(lexer) == '#' || at_end(lexer))
    {
        Token skip;
        skip.type = TOKEN_COUNT; // sentinel: "nothing to emit"
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
        // pop until we match, counting how many DEDENTs are owed
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
        // return first DEDENT now; the rest come out via pending_dedents
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", start_line, start_col);
    }
}

// Scanners — each one consumes characters and returns one token.

// Scans an identifier or keyword; looks up in the keyword table to decide token type.
static Token scan_word(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    int i = 0;
    char buf[LEXER_BUFFER_SIZE];

    while (!at_end(lexer) && (is_alphanumeric(current_char(lexer)) || current_char(lexer) == '_'))
    {
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }
    buf[i] = '\0';

    // check if it's a valid identifier
    if (!is_valid_identifier(buf))
    {
        report_error(lexer, "Invalid identifier", start_line, start_col);
        return make_token(TOKEN_ERROR, buf, start_line, start_col);
    }

    // check if it's a keyword
    TokenType kw = keyword_table_lookup(&lexer->keyword_table, buf);
    return make_token(kw, buf, start_line, start_col);
}

// Scans an integer or float literal (floats require a digit after the dot).
static Token scan_number(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    int i = 0;
    char buf[LEXER_BUFFER_SIZE];
    int is_float = 0;

    // integer part
    while (!at_end(lexer) && is_digit(current_char(lexer)))
    {
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }

    // optional fractional part if it is a FLOAT_LITERAL
    if (!at_end(lexer) && current_char(lexer) == '.' && is_digit(peek_next(lexer)))
    {
        is_float = 1;
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = '.';
        advance(lexer); // consume '.'

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

// Scans a single- or double-quoted string. Reports an error on unterminated strings.
static Token scan_string(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    char quote = current_char(lexer);
    advance(lexer); // consume opening quote

    int i = 0;
    char buf[LEXER_BUFFER_SIZE];

    while (!at_end(lexer) && current_char(lexer) != quote)
    {
        if (current_char(lexer) == '\n')
        {
            report_error(lexer, "Unterminated string literal", start_line, start_col);
            buf[i] = '\0';
            return make_token(TOKEN_ERROR, buf, start_line, start_col);
        }
        if (i < LEXER_BUFFER_SIZE - 1)
            buf[i++] = current_char(lexer);
        advance(lexer);
    }

    if (at_end(lexer))
    {
        report_error(lexer, "Unterminated string literal", start_line, start_col);
        buf[i] = '\0';
        return make_token(TOKEN_ERROR, buf, start_line, start_col);
    }

    advance(lexer); // consume closing quote
    buf[i] = '\0';
    return make_token(TOKEN_STRING_LITERAL, buf, start_line, start_col);
}

// Scans operators, using one character of lookahead for two-char forms.
static Token scan_operator(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    char c = current_char(lexer);
    advance(lexer);
    char next = current_char(lexer);

    switch (c)
    {
    case '+':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_PLUS_ASSIGN, "+=", start_line, start_col);
        }
        return make_token(TOKEN_PLUS, "+", start_line, start_col);
    case '-':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_MINUS_ASSIGN, "-=", start_line, start_col);
        }
        return make_token(TOKEN_MINUS, "-", start_line, start_col);
    case '*':
        if (next == '*')
        {
            advance(lexer);
            return make_token(TOKEN_DOUBLE_STAR, "**", start_line, start_col);
        }
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_STAR_ASSIGN, "*=", start_line, start_col);
        }
        return make_token(TOKEN_STAR, "*", start_line, start_col);
    case '/':
        if (next == '/')
        {
            advance(lexer);
            return make_token(TOKEN_DOUBLE_SLASH, "//", start_line, start_col);
        }
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_SLASH_ASSIGN, "/=", start_line, start_col);
        }
        return make_token(TOKEN_SLASH, "/", start_line, start_col);
    case '%':
        return make_token(TOKEN_PERCENT, "%", start_line, start_col);
    case '=':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_EQ, "==", start_line, start_col);
        }
        return make_token(TOKEN_ASSIGN, "=", start_line, start_col);
    case '!':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_NEQ, "!=", start_line, start_col);
        }
        break;
    case '<':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_LTE, "<=", start_line, start_col);
        }
        return make_token(TOKEN_LT, "<", start_line, start_col);
    case '>':
        if (next == '=')
        {
            advance(lexer);
            return make_token(TOKEN_GTE, ">=", start_line, start_col);
        }
        return make_token(TOKEN_GT, ">", start_line, start_col);
    default:
        break;
    }

    char val[2] = {c, '\0'};
    report_error(lexer, "Unrecognized operator", start_line, start_col);
    return make_token(TOKEN_ERROR, val, start_line, start_col);
}

// Scans a single-character delimiter.
static Token scan_delimiter(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    char c = current_char(lexer);
    advance(lexer);

    char val[2] = {c, '\0'};

    switch (c)
    {
    case '(':
        return make_token(TOKEN_LPAREN, val, start_line, start_col);
    case ')':
        return make_token(TOKEN_RPAREN, val, start_line, start_col);
    case '[':
        return make_token(TOKEN_LBRACKET, val, start_line, start_col);
    case ']':
        return make_token(TOKEN_RBRACKET, val, start_line, start_col);
    case '{':
        return make_token(TOKEN_LBRACE, val, start_line, start_col);
    case '}':
        return make_token(TOKEN_RBRACE, val, start_line, start_col);
    case ':':
        return make_token(TOKEN_COLON, val, start_line, start_col);
    case ',':
        return make_token(TOKEN_COMMA, val, start_line, start_col);
    case '.':
        return make_token(TOKEN_DOT, val, start_line, start_col);
    default:
        break;
    }

    report_error(lexer, "Unrecognized delimiter", start_line, start_col);
    return make_token(TOKEN_ERROR, val, start_line, start_col);
}

// Initializes the lexer state for the given null-terminated source string.
void lexer_init(Lexer *lexer, const char *source, ErrorList *errors)
{
    lexer->source = source;
    lexer->position = 0; // start at the beginning of the source
    lexer->length = (int)strlen(source);
    lexer->line = 1;
    lexer->column = 1;
    lexer->pending_dedents = 0;
    lexer->at_line_start = 1; // start at the beginning of a line
    lexer->buffer_length = 0;
    lexer->buffer[0] = '\0';
    lexer->errors = errors;

    indent_stack_init(&lexer->indent_stack);
    indent_stack_push(&lexer->indent_stack, 0); // base indentation level

    keyword_table_init(&lexer->keyword_table);
}

/* Returns the next token from the source stream.
   Call repeatedly until TOKEN_EOF. Handles INDENT/DEDENT internally. */
Token lexer_next_token(Lexer *lexer)
{
    // deliver any queued DEDENTs before reading new input
    if (lexer->pending_dedents > 0)
    {
        lexer->pending_dedents--;
        return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
    }

    // emit remaining DEDENTs at end of file before the final EOF token
    if (at_end(lexer))
    {
        if (indent_stack_peek(&lexer->indent_stack) > 0)
        {
            indent_stack_pop(&lexer->indent_stack);
            return make_token(TOKEN_DEDENT, "", lexer->line, lexer->column);
        }
        return make_token(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    // handle newlines and indentation at the start of logical lines
    if (current_char(lexer) == '\n')
    {
        int start_line = lexer->line;
        int start_col = lexer->column;
        advance(lexer);
        lexer->at_line_start = 1;
        return make_token(TOKEN_NEWLINE, "\\n", start_line, start_col);
    }

    if (current_char(lexer) == '\t') // tab outside of indentation position
    {
        int start_line = lexer->line;
        int start_col = lexer->column;
        report_error(lexer, "Tab character not allowed", start_line, start_col);
        advance(lexer);
        return make_token(TOKEN_ERROR, "\t", start_line, start_col);
    }

    // handle indentation if we're at the start of a logical line
    if (lexer->at_line_start)
    {
        lexer->at_line_start = 0;
        Token ind = handle_indentation(lexer);

        if (ind.type != TOKEN_COUNT) // TOKEN_COUNT means "nothing to emit, keep going"
            return ind;

        // blank/comment line — consume comment so the recursive call terminates
        if (current_char(lexer) == '#' || current_char(lexer) == '\n' || at_end(lexer))
        {
            skip_whitespace_and_comments(lexer);
            lexer->at_line_start = 1;
            return lexer_next_token(lexer);
        }
    }

    // Skip spaces and comments mid-line
    skip_whitespace_and_comments(lexer);

    // Re-check after skipping (e.g. comment consumed rest of line)
    if (at_end(lexer))
        return lexer_next_token(lexer);

    if (current_char(lexer) == '\n')
        return lexer_next_token(lexer);

    char c = current_char(lexer);

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

    // it's an Unknown character.
    int start_line = lexer->line;
    int start_col = lexer->column;
    char val[2] = {c, '\0'};
    report_error(lexer, "Unrecognized character", start_line, start_col);
    advance(lexer);
    return make_token(TOKEN_ERROR, val, start_line, start_col);
}
