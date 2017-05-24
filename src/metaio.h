#ifndef _METAIO_H_
#define _METAIO_H_
/*
 * This code implements a simple recursive-descent parsing scheme for
 * LIGO_LW XML files, based on the example in Chapter 2 of
 * "Compilers: Principles, Techniques and Tools" by Aho, Sethi and Ullman.
 *
 * For historical reasons, the code contains several different ways of
 * reading data from LIGO_LW files, some of which overlap with or supersede
 * earlier implementations. Mixing of the methods is not recommended.
 *
 * Method 1 (preferred):
 *   1) Use MetaioOpenFile() to prepare the XML file for reading and initialise
 *   the parsing environment
 *   2) Use MetaioOpenTableOnly() to seek to a specific table (or use a null
 *   table name to seek to the first table in the file)
 *   3) Use MetaioGetRow() to read individual rows from the table
 *   4) Use MetaioClose() to parse to the end of the file and clean up, or
 *   MetaioAbort() to clean up immediately without parsing to the end of the
 *   file eg.
 *
 *     struct MetaioParseEnvironment parseEnvironment;
 *     MetaioParseEnv const env = &parseEnvironment;
 *     const char* const filename = "ligo_lw.xml";
 *     const char* const tablename = "row";
 *
 *     if ((ret = MetaioOpenFile(env, filename)) != 0)
 *     {
 * 	 fprintf(stderr, "File not found\n");
 *       exit(1);
 *     }
 * 
 *     if ((ret = MetaioOpenTableOnly(env, tablename)) != 0)
 *     {
 *       fprintf(stderr, "Table not found\n");
 *       exit(1);
 *     }
 * 
 *     while ((ret = MetaioGetRow(env)) > 0)
 *     {
 *          ... process rows ...
 *     }
 * 
 *     if (ret < 0)
 *     {
 *       fprintf(stderr, "Error parsing rows\n");
 *       exit(1);
 *     }
 *        
 *     ret = MetaioClose(env);
 * 
 *     if (ret < 0)
 *     {
 *       fprintf(stderr, "Error parsing rest of file\n");
 *       exit(1);
 *     }
 *
 * Method 2:
 *   1) Use MetaioOpenTable() to prepare the XML file for reading, initialise
 *   the parsing environment and seek to a specific table (or use a null
 *   table name to seek to the first table in the file)
 *   2) Use MetaioGetRow() to read individual rows from the table
 *   3) Use MetaioClose() to parse to the end of the file and clean up, or
 *   MetaioAbort() to clean up immediately without parsing to the end of the
 *   file.
 *
 * Method 3 (original, now deprecated):
 *   1) Use MetaioOpen() to prepare the XML file for reading, initialise
 *   the parsing environment and seek to the first table in the file.
 *   2) Use MetaioGetRow() to read individual rows from the table
 *   3) Use MetaioClose() to parse to the end of the file and clean up, or
 *   MetaioAbort() to clean up immediately without parsing to the end of the
 *   file.
 */

#include <complex.h>
#include <setjmp.h>

typedef int METAIO_INT_4S;
typedef unsigned int METAIO_INT_4U;
typedef short METAIO_INT_2S;
typedef unsigned short METAIO_INT_2U;
typedef long long METAIO_INT_8S;
typedef unsigned long long METAIO_INT_8U;
typedef float METAIO_REAL_4;
typedef double METAIO_REAL_8;
typedef char  METAIO_CHAR;
typedef unsigned char METAIO_CHAR_U;
typedef float complex METAIO_COMPLEX_8;
typedef double complex METAIO_COMPLEX_16;

/*-- This must be kept consistent with "const TypeText" in metaio.c !! --*/
enum METAIO_Type {
    METAIO_TYPE_ILWD_CHAR,
    METAIO_TYPE_ILWD_CHAR_U,
    METAIO_TYPE_INT_4S,
    METAIO_TYPE_INT_4U,
    METAIO_TYPE_INT_2S,
    METAIO_TYPE_INT_2U,
    METAIO_TYPE_INT_8S,
    METAIO_TYPE_INT_8U,
    METAIO_TYPE_LSTRING,
    METAIO_TYPE_REAL_4,
    METAIO_TYPE_REAL_8,
    METAIO_TYPE_CHAR_S,
    METAIO_TYPE_CHAR_V,
    METAIO_TYPE_BLOB,
    METAIO_TYPE_COMPLEX_8,
    METAIO_TYPE_COMPLEX_16,
    METAIO_TYPE_UNKNOWN
};

