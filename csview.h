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
*
* Author:  Alfred Jijo
*
* =====================================================================================
*/

// ---------------------------------------------------------
// Macros
// ---------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
	/**
	 * @brief On GCC/Clang, this macro uses a cleanup attribute to automatically
	 * call csv_free() when the variable goes out of scope.
	 * Usage: autofree_csv csv_document_t* my_doc = csv_read(...);
	 */
	#define autofree_csv __attribute__((cleanup(csv_free)))
#else 
	#define autofree_csv
#endif 

// ---------------------------------------------------------
// Data Structures
// ---------------------------------------------------------

/**
 * @brief Represents a single row in a CSV file
 */
typedef struct {
	char **fields;	// Array of strings, where each string is a field in the row.
	int num_fields; // The number of fields in this row.
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

// ---------------------------------------------------------
// Function Prototypes
// ---------------------------------------------------------

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
