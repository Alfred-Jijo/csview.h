/*
 * =====================================================================================
 *
 * Filename:  csview.h
 *
 * Description:  A single-file C99 header-only library for parsing, writing,
 * and displaying CSV (Comma Separated Values) data.
 *
 * Version:  1.4
 * Created:  06/11/2024
 * Revision:  none
 * Compiler:  gcc
 *
 * Author:  Your Name
 *
 * Style:   snake_case, 8-space indent, custom function definition style
 * =====================================================================================
 */

#ifndef CSVIEW_H
#define CSVIEW_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// -------------------------------------------------------------------------------------
// Compiler-Specific Macros for Automatic Cleanup
// -------------------------------------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
        /**
         * @brief On GCC/Clang, this macro uses a cleanup attribute to automatically
         * call csv_free() when the variable goes out of scope.
         * Usage: autofree_csv csv_document_t* my_doc = csv_read(...);
         */
        #define autofree_csv __attribute__((cleanup(csv_free)))
#else
        /**
         * @brief On other compilers, this macro does nothing. You must manually
         * call csv_free().
         */
        #define autofree_csv
#endif


// -------------------------------------------------------------------------------------
// Data Structures
// -------------------------------------------------------------------------------------

/**
 * @brief Represents a single row in a CSV file.
 */
typedef struct {
        char **fields;      // Array of strings, where each string is a field in the row.
        int num_fields;     // The number of fields in this row.
} csv_row_t;

/**
 * @brief Represents a complete CSV document.
 */
typedef struct {
        csv_row_t **rows;   // Array of CsvRow pointers.
        int num_rows;       // The total number of rows in the CSV.
        char **header;      // Optional: stores the header row fields.
        int num_cols;       // The number of columns, typically based on the header or first row.
} csv_document_t;


// -------------------------------------------------------------------------------------
// Function Prototypes
// -------------------------------------------------------------------------------------

/**
 * @brief Reads a CSV file from the given path.
 *
 * @param file_path The path to the CSV file.
 * @param has_header True if the CSV file has a header row, false otherwise.
 * @return A pointer to a csv_document_t struct, or NULL on failure.
 */
csv_document_t* csv_read(const char* file_path, bool has_header);

/**
 * @brief Writes a csv_document_t to a file.
 *
 * @param doc The csv_document_t to write.
 * @param file_path The path to the output file.
 * @return 0 on success, -1 on failure.
 */
int csv_write(const csv_document_t* doc, const char* file_path);

/**
 * @brief Frees all memory associated with a csv_document_t.
 *
 * @param doc_ptr A pointer to the csv_document_t* variable to free.
 * Note: The cleanup attribute requires a function taking a pointer to the variable.
 */
void csv_free(csv_document_t** doc_ptr);

/**
 * @brief Prints the entire CSV document to the console in a formatted table.
 *
 * @param doc The csv_document_t to print.
 */
void csv_show(const csv_document_t* doc);

/**
 * @brief Prints basic information about the CSV document.
 *
 * @param doc The csv_document_t to print info for.
 */
void csv_info(const csv_document_t* doc);


// -------------------------------------------------------------------------------------
// Implementation
// -------------------------------------------------------------------------------------

#ifdef CSVIEW_IMPLEMENTATION