/* A structure to hold variable-sized C-style strings */
struct MetaioString {
    METAIO_CHAR* data;      /* A pointer to the data, null terminated */
    size_t       len;       /* The length of the string */
    size_t       datasize;  /* The length of the memory pointed to by data */
};

/* A structure to hold variable-sized byte strings */
struct MetaioStringU {
    METAIO_CHAR_U* data;    /* A pointer to the data, which may include null */
    size_t         len;     /* The length of the string */
    size_t         datasize;/* The length of the memory pointed to by data */
};

struct MetaioColumn {
    char* name;
    enum METAIO_Type data_type;
};

struct MetaioStream {
    char* name;
    char* type;
    char  delimiter;
};

struct MetaioRowElement {
    struct MetaioColumn* col;
    int valid;
    union MetaioRowEltData {
	METAIO_REAL_4        real_4;
	METAIO_REAL_8        real_8;
	METAIO_INT_4S        int_4s;
	METAIO_INT_4U        int_4u;
	METAIO_INT_2S        int_2s;
	METAIO_INT_2U        int_2u;
	METAIO_INT_8S        int_8s;
	METAIO_INT_8U        int_8u;
	METAIO_COMPLEX_8     complex_8;
	METAIO_COMPLEX_16    complex_16;
	struct MetaioString  lstring;	/* also used for ilwd:char, char_s, char_v */
	struct MetaioStringU blob;	/* also used for ilwd:char_u */
    } data;
};

#define METAIOMAXCOLS 100

struct MetaioTable {
    char*                   name;
    char*                   comment;
    struct MetaioColumn     col[METAIOMAXCOLS];
    struct MetaioRowElement elt[METAIOMAXCOLS];
    int                     numcols;
    int                     maxcols;
    struct MetaioStream     stream;
};

struct MetaioLigo_lw {
    char*        name;
    char*        comment;
    struct MetaioTable table;
};

struct MetaioFileRecord {
    char*  name;
    void*  fp;
    size_t lineno;
    size_t charno;
    int nrows;
    char mode;
    char* tablename;
};

typedef struct MetaioFileRecord* MetaioFile;

struct MetaioParseEnvironment {
    struct MetaioFileRecord fileRec;
    MetaioFile              file;
    struct MetaioString     buffer;
    int                     token;
    int                     mierrno;
    struct MetaioString     mierrmsg;  /* Parsing error messages put here */
    jmp_buf                 jmp_env;
    struct MetaioLigo_lw    ligo_lw;
};

typedef struct MetaioParseEnvironment* MetaioParseEnv;

/*
 * Returns the name of the data type as a string.
 * Usually only useful for debugging.
 */
extern
const char* MetaioTypeText(int data_type);

/*
 * This function creates the file pointer in env and can be called before
 * MetaioOpenTable() which will then fill the env with information about
 * the appropriate table
 *
 * Returns 0 if successful, non-zero otherwise.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 */
extern
int MetaioOpenFile(MetaioParseEnv const env, const char* const filename);

/*
 * This function parses the contents of filename up to the beginning
 * of the first row of data. It fills the parse environment structure
 * 'env' with information describing the format that MetaioGetRow() will
 * expect the rows to be in. It must be called before using MetaioGetRow()
 *
 * Returns 0 if successful, non-zero otherwise.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 *
 * DEPRECATED. Use MetaioOpenFile and MetaioOpenTableOnly instead.
 */
extern
int MetaioOpen(MetaioParseEnv const env, const char* const filename);

/*
 * This function is like MetaioOpen, but it allows the user to specify the
 * name of the table to be read from a multi-table file.  If a blank table name
 * is specified, or tablename is a null pointer, it reads the first table from
 * the file (i.e. equivalent to MetaioOpen).  The table name is
 * case-insensitive.  An error occurs if the specified table does not exist
 * in the file.
 *
 * Within the xml file, the table NAME attribute take the form of a series of
 * colon-delimited strings, optionally terminated with the tag ":table". This
 * function seeks to the first table in the file where tablename matches the
 * second-last string (if :table is present) or the last string (if :table is
 * not present). In either case, the matching is case-insensitive. Any other
 * leading colon-delimited strings in the the NAME attribute are ignored.
 *
 * Example: To read in rows from
 *
 *    <Table Name="ldasgroup:row:table">
 *      ...
 *    </Table>
 *
 * tablename should be set to "row".
 *
 * Returns 0 if successful, non-zero otherwise.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 *
 * DEPRECATED. Use MetaioOpenFile and MetaioOpenTableOnly instead.
 */
extern
int MetaioOpenTable(MetaioParseEnv const env, const char* const filename,
		    const char* const tablename);

