/* compiler.h — top-level entry point that drives all compilation stages. */

#ifndef COMPILER_H
#define COMPILER_H

/* Compile Python source text and write the resulting C file to output_file.
   Returns 0 on success, non-zero if any errors were reported. */
int compile(const char *source, const char *output_file);

#endif