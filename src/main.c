/* main.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer/lexer.h"
#include "lexer/token_types.h"

/* Program version and name */
#define PROGRAM_NAME "pytoc"
#define PROGRAM_VERSION "0.1.0"

static void commands_line_handle(int argc, char *argv[], const char *input_file, const char *output_file);
static void print_help(void);
static void print_version(void);

int main(int argc, char *argv[])
{
    const char *input_file = NULL;
    const char *output_file = "output.c";

    commands_line_handle(argc, argv, input_file, output_file);

    /* Check that an input file was provided */
    if (input_file == NULL)
    {
        fprintf(stderr, "Error: no input file specified\n");
        fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
        return 1;
    }

    /* Read source file into memory */
    FILE *file = fopen(input_file, "r");
    if (!file)
    {
        fprintf(stderr, "Error: cannot open %s\n", input_file);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = malloc(file_size + 1);
    fread(source, 1, file_size, file);
    source[file_size] = '\0';
    fclose(file);

    /* Initialize lexer and scan all tokens */
    Lexer lexer;
    lexer_init(&lexer, source);

    Token token;
    do
    {
        token = lexer_next_token(&lexer);
        printf("Line %d Col %d: type=%d value='%s'\n",
               token.line, token.column, token.type, token.value);
    } while (token.type != TOKEN_EOF);

    free(source);
    return 0;
}

static void commands_line_handle(int argc, char *argv[], const char *input_file, const char *output_file)
{
    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            print_help();
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
        {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            /* -o requires a filename after it */
            if (i + 1 >= argc)
            {
                fprintf(stderr, "Error: -o requires a filename argument\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
                return 1;
            }
            output_file = argv[i + 1];
            i++; /* skip the filename on next iteration */
        }
        else if (argv[i][0] == '-')
        {
            /* Unknown option */
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
            return 1;
        }
        else
        {
            /* Not an option — must be the input file */
            if (input_file != NULL)
            {
                fprintf(stderr, "Error: multiple input files specified\n");
                fprintf(stderr, "Try '%s --help' for more information.\n", PROGRAM_NAME);
                return 1;
            }
            input_file = argv[i];
        }
    }
}

/* Print the help message */
static void print_help(void)
{
    printf("Usage: %s [options] <input.py>\n", PROGRAM_NAME);
    printf("\n");
    printf("A compiler that translates Python source code to C.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message and exit\n");
    printf("  -v, --version    Show version information and exit\n");
    printf("  -o <file>        Write output to <file> (default: output.c)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s hello.py                 Compile hello.py to output.c\n", PROGRAM_NAME);
    printf("  %s -o result.c hello.py     Compile hello.py to result.c\n", PROGRAM_NAME);
    printf("\n");
    printf("The compiler produces two files:\n");
    printf("  output.c     The translated C source code\n");
    printf("  errors.log   A report of any errors encountered\n");
}

/* Print the version message */
static void print_version(void)
{
    printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("A Python-to-C compiler\n");
}