/*
 * This function is like MetaioOpenTable, but it requires that the
 * xml file is already open.  An error occurs if the
 * specified table does not exist in the file or the file pointer is
 * null.
 *
 * Within the xml file, the table NAME attribute take the form of a series of
 * colon-delimited strings, optionally terminated with the tag ":table". This
 * function seeks to the first table in the file where tablename matches the
 * second-last string (if :table is present) or the last string (if :table is
 * not present). In either case, the matching is case-insensitive. Any other
 * leading colon-delimited strings in the the NAME attribute are ignored.
 *
 * If tablename is a null pointer or points to an empty string, the tablename
 * is taken to match the first table in the file.
 *
 * Example: To read in rows from
 *
 *    <Table Name="ldasgroup:row:table">
 *      ...
 *    </Table>
 *
 * tablename should be set to "row".
 *
 * Returns 0 if successful, non-zero otherwise.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 */
extern
int MetaioOpenTableOnly(MetaioParseEnv const env, const char* const tablename);

/*
 * Parse the next row in the input file and insert it into 'env',
 * overwriting the current row. Row element i can be accessed
 * via env->ligo_lw.table.elt[i].
 *
 * Returns 1 if a row was obtained, 0 if no row was obtained and
 * a negative number if an error was encountered.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 */
extern
int MetaioGetRow(MetaioParseEnv const env);

/*
 * Finish off parsing the file (looking for closing tags and so on), close
 * the file and free resources owned by 'env'. After calling this, accessing
 * any part of 'env' is undefined.
 *
 * Returns 0 if successful, or a negative number if an error was encountered.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 */
extern
int MetaioClose(MetaioParseEnv const env);

/*
 * Immediately close the file, without trying to parse the rest of it,
 * and free resources owned by 'env'. After calling this, accessing
 * any part of 'env' is undefined.
 *
 * Returns 0 if successful, or a negative number if an error was encountered.
 *
 * In case of an error, an error message is returned in env->mierrmsg.
 * The line number in and character positions in the XML file where the
 * error occurred is returned in env->file->lineno and env->file->charno.
 */
extern
int MetaioAbort(MetaioParseEnv const env);

/*
 * Clear/reset the internal error code.
 */
extern
void MetaioClearErrno(MetaioParseEnv const env);

/*
 * Retrieve the most recent error message.  The return value is a pointer
 * to a null-terminated string or NULL if there has been no error or the
 * cause of the most recent error is not known.
 */
extern
const char *MetaioGetErrorMessage(MetaioParseEnv const env);

/*
 * Look up a the name of the column with the given index, ignoring any "prefix"
 * string(s) delimited by colons.  For instance, if the file contains a column
 * named "processgroup:process:program", this routine returns a pointer to the
 * first letter of "program".
 *
 * Returns a pointer to the column name, or 0 if the column number is invalid.
 */
extern
char *MetaioColumnName(const MetaioParseEnv env, int icol);

/*
 * Find a column by name.  The name comparison is case-insensitive and ignores
 * any "prefix" strings, delimited by colons, in the column name in the file.
 *
 * Returns the index of the column with the specified name, or -1 if no such
 * column is found.
 */
extern
int MetaioFindColumn(const MetaioParseEnv env, const char *name);

/*
 * Compares element values in a way which depends on the particular data type.
 *
 * Returns 0 if the values are equal; returns -1 if the first value is "less"
 * than the second value; returns 1 if the first value is "greater" than the
 * second value.
 */
extern
int MetaioCompareElements(struct MetaioRowElement *elt1,
                          struct MetaioRowElement *elt2);

/*
 * Print the value of a row element to the given file (which may be a standard
 * stream such as stdout, stderr) as a string.
 * 
 * Returns the number of characters printed.
 */
extern
int MetaioFprintElement(FILE *f, const struct MetaioRowElement *elt);

/*
 * Opens a file for writing, and writes the LIGO_LW header.
 * Returns 0 if successful, nonzero if there was an error creating the file.
 */
extern
int MetaioCreate(const MetaioParseEnv env, const char* const filename);

/*
 * Copies column definitions, etc., from one metaio environment to another.
 * Returns 0 if successful, nonzero if there was an error.
 */
extern
int MetaioCopyEnv(const MetaioParseEnv dest, const MetaioParseEnv source);

/*
 * Copies row contents from one metaio stream to another.
 * Returns 0 if successful, nonzero if there was an error.
 */
extern
int MetaioCopyRow(const MetaioParseEnv dest, const MetaioParseEnv source);

/*
 * Writes out the current row.
 * Returns 0 if successful, nonzero if there was an error.
 */
extern
int MetaioPutRow(const MetaioParseEnv env);

#endif /* _METAIO_H_ */
