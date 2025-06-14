/*
 * =====================================================================================
 *
 * The MIT License
 *
 * Copyright (c) 2025 Alfred Jijo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * =====================================================================================
 *
 * HOW TO USE
 *
 * This is a single-file header-only library. To use it, do the following:
 *
 * 1.  Drop this file (`csview.h`) into your project directory.
 * 2.  In exactly ONE of your C source files, add the following lines:
 *
 * #define CSVIEW_IMPLEMENTATION
 * #include "csview.h"
 *
 * 3.  In all other source files where you need to use the library, just
 * include the header normally:
 *
 * #include "csview.h"
 *
 *
 * AUTOMATIC MEMORY MANAGEMENT
 *
 * If you are compiling with GCC or Clang, you can use the `autofree_csv` macro
 * to automatically free the memory of a csv_document_t when it goes out of scope.
 *
 * Example:
 *
 * void my_function() {
 * autofree_csv csv_document_t* doc = csv_read("my_file.csv", true);
 * if (!doc) {
 * return;
 * }
 * // ... do work with doc ...
 *
 * } // `csv_free(&doc)` is automatically called here.
 *
 *
 * =====================================================================================
 *
 * Library:  csview.h
 * Version:  2.1
 * Created:  06/11/2024
 *
 * Author:  Alfred Jijo
 * =====================================================================================
 */

#ifndef CSVIEW_H
#define CSVIEW_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// -------------------------------------------------------------------------------------
// Compiler-Specific Macros for Automatic Cleanup
// -------------------------------------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
        #define autofree_csv __attribute__((cleanup(csv_free)))
#else
        #define autofree_csv
#endif

// -------------------------------------------------------------------------------------
// Data Structures
// -------------------------------------------------------------------------------------
typedef struct {
        char **fields;
        int num_fields;
} csv_row_t;

typedef struct {
        csv_row_t **rows;
        int num_rows;
        char **header;
        int num_cols;
} csv_document_t;

// -------------------------------------------------------------------------------------
// Function Prototypes
// -------------------------------------------------------------------------------------
csv_document_t* csv_read(const char* file_path, bool has_header);
int csv_write(const csv_document_t* doc, const char* file_path);
void csv_free(csv_document_t** doc_ptr);
void csv_show(const csv_document_t* doc);
void csv_info(const csv_document_t* doc);


// =====================================================================================
//
//    IMPLEMENTATION SECTION
//
// =====================================================================================

#ifdef CSVIEW_IMPLEMENTATION

// -- Platform-Specific Header Includes --
#if defined(_WIN32)
        #include <windows.h>
#else
        #include <unistd.h> // For read, write, close
        #include <fcntl.h>  // For open
        #include <errno.h>  // For errno
#endif

// -- Platform-Agnostic Type Definitions for File Handles --
#if defined(_WIN32)
        typedef HANDLE platform_file_t;
        const platform_file_t PLATFORM_INVALID_FILE = INVALID_HANDLE_VALUE;
#else
        typedef int platform_file_t;
        const platform_file_t PLATFORM_INVALID_FILE = -1;
#endif

// Unnamed enum for internal constants, replacing "magic numbers"
enum {
        _CSV_INTERNAL_BUFFER_SIZE = 4096,
        _CSV_INITIAL_ROW_CAPACITY = 10,
        _CSV_INITIAL_FIELD_CAPACITY = 10,
        _CSV_MAX_INT_STR_SIZE = 12 // For -2147483647 and null terminator
};

static
void
_itoa(int value,
      char* buffer)
{
        char temp[12];
        int i = 0;
        int j = 0;
        bool is_negative = false;

        if (value == 0) {
                buffer[0] = '0';
                buffer[1] = '\0';
                return;
        }

        if (value < 0) {
                is_negative = true;
                value = -value;
        }

        while (value > 0) {
                temp[i++] = (value % 10) + '0';
                value /= 10;
        }

        if (is_negative) {
                buffer[j++] = '-';
        }

        while (i > 0) {
                buffer[j++] = temp[--i];
        }
        buffer[j] = '\0';
}


// Writes a buffer to the console (stdout)
static
void
_platform_console_write(const char* buffer,
			size_t len)
{
#if defined(_WIN32)
        HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD bytes_written;
        WriteFile(h_stdout, buffer, (DWORD)len, &bytes_written, NULL);
#else
        write(STDOUT_FILENO, buffer, len);
#endif
}

// Opens a file for reading
static
platform_file_t
_platform_file_open_read(const char* path)
{
#if defined(_WIN32)
        return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else
        return open(path, O_RDONLY);
#endif
}

// Opens a file for writing, creates it if it doesn't exist, truncates it if it does
static
platform_file_t
_platform_file_open_write(const char* path)
{
#if defined(_WIN32)
        return CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else
        return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
#endif
}

