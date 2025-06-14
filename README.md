# csview.h

`csview.h` is a simple, modern, single-file C99 header library for reading, writing, and displaying CSV (Comma Separated Values) data. It is designed to be dropped into any C project with zero configuration.

---

## Features

- **CLI Display:** Includes a `csv_show()` function to print formatted tables directly to the console.
- **Automatic Cleanup (Optional):** Uses a GCC/Clang attribute `__attribute__((cleanup))` to provide automatic memory management, reducing the risk of memory leaks.
- **Portable:** The automatic cleanup feature is wrapped in a macro, allowing the code to compile on any C99-compliant compiler (like MSVC), where manual cleanup is required.

---

## How to Use

To use `csview.h` in your project, you need to do two things:

1.  Place `csview.h` in your project's source or include directory.
2.  In **one** C file, define `CSVIEW_IMPLEMENTATION` before including the header. This tells the compiler to build the function bodies.

```c
#define CSVIEW_IMPLEMENTATION
#include "csview.h"
```

## Building the Example

The provided main.c demonstrates the library's features. To compile it on a system with GCC or Clang, run the following command in your terminal:
```shell
gcc -std=c99 -Wall -o main main.c
```

Then, run the compiled program:

```shell
./main
```

## Examples

The library supports two memory management patterns:
    - automatic for GCC/Clang 
    - manual for any other compiler

### Example 1: Automatic Cleanup

On GCC and Clang, you can use the autofree_csv macro to ensure memory is automatically freed when the variable goes out of scope. This is the recommended approach for compatible compilers.

```c
void
automatic_cleanup()
{
        printf("--- DEMO 1: Automatic Cleanup with `autofree_csv` ---\n\n");

        // We use a block scope `{...}` to demonstrate the cleanup.
        // When the 'doc' variable goes out of scope at the closing brace,
        // csv_free() will be called on it automatically by the compiler.
        {
                printf("Reading 'test.csv'...\n");
                autofree_csv csv_document_t* doc = csv_read("test.csv", true);
                if (!doc) {
                        fprintf(stderr, "Failed to read 'test.csv'.\n");
                        return;
                }

                csv_info(doc);
                printf("\nDisplaying CSV contents:\n");
                csv_show(doc);

                printf("\nWriting data to 'new_test.csv'...\n");
                if (csv_write(doc, "new_test.csv") == 0) {
                        printf("Successfully wrote to new_test.csv.\n");
                }

                printf("\n'doc' is now going out of scope. Cleanup will be automatic.\n");
        } // <-- csv_free(&doc) is automatically called here on GCC/Clang

        printf("\n--- End of Demo 1 ---\n\n");
}
```

### Example 2: Manual Cleanup

If you are using a different compiler (like MSVC) or prefer to manage memory explicitly, you can call csv_free() yourself. This works on all C99 compilers.

```c
void
manual_cleanup()
{
        printf("--- DEMO 2: Manual Cleanup with `csv_free()` ---\n\n");

        // Here, we do not use the macro and must clean up memory ourselves.
        csv_document_t* doc = csv_read("new_test.csv", true);
        if (!doc) {
                fprintf(stderr, "Failed to read 'new_test.csv'.\n");
                return;
        }

        printf("Successfully read 'new_test.csv'.\n");
        csv_info(doc);

        printf("\nCalling csv_free() manually...\n");

        // Note: We must pass the ADDRESS of our pointer to csv_free.
        csv_free(&doc);

        // After freeing, the pointer is invalid. It's good practice to NULL it.
        doc = NULL;

        printf("Memory has been freed.\n");
        printf("\n--- End of Demo 2 ---\n");
}
```
