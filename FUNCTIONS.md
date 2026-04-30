# Function Reference

## View — `src/View/main.c`

| Function | Returns | Notes |
|---|---|---|
| `main(int argc, char *argv[])` | `int` | `0` success, `1` error |
| `commands_line_handle(argc, argv, &input_file, &output_file)` | `int` | `0` success, `1` clean exit (help/version), `-1` error |
| `file_initialization(input_file, &source)` | `int` | `0` success, `-1` error; allocates `*source` with `malloc` |
| `print_help(void)` | `void` | Prints usage to stdout |
| `print_version(void)` | `void` | Prints version to stdout |

---

## Controller — `src/controller/compiler.c`

| Function | Returns | Notes |
|---|---|---|
| `compile(source, output_file)` | `int` | `0` success; placeholder for full compilation pipeline |

---

## Model — Lexer (`src/Model/lexer/`)

### Public API — `lexer.h`

| Function | Returns | Notes |
|---|---|---|
| `lexer_init(lexer, source)` | `void` | Initializes all lexer fields; seeds the indent stack and keyword table |
| `lexer_next_token(lexer)` | `Token` | Returns the next token; call repeatedly until `TOKEN_EOF` |

### Internal helpers — `lexer.c`

| Function | Returns | Notes |
|---|---|---|
| `current_char(lexer)` | `char` | Current character, or `'\0'` at end |
| `peek_next(lexer)` | `char` | One character ahead, or `'\0'` at end |
| `at_end(lexer)` | `int` | `1` if all input consumed |
| `advance(lexer)` | `void` | Moves position forward; updates line/column |
| `make_token(type, value, line, col)` | `Token` | Constructs and returns a `Token` |
| `is_letter(c)` | `int` | `1` if ASCII letter |
| `is_digit(c)` | `int` | `1` if ASCII decimal digit |
| `is_alphanumeric(c)` | `int` | `1` if letter or digit |
| `is_valid_identifier(s)` | `int` | `1` if valid Python identifier |
| `skip_whitespace_and_comments(lexer)` | `void` | Skips spaces and `#` comments mid-line |
| `count_indent(lexer)` | `int` | Spaces on current line; `-1` on tab error |
| `handle_indentation(lexer)` | `Token` | Emits `TOKEN_INDENT` / `TOKEN_DEDENT`; `TOKEN_COUNT` = nothing to emit |
| `scan_word(lexer)` | `Token` | Scans identifier or keyword |
| `scan_number(lexer)` | `Token` | Scans integer or float literal |
| `scan_string(lexer)` | `Token` | Scans single- or double-quoted string |
| `scan_operator(lexer)` | `Token` | Scans operator with one char of lookahead |
| `scan_delimiter(lexer)` | `Token` | Scans single-character delimiter |
| `report_error(message, line, column)` | `void` | Placeholder; will call `errors_add()` when implemented |

---

## Model — Error Handling (`src/Model/errors/errors.h`)

| Function | Returns | Notes |
|---|---|---|
| `error_list_init(list)` | `void` | Resets the error list to empty |
| `error_list_add(list, message, line, column)` | `void` | Appends one error; silently ignores if list is full |
| `error_list_write_log(list)` | `void` | Writes all errors to `errors.log`; writes a success message if list is empty |

---

## Model — Indent Stack (`src/Model/lexer/indent_stack.h`)

| Function | Returns | Notes |
|---|---|---|
| `indent_stack_init(stack)` | `void` | Resets stack to empty |
| `indent_stack_push(stack, value)` | `void` | Pushes indent level; silently ignores if full |
| `indent_stack_pop(stack)` | `int` | Popped value, or `-1` if empty |
| `indent_stack_peek(stack)` | `int` | Top value, or `0` if empty |
| `indent_stack_is_empty(stack)` | `int` | `1` if empty |
| `indent_stack_size(stack)` | `int` | Number of elements on the stack |

---

## Model — Keyword Table (`src/Model/lexer/keyword_table.h`)

| Function | Returns | Notes |
|---|---|---|
| `keyword_table_init(table)` | `void` | Clears table and inserts all Python keywords |
| `keyword_table_lookup(table, word)` | `TokenType` | Returns keyword token type, or `TOKEN_IDENTIFIER` if not found |
| `hash_string(s)` *(static)* | `unsigned int` | Polynomial hash used for open-addressing |
| `keyword_table_insert(table, keyword, token_type)` *(static)* | `void` | Inserts one entry with linear probing |