// Reads from a file into a buffer
static
long
_platform_file_read(platform_file_t file,
		    char* buffer,
		    long buffer_size)
{
#if defined(_WIN32)
        DWORD bytes_read;
        if (!ReadFile(file, buffer, (DWORD)buffer_size, &bytes_read, NULL)) {
                return -1;
        }
        return (long)bytes_read;
#else
        return read(file, buffer, (size_t)buffer_size);
#endif
}

// Writes a buffer to a file
static
bool
_platform_file_write(platform_file_t file,
		     const char* buffer,
		     long len)
{
#if defined(_WIN32)
        DWORD bytes_written;
        if (!WriteFile(file, buffer, (DWORD)len, &bytes_written, NULL)) {
                return false;
        }
        return bytes_written == (DWORD)len;
#else
        ssize_t result = write(file, buffer, (size_t)len);
        return result == len;
#endif
}

// Closes a file
static
void
_platform_file_close(platform_file_t file)
{
#if defined(_WIN32)
        CloseHandle(file);
#else
        close(file);
#endif
}

// A buffered reader struct to replace `fgets`
typedef struct {
        platform_file_t file;
        char buffer[_CSV_INTERNAL_BUFFER_SIZE];
        long bytes_in_buffer;
        long current_pos;
} buffered_reader_t;

// Reads one line from the buffered reader. Returns bytes in line, or 0 for EOF, or -1 for error.
static
long
_read_line(buffered_reader_t* reader,
	   char* line_out,
	   long max_len)
{
        long line_pos = 0;
        while (line_pos < max_len - 1) {
                if (reader->current_pos >= reader->bytes_in_buffer) {
                        reader->current_pos = 0;
                        reader->bytes_in_buffer = _platform_file_read(reader->file, reader->buffer, _CSV_INTERNAL_BUFFER_SIZE);
                        if (reader->bytes_in_buffer <= 0) {
                                break; // EOF or error
                        }
                }

                char c = reader->buffer[reader->current_pos++];
                if (c == '\n') {
                        break;
                }
                if (c != '\r') {
                        line_out[line_pos++] = c;
                }
        }
        line_out[line_pos] = '\0';
        return (reader->bytes_in_buffer <= 0 && line_pos == 0) ? reader->bytes_in_buffer : line_pos;
}


// ---- Main Library Implementation ----
static
csv_row_t*
_parse_csv_line(const char* line)
{
        csv_row_t* row = (csv_row_t*)malloc(sizeof(csv_row_t));
        if (!row) { return NULL; }

        row->fields = NULL;
        row->num_fields = 0;
        
        const char* ptr = line;
        int field_capacity = _CSV_INITIAL_FIELD_CAPACITY;
        row->fields = (char**)malloc(field_capacity * sizeof(char*));

        while (*ptr) {
                const char* start = ptr;
                const char* end = start;
                bool in_quotes = false;

                if (*ptr == '"') {
                        in_quotes = true;
                        start++;
                        end = strchr(start, '"');
                        if (!end) { end = start + strlen(start); }
                } else {
                        end = strchr(start, ',');
                        if (!end) { end = start + strlen(start); }
                }

                int len = end - start;
                char* field = (char*)malloc(len + 1);
                strncpy(field, start, len);
                field[len] = '\0';
                
                if (row->num_fields >= field_capacity) {
                        field_capacity *= 2;
                        row->fields = (char**)realloc(row->fields, field_capacity * sizeof(char*));
                }
                row->fields[row->num_fields++] = field;
                
                ptr = end;
                if (in_quotes && *ptr == '"') { ptr++; }
                if (*ptr == ',') { ptr++; }
                while (*ptr == ' ' || *ptr == '\t') { ptr++; }
        }
        return row;
}

csv_document_t*
csv_read(const char* file_path,
         bool has_header)
{
        platform_file_t file = _platform_file_open_read(file_path);
        if (file == PLATFORM_INVALID_FILE) {
                // Not using perror, but could log GetLastError/errno here
                return NULL;
        }

        buffered_reader_t reader = { .file = file, .bytes_in_buffer = 0, .current_pos = 0 };
        csv_document_t* doc = (csv_document_t*)calloc(1, sizeof(csv_document_t));
        if (!doc) {
                _platform_file_close(file);
                return NULL;
        }

        char line[_CSV_INTERNAL_BUFFER_SIZE];
        int row_capacity = _CSV_INITIAL_ROW_CAPACITY;
        doc->rows = (csv_row_t**)malloc(row_capacity * sizeof(csv_row_t*));

        // Read header
        if (has_header && _read_line(&reader, line, sizeof(line)) > 0) {
                csv_row_t* header_row = _parse_csv_line(line);
                doc->header = header_row->fields;
                doc->num_cols = header_row->num_fields;
                free(header_row);
        }

        // Read data rows
        while (_read_line(&reader, line, sizeof(line)) > 0) {
                if (strlen(line) == 0) continue;

                if (doc->num_rows >= row_capacity) {
                        row_capacity *= 2;
                        doc->rows = (csv_row_t**)realloc(doc->rows, row_capacity * sizeof(csv_row_t*));
                }
                csv_row_t* current_row = _parse_csv_line(line);
                doc->rows[doc->num_rows++] = current_row;
                if (doc->num_cols == 0) {
                        doc->num_cols = current_row->num_fields;
                }
        }

        _platform_file_close(file);
        return doc;
}