// Internal helper to parse a single line
static
csv_row_t*
_parse_csv_line(const char* line
		)
{
        csv_row_t* row = (csv_row_t*)malloc(sizeof(csv_row_t));
        if (!row) {
                return NULL;
        }

        row->fields = NULL;
        row->num_fields = 0;
        
        const char* ptr = line;
        int field_capacity = 10;
        row->fields = (char**)malloc(field_capacity * sizeof(char*));

        while (*ptr) {
                const char* start = ptr;
                const char* end = start;
                bool in_quotes = false;

                if (*ptr == '"') {
                        in_quotes = true;
                        start++;
                        end = strchr(start, '"');
                        if (!end) { // Malformed CSV
                                end = start + strlen(start);
                        }
                } else {
                        end = strchr(start, ',');
                        if (!end) {
                                end = start + strlen(start);
                        }
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
                if (in_quotes && *ptr == '"') {
                        ptr++;
                }
                if (*ptr == ',') {
                        ptr++;
                }
                while (*ptr == ' ' || *ptr == '\t') { // Skip whitespace
                        ptr++;
                }
        }

        return row;
}

csv_document_t*
csv_read(const char* file_path,
         bool has_header)
{
        FILE* file = fopen(file_path, "r");
        if (!file) {
                perror("Error opening file");
                return NULL;
        }

        csv_document_t* doc = (csv_document_t*)calloc(1, sizeof(csv_document_t));
        if (!doc) {
                fclose(file);
                return NULL;
        }

        char line[1024];
        int row_capacity = 10;
        doc->rows = (csv_row_t**)malloc(row_capacity * sizeof(csv_row_t*));

        // Handle header
        if (has_header && fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\r\n")] = 0; // Remove newline
                csv_row_t* header_row = _parse_csv_line(line);
                doc->header = header_row->fields;
                doc->num_cols = header_row->num_fields;
                free(header_row); // We only keep the fields
        }

        // Read data rows
        while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\r\n")] = 0; // Remove newline
                if (strlen(line) == 0) {
                        continue; // Skip empty lines
                }

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

        fclose(file);
        return doc;
}

int
csv_write(const csv_document_t* doc,
          const char* file_path)
{
        FILE* file = fopen(file_path, "w");
        if (!file) {
                perror("Error opening file for writing");
                return -1;
        }

        // Write header
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        fprintf(file, "%s%s", doc->header[i], (i == doc->num_cols - 1) ? "" : ",");
                }
                fprintf(file, "\n");
        }

        // Write rows
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        fprintf(file, "%s%s", doc->rows[i]->fields[j], (j == doc->rows[i]->num_fields - 1) ? "" : ",");
                }
                fprintf(file, "\n");
        }

        fclose(file);
        return 0;
}

void
csv_free(csv_document_t** doc_ptr)
{
        if (!doc_ptr || !*doc_ptr) {
                return;
        }
        csv_document_t* doc = *doc_ptr;

        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        free(doc->header[i]);
                }
                free(doc->header);
        }

        if (doc->rows) {
                for (int i = 0; i < doc->num_rows; i++) {
                        if (doc->rows[i]) {
                                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                                        free(doc->rows[i]->fields[j]);
                                }
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
                printf("CSV Document is NULL.\n");
                return;
        }

        // Determine column widths
        int* col_widths = (int*)calloc(doc->num_cols, sizeof(int));
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        col_widths[i] = strlen(doc->header[i]);
                }
        }
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        int len = strlen(doc->rows[i]->fields[j]);
                        if (j < doc->num_cols && len > col_widths[j]) {
                                col_widths[j] = len;
                        }
                }
        }

        // Print header
        if (doc->header) {
                for (int i = 0; i < doc->num_cols; i++) {
                        printf("%-*s | ", col_widths[i], doc->header[i]);
                }
                printf("\n");
                for (int i = 0; i < doc->num_cols; i++) {
                        for(int j = 0; j < col_widths[i]; j++) {
                                putchar('-');
                        }
                        printf("-+-");
                }
                printf("\n");
        }
        
        // Print rows
        for (int i = 0; i < doc->num_rows; i++) {
                for (int j = 0; j < doc->rows[i]->num_fields; j++) {
                        if (j < doc->num_cols) {
                                printf("%-*s | ", col_widths[j], doc->rows[i]->fields[j]);
                        }
                }
                printf("\n");
        }

        free(col_widths);
}

void
csv_info(const csv_document_t* doc)
{
        if (!doc) {
                printf("CSV Document is NULL.\n");
                return;
        }
        printf("--- CSV Info ---\n");
        printf("Rows:    %d\n", doc->num_rows);
        printf("Columns: %d\n", doc->num_cols);
        printf("Header:  %s\n", doc->header ? "Yes" : "No");
        printf("----------------\n");
}

#endif // CSVIEW_IMPLEMENTATION
#endif // CSVIEW_H