int
csv_write(const csv_document_t* doc,
          const char* file_path)
{
        platform_file_t file = _platform_file_open_write(file_path);
        if (file == PLATFORM_INVALID_FILE) { return -1; }

        // Write header
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        _platform_file_write(file, doc->header[i], (long)strlen(doc->header[i]));
                        if (i < doc->num_cols - 1) {
                                _platform_file_write(file, ",", 1);
                        }
                }
                _platform_file_write(file, "\n", 1);
        }

        // Write rows
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        _platform_file_write(file, doc->rows[i]->fields[j], (long)strlen(doc->rows[i]->fields[j]));
                        if (j < doc->rows[i]->num_fields - 1) {
                                _platform_file_write(file, ",", 1);
                        }
                }
                _platform_file_write(file, "\n", 1);
        }

        _platform_file_close(file);
        return 0;
}

void
csv_free(csv_document_t** doc_ptr)
{
        if (!doc_ptr || !*doc_ptr) { return; }
        csv_document_t* doc = *doc_ptr;

        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) { free(doc->header[i]); }
                free(doc->header);
        }

        if (doc->rows) {
                for (int i = 0; i < doc->num_rows; i++) {
                        if (doc->rows[i]) {
                                for (int j = 0; j < doc->rows[i]->num_fields; j++) { free(doc->rows[i]->fields[j]); }
                                free(doc->rows[i]->fields);
                                free(doc->rows[i]);
                        }
                }
                free(doc->rows);
        }
        free(doc);
}

void
csv_show(const csv_document_t* doc)
{
        if (!doc) {
                _platform_console_write("CSV Document is NULL.\n", 22);
                return;
        }

        int* col_widths = (int*)calloc(doc->num_cols, sizeof(int));
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) { col_widths[i] = (int)strlen(doc->header[i]); }
        }
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        int len = (int)strlen(doc->rows[i]->fields[j]);
                        if (j < doc->num_cols && len > col_widths[j]) { col_widths[j] = len; }
                }
        }
        
        char buffer[_CSV_INTERNAL_BUFFER_SIZE];

        // Print header
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        _platform_console_write(doc->header[i], strlen(doc->header[i]));
                        for(int p = strlen(doc->header[i]); p < col_widths[i]; p++) { _platform_console_write(" ", 1); }
                        _platform_console_write(" | ", 3);
                }
                _platform_console_write("\n", 1);
                for (int i = 0; i < doc->num_cols; i++) {
                        for(int j = 0; j < col_widths[i]; j++) { _platform_console_write("-", 1); }
                        _platform_console_write("-+-", 3);
                }
                _platform_console_write("\n", 1);
        }
        
        // Print rows
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        if (j < doc->num_cols) {
                                _platform_console_write(doc->rows[i]->fields[j], strlen(doc->rows[i]->fields[j]));
                                for(int p = strlen(doc->rows[i]->fields[j]); p < col_widths[j]; p++) { _platform_console_write(" ", 1); }
                                _platform_console_write(" | ", 3);
                        }
                }
                _platform_console_write("\n", 1);
        }

        free(col_widths);
}

void
csv_info(const csv_document_t* doc)
{
        if (!doc) {
                _platform_console_write("CSV Document is NULL.\n", 22);
                return;
        }
        
        char num_buffer[_CSV_MAX_INT_STR_SIZE];
        _platform_console_write("--- CSV Info ---\n", 17);
        _platform_console_write("Rows:    ", 9);
        _itoa(doc->num_rows, num_buffer);
        _platform_console_write(num_buffer, strlen(num_buffer));
        _platform_console_write("\n", 1);
        
        _platform_console_write("Columns: ", 9);
        _itoa(doc->num_cols, num_buffer);
        _platform_console_write(num_buffer, strlen(num_buffer));
        _platform_console_write("\n", 1);

        _platform_console_write("Header:  ", 9);
        _platform_console_write(doc->header ? "Yes\n" : "No\n", doc->header ? 4 : 3);
        _platform_console_write("----------------\n", 17);
}


#endif // CSVIEW_IMPLEMENTATION
#endif // CSVIEW_H
