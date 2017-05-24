/*
 * This code implements a simple recursive-descent parsing scheme for
 * LIGO_LW XML files, based on the example in Chapter 2 of
 * "Compilers: Principles, Techniques and Tools" by Aho, Sethi and Ullman.
 *
 * The production rules for the parser are:
 *
 * ligo_lw  ->  <LIGO_LW ligo_lw_attr ligo_lw_body /LIGO_LW > END_OF_FILE
 *   ligo_lw_attr  ->  name >
 *     name  ->  NAME = "string_literal"
 *   ligo_lw_body  ->  [ comment ] table table table ...
 *     comment  ->  <COMMENT > string_literal </COMMENT >
 *     table  ->  <TABLE table_attr table_body </TABLE >
 *       table_attr  ->  name >
 *       table_body  ->  [ comment ] column column ... stream
 *         column  ->  <COLUMN column_attr / >
 *           column_attr  ->  name type >
 *             type  ->  TYPE = "type_name"
 *         stream  ->  <STREAM stream_attr row row ... /STREAM >
 *           stream_attr  ->  name type delimiter >
 *             delimiter  ->  DELIMITER = "delimiter_char"
 *           row  ->  row_element delimiter_char row_element delimiter_char ...
 *
 * The capitalized words and the symbols >, = and / are all individual
 * tokens (a full list is provided in TokenText below).
 *
 * Note that the parsing of a file is "halted" at the greater-than symbol (>)
 * at the end of the stream_attr rule ie. at the start of the first row of
 * the table. At this point control over parsing the rows is handed back to
 * the user. Individual rows are read parsed one at a time using
 * MetaioGetRow().
 *
 * After all rows are read in, MetaioClose() is used to parse the remainder
 * of the file, although this mainly serves to verify that the rest of the
 * file has the correct syntax.
 */

#include <complex.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "config.h"
#include "metaio.h"
#include "base64.h"
#include "ligo_lw_header.h"

#ifdef  DEBUG
#define DEBUGMSG1(s)    printf(s##"\n")
#define DEBUGMSG2(s,d)  printf(s, (d))
#else	/* DEBUG */
#define DEBUGMSG1(s)
#define DEBUGMSG2(s,d)
#endif	/* DEBUG */

#ifdef HAVE_LIBZ
#include <zlib.h>

#define gzclose(fp) gzclose((gzFile)(fp))
#define gzgetc(fp) gzgetc((gzFile)(fp))
#define gzungetc(c,fp) gzungetc(c,(gzFile)(fp))

/*
 * Warning: This is not a fully implemented scanf routine!
 * Warning: Only a single conversion to a numeric type is supported!
 */

static int gzvscanf( gzFile file, const char *fmt, va_list ap )
{
    static const char terminators[] = {',', '\n', '\\', '\"', '\0'};
    char buf[4096];
    size_t i;
    for ( i = 0; i < sizeof(buf); i++ ) {
        int c = gzgetc( file );
        if ( c == -1 )
        {
            /* error */
#if 0
            /* how to extract error message */
            int errnum;
            const char *msg = gzerror( file, &errnum );
#endif
            return EOF;	/* like C library's vscanf() */
        }
        if ( strchr( terminators, c ) )
        {
            /* end of field */
            gzungetc( c, file );
            buf[i] = '\0';
            return vsscanf( buf, fmt, ap );
        }
        /* append and continue */
        buf[i] = c;
    }
    /* buffer too small */
    return EOF;
}

static int gzscanf( gzFile file, const char *fmt, ... )
{
    int c;
    va_list ap;
    va_start( ap, fmt );
    c = gzvscanf( file, fmt, ap );
    va_end( ap );
    return c;
}

static int geterrno( gzFile file, const char **msg )
{
    int errnum;

    /* retrieve error and message from zlib */
    *msg = gzerror(file, &errnum);
    if(errnum == Z_OK)
    {
        /* no error */
        *msg = NULL;
        if(gzeof(file))
            /* failure was EOF */
            /* EOF probably doesn't overlap with any errno constants */
            errnum = EOF;
        else
            errnum = 0;
    }
    else if(errnum == Z_STREAM_END)
    {
        /* failure was EOF */
        *msg = NULL;
        /* EOF probably doesn't overlap with any errno constants */
        errnum = EOF;
    }
    else if(errnum == Z_ERRNO)
    {
        /* error came from underlying file operations, not zlib */
        if(gzeof(file))
        {
            /* failure was EOF */
            *msg = NULL;
            /* EOF probably doesn't overlap with any errno constants */
            errnum = EOF;
        }
	else
        {
           /* failure was not EOF, use errno and retrieve appropriate
             * message */
            *msg = strerror(errno);
            errnum = errno;
        }
    }
    return errnum;
}

#else	/* HAVE_LIBZ */

#define gzopen(path,mode) fopen(path,mode)
#define gzclose(fp) fclose(fp)
#define gzgetc(fp) fgetc(fp)
#define gzungetc(c,fp) ungetc(c,fp)
#define gzscanf fscanf

static int geterrno( FILE *file, const char **msg )
{
    int errnum;
    if(ferror(file))
    {
        /* there was an error, use errno and retrieve appropriate message */
        *msg = strerror(errno);
        errnum = errno;
    }
    else
    {
        /* no error */
        *msg = NULL;
        if(feof(file))
            /* EOF probably doesn't overlap with any errno constants */
            errnum = EOF;
        else
            errnum = 0;
    }
    return errnum;
}

#endif	/* HAVE_LIBZ */


enum Token {
    CLOSE_COLUMN,
    CLOSE_COMMENT,
    CLOSE_LIGO_LW,
    CLOSE_STREAM,
    CLOSE_TABLE,
    COLUMN,
    COMMENT,
    DELIMITER,
    LIGO_LW,
    NAME,
    STREAM,
    TABLE,
    TYPE,
    GREATER_THAN,
    EQUALS,
    FORWARD_SLASH,
    UNKNOWN,        /* UNKNOWN must always be second-last */
    END_OF_FILE     /* END_OF_FILE must always be last */
};

static
const char* const TokenText[UNKNOWN] = {
    "</Column",
    "</Comment",
    "</LIGO_LW",
    "</Stream",
    "</Table",
    "<Column",
    "<Comment",
    "Delimiter",
    "<LIGO_LW",
    "Name",
    "<Stream",
    "<Table",
    "Type",
    ">",
    "=",
    "/"             /* There is no token text for UNKNOWN or END_OF_FILE */
};

/*
 * This must be kept consistent with "enum METAIO_Type" in metaio.h!!
 *
 * FIXME:  the time types "GPS", "Unix", and "ISO-8601" are not yet
 * supported.
 */

static const struct {
    const char * const name[3];
} TypeText[METAIO_TYPE_UNKNOWN] = {
    {{"ilwd:char", "char", NULL}},
    {{"ilwd:char_u", "char_u", NULL}},
    {{"int_4s", "int", NULL}},
    {{"int_4u", NULL}},
    {{"int_2s", "short", NULL}},
    {{"int_2u", NULL}},
    {{"int_8s", "long", NULL}},
    {{"int_8u", NULL}},
    {{"lstring", "string", NULL}},
    {{"real_4", "float", NULL}},
    {{"real_8", "double", NULL}},
    {{"char_s", NULL}},
    {{"char_v", NULL}},
    {{"blob", NULL}},
    {{"complex_8", NULL}},
    {{"complex_16", NULL}}
};

/*
 * Resize a string
 */

static
int string_resize(struct MetaioString * const str, size_t len)
{
    size_t ResizeIncrement = 512;
    size_t new_datasize = ((len+1)/ResizeIncrement + 1)*ResizeIncrement;

    if (new_datasize != str->datasize)
    {
        void *new = realloc(str->data, new_datasize * sizeof(*str->data));
        if(!new)
            return -1;
        str->data = new;
        str->datasize = new_datasize;
    }
    return 0;
}

/*
 * Append a character to the end of a string
 */

static
int append_char(struct MetaioString * const str, int c)
{
    size_t new_len = str->len + 1;

    if (new_len >= str->datasize)
        if(string_resize(str, new_len) < 0)
            return -1;

    str->len = new_len;
    str->data[str->len - 1] = c;
    str->data[str->len] = '\0';
    return 0;
}

/*
 * Append a null-terminated C-style string to a MetaioString
 */

static
int append_cstr(struct MetaioString * const str, const char* const s)
{
    size_t s_len = strlen(s);
    size_t new_len = str->len + s_len;

    if (new_len >= str->datasize)
        if(string_resize(str, new_len) < 0)
            return -1;

    str->len = new_len;
    strcpy(&str->data[str->len - s_len], s);
    return 0;
}

/*
 * Assign a null-terminated C-style string to a char*, assuming the
 * original char* was dynamically allocated. This is to try and avoid
 * memory leaks.
 */

static
char *assign_cstr(char **dest, const char * const src)
{
    free(*dest);
    *dest = strdup(src);
    return *dest;
}

/*
 * Resize a MetaioStringU (ie. a string of unsigned characters)
 */

static
int stringu_resize(struct MetaioStringU * const str, size_t len)
{
    size_t ResizeIncrement = 512;
    size_t new_datasize = ((len+1)/ResizeIncrement + 1)*ResizeIncrement;

    if (new_datasize != str->datasize)
    {
        void *new = realloc(str->data, new_datasize * sizeof(*str->data));
        if(!new)
            return -1;
        str->data = new;
        str->datasize = new_datasize;
    }
    return 0;
}

/*
 * Append an unsigned int to a MetaioStringU
 */

static
int append_char_u(struct MetaioStringU * const str, unsigned int c)
{
    size_t new_len = str->len + 1;

    if (new_len >= str->datasize)
        if(stringu_resize(str, new_len) < 0)
            return -1;

    str->len = new_len;
    str->data[str->len - 1] = c;
    str->data[str->len] = '\0';
    return 0;
}

/*
 * This function is called when an error in parsing is encountered,
 * generally due to a syntax error within the file being processed.
 */

static
void parse_error(MetaioParseEnv const env, int code,
                 const char* const format, ...)
{
    char errbuf[257];
    va_list args;

    if (code == 0)
    {
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fputs("code == 0, should be non-zero\n", stderr);
        exit(1);
    }

    snprintf(errbuf, sizeof(errbuf) / sizeof(*errbuf) - 1, "error in %s line %d near position %d: ", env->file->name, (int)env->file->lineno, (int)env->file->charno);
    
    append_cstr(&(env->mierrmsg), errbuf);

    va_start(args, format);
    vsnprintf(errbuf, sizeof(errbuf) / sizeof(*errbuf) - 1, format, args);
    va_end(args);
    
    append_cstr(&(env->mierrmsg), errbuf);

    env->mierrno = code;
    
    DEBUGMSG2("Buffer is <%s>\n", env->buffer.data);

    longjmp(env->jmp_env, code);
}

/*
 * Clear/reset internal errno code.
 */

void MetaioClearErrno(MetaioParseEnv const env)
{
    env->mierrno = 0;
}

/*
 * Retrieve the most recent error message.  The return value is a pointer
 * to a null-terminated string or NULL if there has been no error or the
 * cause of the most recent error is not known.
 */

const char *MetaioGetErrorMessage(MetaioParseEnv const env)
{
    return env->mierrmsg.data;
}

/*
 * Initialize the environment structure.
 *
 * The environment structure is used to pass certain data values
 * between functions in the parser. This is done in preference to
 * using global variables to a) limit access to global scope and b) so the
 * parser could conceivably be used in threaded environments.
 */

static
int init_parse_env(MetaioParseEnv const env, const char* const filename,
                   const char* mode )
{
    int i = 0;

    /* Error handling must be initialized first */
    MetaioClearErrno(env);
    env->mierrmsg.data = 0;
    env->mierrmsg.len = 0;
    env->mierrmsg.datasize = 0;

    env->file = &(env->fileRec);
    env->file->name = strdup(filename);
    env->file->fp = 0;
    env->file->lineno = 1;
    env->file->charno = 1;
    env->file->nrows = 0;
    env->file->mode = mode[0];
    env->file->tablename = NULL;

    env->token = UNKNOWN;

    env->buffer.data = 0;
    env->buffer.len = 0;
    env->buffer.datasize = 0;

    env->ligo_lw.name = 0;
    env->ligo_lw.comment = 0;

    env->ligo_lw.table.name = 0;
    env->ligo_lw.table.comment = 0;
    env->ligo_lw.table.maxcols = METAIOMAXCOLS;
    env->ligo_lw.table.numcols = 0;
    
    for (i = 0; i < METAIOMAXCOLS; ++i)
    {
        env->ligo_lw.table.col[i].name = 0;
        env->ligo_lw.table.col[i].data_type = METAIO_TYPE_UNKNOWN;
    }

    env->ligo_lw.table.stream.name = 0;
    env->ligo_lw.table.stream.type = 0;
    env->ligo_lw.table.stream.delimiter = '\0';

    /*-- Now try to open the file --*/
    switch ( mode[0] )
    {
    case 'r':
        if (!(env->file->fp = gzopen(filename, "r")))
        {
            const char *msg;
            geterrno(env->file->fp, &msg);
            parse_error(env, -1, "cannot open \"%s\": %s", filename, msg);
            return 1;
        }
        break;

    case 'w':
        if (!(env->file->fp = fopen(filename, "w")))
            return 1;
        break;

    default:
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fprintf(stderr, "mode[0] = \'%c\', should be either r or w\n", mode[0]);
        exit(1);
    }

    env->file->nrows = 0;

    return 0;
}

/*
 * Destroy the parse environment. Should usually only be called when
 * parsing is completed.
 *
 * FIXME:  any memory that might have been allocated for an error message
 * in the mierrmsg element is leaked.  the memory needs to be left intact
 * so that errors that occur in this function can be reported by the
 * calling code.  the amount of leaked memory is typically a few hundred
 * bytes per parsing pass (i.e., per table), and so in a long-running
 * program that parses, say, 10 tables from each of 1000 files, several
 * megabytes will be lost.  valgrind will report this as memory lost via a
 * call to string_resize().
 */

static
int destroy_parse_env(MetaioParseEnv const env)
{
    int ret = 0;
    int i = 0;

    for (i = 0; i < env->ligo_lw.table.numcols; i++)
    {
        /* Delete the row data */
        enum METAIO_Type type = env->ligo_lw.table.elt[i].col->data_type;

        if(type == METAIO_TYPE_LSTRING || type == METAIO_TYPE_ILWD_CHAR || type == METAIO_TYPE_CHAR_S || type == METAIO_TYPE_CHAR_V)
        {
            free(env->ligo_lw.table.elt[i].data.lstring.data);
            env->ligo_lw.table.elt[i].data.lstring.data = 0;
            env->ligo_lw.table.elt[i].data.lstring.len = 0;
            env->ligo_lw.table.elt[i].data.lstring.datasize = 0;
        }
        else if(type == METAIO_TYPE_BLOB || type == METAIO_TYPE_ILWD_CHAR_U)
        {
            free(env->ligo_lw.table.elt[i].data.blob.data);
            env->ligo_lw.table.elt[i].data.blob.data = 0;
            env->ligo_lw.table.elt[i].data.blob.len = 0;
            env->ligo_lw.table.elt[i].data.blob.datasize = 0;
        }
        
        env->ligo_lw.table.elt[i].col = 0;

        /* Delete the column data */
        free(env->ligo_lw.table.col[i].name);
        env->ligo_lw.table.col[i].name = 0;
    }

    /* Delete the stream */
    free(env->ligo_lw.table.stream.name);
    env->ligo_lw.table.stream.name = 0;

    free(env->ligo_lw.table.stream.type);
    env->ligo_lw.table.stream.type = 0;

    /* Delete the table */
    free(env->ligo_lw.table.name);
    env->ligo_lw.table.name = 0;

    free(env->ligo_lw.table.comment);
    env->ligo_lw.table.comment = 0;

    /* Delete the ligo_lw */
    free(env->ligo_lw.name);
    env->ligo_lw.name = 0;

    free(env->ligo_lw.comment);
    env->ligo_lw.comment = 0;

    /* Delete the buffer */
    if (env->buffer.data != 0 && env->buffer.datasize != 0)
    {
        free(env->buffer.data);
        env->buffer.data = 0;
        env->buffer.len = 0;
        env->buffer.datasize = 0;
    }

    /* Delete the file */
    free(env->file->name);
    env->file->name = 0;

    free(env->file->tablename);
    env->file->tablename = 0;

    if (env->file->fp != 0)
    {
        if ( env->file->mode == 'r' )
        {
            ret = gzclose(env->file->fp);
            if ( ret == -3 )
                /* XXX hack put in due to bug in zlib 1.2.2       XXX */
                /* XXX this is not necessary in 1.2.3 and should  XXX */
                /* XXX be removed when we no longer support FC4   XXX */
                ret = 0;
        }
        else if ( env->file->mode == 'w' )
            ret = fclose(env->file->fp);
        else
        {
            fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
            fprintf(stderr, "mode = \'%c\', should be either r or w\n", env->file->mode);
            exit(1);
        }
        env->file->fp = 0;
    }

    env->file->nrows = 0;

    /* previous parse errors take precedent */
    return env->mierrno ? env->mierrno : ret;
}

/*
 * Returns 1 if c is a valid token character. Allowed token characters are
 * all alphanumerics, and the characters '_' and ':'.
 */

static
int is_token_char(int c)
{
    return isalnum(c) || (c == '_') || (c == ':');
}

/*
 * Return the single character corresponding to a character entity string.
 * Only the leading characters are compared, for example "&lt;blahblah"
 * will be found to match "&lt;".  The return value is the translated
 * character or 0 if the string is not recognized.
 */

static
int character_entity(const char* const s)
{
    static const struct entities {
        const char *s;
        int c;
    } entities[] = {
        {"&gt;", '>'},
        {"&lt;", '<'},
        {"&amp;", '&'},
        {"&quot;", '\"'},
        {"&apos;", '\''},
        {"&nbsp;", ' '},
        {NULL, 0}
    };
    const struct entities *entity;

    for(entity = entities; entity->s; entity++)
        if(!strncmp(entity->s, s, strlen(entity->s)))
            return entity->c;

    return 0;
}

/*
 * Get the next character from the stream. This function contains
 * a filter for fgetc to guarantee we always count lines correctly
 */

static
int get_char(MetaioParseEnv env)
{
    int c = gzgetc(env->file->fp);

    if(c < 0)
    {
        const char *msg;
        int errnum = geterrno(env->file->fp, &msg);
        if(!msg && (errnum == EOF))
            return EOF;
        parse_error(env, -1, msg ? msg : "unknown failure");
    }

    if (c == '\n')
    {
        env->file->lineno++;
        env->file->charno = 1;
    }
    else
        env->file->charno++;

    return c;
}

/*
 * Push a character back onto the stream. This function contains
 * a filter for ungetc to guarantee we always count lines correctly
 */

static
int unget_char(MetaioParseEnv env, int c)
{
    if(c < 0)
        /* get_char() returned error --> us too */
	return c;

    env->file->charno--;
    if (c == '\n')
        env->file->lineno--;

    return gzungetc(c, env->file->fp);
}

/*
 * Return the next non-whitespace character.
 */

static
int skip_whitespace(MetaioParseEnv env)
{
    int c;

    do
        c = get_char(env);
    while(isspace(c));

    return c;
}

/*
 * This function acts like fgets(), placing the next lstring from the file
 * into s.  Returns the number of characters written to the string.
 */

static int fscanf_lstring(
    MetaioParseEnv env,
    struct MetaioString *s,
    char terminator
)
{
    char escape_character = '\\';
    /* NOTE:  terminator must be listed last because it can be 0 */
    /* FIXME:  (9.x) remove delimiter, only listed for temporary backwards
     * compatibility with pre-8.x documents */
    char escapables[] = {escape_character, env->ligo_lw.table.stream.delimiter, terminator, '\0'};
    size_t start_len = s->len;
    int is_character_entity = 0;
    int is_escaped = 0;
    int start_of_character_entity = -1;

    /* Make sure we start with at least a zero-length string */
    string_resize(s, 1);
    s->data[0] = '\0';

    /* Retrieve the string upto but not including an unescaped terminator */
    do
    {
        /* Get a character */
        int c = get_char(env);
        if(c < 0)
            parse_error(env, -1, "failure reading lstring:  premature EOF");

        /* Start of an XML tag --> end of string */
        if(c == '<')
        {
            /* Unconsume the start-of-tag */
            unget_char(env, c);
            break;
        }

        /* Are we starting a character entity? */
        if((c == '&') && !is_character_entity)
        {
            is_character_entity = 1;
            start_of_character_entity = s->len;
        }

        /* Have we just ended a character entity? */
        if((c == ';') && is_character_entity)
        {
            is_character_entity = 0;
            append_char(s, c);
            c = character_entity(&s->data[start_of_character_entity]);
            if(!c)
                /* Not recognized */
                parse_error(env, -1, "unrecognized character entity \"%s\"", &s->data[start_of_character_entity]);
            /* Recognized, unwind to start of character entity */
            s->len = start_of_character_entity;
            s->data[s->len] = '\0';
        }

        /* End of string? */
        if((c == terminator) && !is_escaped)
        {
            /* Unconsume the terminator */
            unget_char(env, c);
            break;
        }

        /* Escaped? */
        if(is_escaped && !is_character_entity)
        {
            if(!strchr(escapables, c))
		/* Escaped character is not in the list of characters that
                 * can be escaped */
                parse_error(env, -1, "unrecognized escape sequence \"%hhc%c\"", escape_character, c);
            /* Strip preceding backslash */
            s->len--;
            s->data[s->len] = '\0';
            /* Append character to string */
            append_char(s, c);
        }
        else
            /* Append character to string */
            append_char(s, c);

        /* Is the next character escaped? */
        is_escaped = is_escaped ? is_character_entity : c == escape_character;
    }
    while(1);

    /* Done */
    return s->len - start_len;
}

/*
 * This function acts like fread(), placing the next decoded blob from the
 * file into s.  Returns the number of octets in the decoded blob or
 * < 0 on error.  The input buffer is both overwritten.
 */

static int fscanf_blob(
    MetaioParseEnv env,
    struct MetaioStringU *s,
    char terminator
)
{
    struct MetaioString b64 = {
        .data = calloc(1, sizeof(*b64.data)),
        .len = 0,
        .datasize = 1
    };
    int ok;

    if(!b64.data)
        /* Memory error */
        return -1;

    /* Retrieve the string upto but not including a terminator */
    do
    {
        /* Get a character */
        int c = get_char(env);
        if(c < 0)
            parse_error(env, -1, "failure reading blob:  premature EOF");

        /* Start of an XML tag or terminator --> end of string */
        if(c == '<' || c == terminator)
        {
            /* Unconsume the terminating character */
            unget_char(env, c);
            break;
        }

        /* Append character to string */
        append_char(&b64, c);
    }
    while(1);

    /* Decode base64 data.  Formula for required length of decoded buffer
     * copied from base64.c */
    stringu_resize(s, 3 * (b64.len / 4) + 2);
    ok = base64_decode(b64.data, b64.len, (char *) s->data, &s->datasize);
    s->len = s->datasize;
    free(b64.data);
    if(!ok)
        /* Decode error */
        return -1;

    /* Done */
    return s->len;
}

/*
 * Scan the file for a ilwd:char_u string until the quote character is
 * reached.  The quote character us not consumed. An ilwd:char staring
 * consists of a combination of the following:
 *
 * - Unsigned bytes represented by 3 octal digits eg. \123
 *
 * - Non-space character eg. xyz
 *
 * - Backslash-escaped characters eg. backslash \\, space \ , comma \,
 *   and quotes \" or \'.
 *
 * The string may be any combination of the above. Spaces are treated as
 * separators between two bytes and are ignored, unless escaped using a
 * backslash. eg. '\057 a\ b  \"x' would (in ASCII) be translated to 'Wa b"x'.
 *
 * Like scanf(), this function returns the number of objects assigned,
 * which in this case means the number of Byte objects, *not* the number
 * of bytes - that value is contained in the 'len' field of the Byte object.
 *
 * If one or more valid bytes is read, 1 is returned.
 *
 * If no valid bytes are read, 0 is returned.
 * 
 * If EOF is reached before any valid bytes are read, EOF is returned.
 *
 * :NOTE: 
 * 
 * A slight flaw in this code is that it will accept octal
 * strings that are shorter than 3 characters eg. \10.
 */

static
int fscanf_ilwd_char_u(
    MetaioParseEnv env,
    struct MetaioStringU *b,
    const char *terminators,
    const char *specials
)
{
    int val;
    int count = 1;
    int ret   = 0;

    b->len = 0;

    /*-- Get first character which is not a space --*/
    val = skip_whitespace(env);
    if(val < 0)
        parse_error(env, -1, "failure reading ilwd_char_u:  premature EOF");

    while ((count == 1) && (val <= 256) && ! strchr(terminators, val) && (val != EOF))
    {
        if (val == '&')
        {
            char tmp[8] = {val, 0, };
            int i;
            
            for(i = 1; i < sizeof(tmp) - 1;)
            {
                int c = get_char(env);
                if( c < 0 )
                    parse_error(env, -1, "failure reading ilwd_char_u:  premature EOF");
                tmp[i] = c;
                if(tmp[i++] == ';')
                {
                    tmp[i] = 0;
                    break;
                }
            }

            /* Identify the character entity */
            val = character_entity(tmp);
        }
        else if (val == '\\')
        {
            val = get_char(env);
            if ( val < 0 )
                parse_error(env, -1, "failure reading ilwd_char_u:  premature EOF");
            if (isdigit(val))
            {
                /*
                  We got something of the form \[digit][digit]...
                  read it as an octal.
                */
                unget_char(env, (int) val);
                count = gzscanf(env->file->fp, "%3o", &val);
                if ( count < 0 )
                    parse_error(env, -1, "failure parsing octal code");
            }
            else if ( strchr(specials, val) )
                /* We got a properly-escaped character (such as space) */
                count = 1;            
            else
                /*-- This character should not have been escaped --*/
                parse_error(env, -1, "invalid escape sequence \"\\%c\"", val);
        }
        append_char_u(b, val);

        /*-- Get next character, skipping any spaces. --*/
        val = skip_whitespace(env);
        if(val < 0)
            parse_error(env, -1, "failure reading ilwd_char_u:  premature EOF");
    }
    
    /* Unget the character that caused the loop to finish */
    unget_char(env, (int) val);

    /*
      A val of > 256 means that a valid octal string or character was read
      but it was too large to fit in a byte. This invalidates the whole
      string, so we abandon any data we got.
    */
    
    if (val > 256)
    {
        b->len = 0;
        ret = 0;
    }
    else 
    {   
        if (b->len == 0)
            ret = count == EOF ? EOF : 0;
        else
            ret = 1;
    }

    return ret;
}

/*
 * See if the current table name (stored in env->ligo_lw.table.name) matches
 * the table which is being searched for (stored in env->file->tablename).
 *
 * The two are deemed to match if any of the following conditions are true:
 * 1) tablename is a null pointer
 * 2) tablename points to the null string
 * 3) tablename exactly matches the last characters of ligo_lw.table.name
 * 4) tablename exactly matches the last characters of ligo_lw.table.name
 *    before the terminating string ":table"
 *
 * Eg. if tablename = "foo" then it matches ligo_lw.table.name values such as
 *   - ligo_lw.table.name = "foo" or "foo:table"
 *   - ligo_lw.table.name = "bar:foo" or "bar:foo:table"
 * etc.
 *
 * Returns 0 if the table names match, non-zero otherwise
 */

static
int match_tablename(MetaioParseEnv const env)
{
    int match = 0;

    if (env->file->tablename == 0)
        match = 1;
    else 
    {
        size_t len_tablename = strlen(env->file->tablename);
        if (len_tablename == 0)
            match = 1;
        else
        {
            const char* const name = env->ligo_lw.table.name;
            size_t len = strlen(name);
            if (len >= 6 && !strcasecmp(name+len-6, ":table"))
            {
                if (len >= 6 + len_tablename && !strncasecmp(name+len-6-len_tablename, env->file->tablename, len_tablename))
                    match = 1;
            }
            else
            {
                if (len >= len_tablename && !strncasecmp(name+len-len_tablename, env->file->tablename, len_tablename))
                    match = 1;
            }
        }
    }

    return match;
}

/*
 * See if the string s matches any of the token strings (case-insensitive).
 * If it does, the function returns an integer correspondending to one of
 * the elements of the enumerated type Token (which includes zero), otherwise
 * it returns the integer corresponding to UNKNOWN.
 */

static
enum Token match_token_string(const char* const s)
{
    enum Token i;
    for(i = 0; i < UNKNOWN; i++)
        if(!strcmp(TokenText[i], s))
            break;
    return i;
}

/*
 * See if the string s matches any of the type strings (case-insensitive).
 * If it does, the function returns an integer correspondending to one of
 * the elements of the enumerated type METAIO_Type (which includes zero),
 * otherwise it returns the integer corresponding to METAIO_TYPE_UNKNOWN.
 */

static
enum METAIO_Type match_type_string(const char* const s)
{
    enum METAIO_Type i;

    for(i = 0; i < METAIO_TYPE_UNKNOWN; i++)
    {
        const char * const *name;
        for(name = TypeText[i].name; *name; name++)
            if(!strcasecmp(*name, s))
                return i;
    }
    return METAIO_TYPE_UNKNOWN;
}

/*
 * Returns a pointer to a string containing the name of the token
 */

static
const char* token_text(enum Token token)
{
    if(token >= 0 && token < UNKNOWN)
        return TokenText[token];
    if(token == END_OF_FILE)
        return "END_OF_FILE";
    return "UNKNOWN";
}

/*
 * Returns a pointer to a string containing the name of the type, or NULL
 * if the type is not recognized.
 */

const char *MetaioTypeText(int type)
{
    if(type >= 0 && type < METAIO_TYPE_UNKNOWN)
        return TypeText[type].name[0];
    return NULL;
}

static
int read_token_string(MetaioParseEnv const env, struct MetaioString * const s)
{
    size_t start_len = s->len;
    int c = get_char(env);

    while (is_token_char(c) && (c != EOF))
    {
        append_char(s, c);
        c = get_char(env);
    }

    /* Unconsume the character that caused us to halt */
    unget_char(env, c);

    return s->len - start_len;
}

static
void read_attribute_value(MetaioParseEnv const env, struct MetaioString * const s)
{
    int quote = skip_whitespace(env);
    if(quote < 0)
        parse_error(env, -1, "failure reading attribute value:  premature EOF");

    if(quote == '\"' || quote == '\'')
    {
        if ( fscanf_lstring(env, s, quote) < 0 )
            parse_error(env, -1, "failure parsing attribute value");

        /* Consume the terminating quote */
        if(get_char(env) != quote)
            parse_error(env, -1, "unmatched quote when reading attribute value");
    }
    else
    {
        /* Attributes *must* be quoted. May as well unget what we read. */
        unget_char(env, quote);
        parse_error(env, -1, "missing quote when reading attribute value");
    }
}

static
void get_next_token(MetaioParseEnv const env)
{
    int c = skip_whitespace(env);

    env->buffer.len = 0;
    if(c >= 0)
        append_char(&(env->buffer), c);

    switch(c)
    {
    case '<':
        c = skip_whitespace(env);
        if(c < 0)
            parse_error(env, -1, "failure reading tag:  premature EOF");
        append_char(&(env->buffer), c);
        read_token_string(env, &(env->buffer));
        env->token = match_token_string(env->buffer.data);
        break;
    case '>':
        env->token = GREATER_THAN;
        break;
    case '=':
        env->token = EQUALS;
        break;
    case '/':
        env->token = FORWARD_SLASH;
        break;
    case EOF:
        env->token = END_OF_FILE;
        break;
    default:
        read_token_string(env, &(env->buffer));
        env->token = match_token_string(env->buffer.data);
        break;
    }
}

static
void unexpected_token(MetaioParseEnv const env, enum Token expected)
{
    parse_error(env, -1, "got %s when expecting %s", token_text(env->token), token_text(expected));
}

static
void match(MetaioParseEnv const env, enum Token token)
{
    if (env->token == token)
        get_next_token(env);
    else
        unexpected_token(env, token);
}

static
void name(MetaioParseEnv const env, char **s)
{
    switch(env->token)
    {
    case NAME:
        match(env, NAME);
        env->buffer.len = 0;
        read_attribute_value(env, &(env->buffer));
        assign_cstr(s, env->buffer.data);
        match(env, EQUALS);
        break;
    default:
        assign_cstr(s, "");
        break;
    }
}

static
void type(MetaioParseEnv const env, char **s)
{
    switch(env->token)
    {
    case TYPE:
        match(env, TYPE);
        env->buffer.len = 0;
        read_attribute_value(env, &(env->buffer));
        assign_cstr(s, env->buffer.data);
        match(env, EQUALS);
        break;

    default:
        assign_cstr(s, "Local");
        break;
    }

    return;
}

static
void data_type(MetaioParseEnv const env, enum METAIO_Type * const type)
{
    match(env, TYPE);

    env->buffer.len = 0;
    read_attribute_value(env, &(env->buffer));

    *type = match_type_string(env->buffer.data);
    if (*type == METAIO_TYPE_UNKNOWN)
    {
        /*-- Haven't yet set up pointers for this column, so ignore it --*/
        env->ligo_lw.table.numcols--;
        parse_error(env, -1, "unknown data type \"%s\"", env->buffer.data);
    }

    match(env, EQUALS);
}

static
void delimiter(MetaioParseEnv const env, char* const c)
{
    match(env, DELIMITER);

    env->buffer.len = 0;
    read_attribute_value(env, &(env->buffer));

    if (env->buffer.len > 1)
        parse_error(env, -1, "invalid delimiter \"%s\", must be a single character", env->buffer.data);

    *c = env->buffer.data[0];

    /*
     * Some delimiter characters are banned, including most things that
     * can appear in a row element.
     */
    if (*c == '\0' || strchr("<\"\'\\0123456789+-.Ee", *c))
        parse_error(env, -1, "character '%hhc' is invalid as a delimiter", *c);

    match(env, EQUALS);
}

static
void comment(MetaioParseEnv const env, char **s)
{
    if(env->token == COMMENT)
    {
        match(env, COMMENT);
        env->buffer.len = 0;
        if ( fscanf_lstring(env, &(env->buffer), 0) < 0 )
            parse_error(env, -1, "failure parsing Comment text");
        assign_cstr(s, env->buffer.data);
        match(env, GREATER_THAN);
        match(env, CLOSE_COMMENT);
        match(env, GREATER_THAN);
    }
    else
        assign_cstr(s, "");
}

static
void column_attr(MetaioParseEnv const env)
{
    int colnum = env->ligo_lw.table.numcols - 1;

    switch(env->token)
    {
    case TYPE:
      /*-- Type, then name --*/
      data_type(env, &(env->ligo_lw.table.col[colnum].data_type));
      DEBUGMSG2("DATA TYPE = \"%s\"\n", MetaioTypeText(env->ligo_lw.table.col[colnum].data_type));

      name(env, &(env->ligo_lw.table.col[colnum].name));
      DEBUGMSG2("NAME = \"%s\"\n", env->ligo_lw.table.col[colnum].name);
      break;

    default:
      /*-- Name, then type --*/
      name(env, &(env->ligo_lw.table.col[colnum].name));
      DEBUGMSG2("NAME = \"%s\"\n", env->ligo_lw.table.col[colnum].name);

      data_type(env, &(env->ligo_lw.table.col[colnum].data_type));
      DEBUGMSG2("DATA TYPE = \"%s\"\n", MetaioTypeText(env->ligo_lw.table.col[colnum].data_type));
      break;
    }

    env->ligo_lw.table.elt[colnum].col = &(env->ligo_lw.table.col[colnum]);
    env->ligo_lw.table.elt[colnum].data.int_4s = 0;
    env->ligo_lw.table.elt[colnum].data.int_4u = 0;
    env->ligo_lw.table.elt[colnum].data.int_2s = 0;
    env->ligo_lw.table.elt[colnum].data.int_2u = 0;
    env->ligo_lw.table.elt[colnum].data.int_8s = 0LL;
    env->ligo_lw.table.elt[colnum].data.int_8u = 0LL;
    env->ligo_lw.table.elt[colnum].data.real_4 = 0.0;
    env->ligo_lw.table.elt[colnum].data.real_8 = 0.0;
    env->ligo_lw.table.elt[colnum].data.lstring.data = 0;
    env->ligo_lw.table.elt[colnum].data.lstring.len = 0;
    env->ligo_lw.table.elt[colnum].data.lstring.datasize = 0;
    env->ligo_lw.table.elt[colnum].data.blob.data = 0;
    env->ligo_lw.table.elt[colnum].data.blob.len = 0;
    env->ligo_lw.table.elt[colnum].data.blob.datasize = 0;
    env->ligo_lw.table.elt[colnum].data.complex_8 = 0.0;
    env->ligo_lw.table.elt[colnum].data.complex_16 = 0.0;

    match(env, FORWARD_SLASH);
    match(env, GREATER_THAN);
}

static
void column(MetaioParseEnv const env)
{
    if (env->ligo_lw.table.numcols < env->ligo_lw.table.maxcols)
        env->ligo_lw.table.numcols++;
    else
        parse_error(env, -1, "number of columns exceeds %d", env->ligo_lw.table.maxcols);

    match(env, COLUMN);
    column_attr(env);
}

static
void match_numeric(MetaioParseEnv const env,
                   struct MetaioRowElement* const elt)
{
    int count;

    /*
      Need to skip whitespace with this function, since I don't
      want to fscanf() to eat newlines without incrementing the
      line number
    */
    int c = skip_whitespace(env);
    if(c < 0)
        parse_error(env, -1, "failure reading number:  premature EOF");
    unget_char(env, c);

    /* Zero or more whitespace between two delimiters maps to a null value */
    if(c == env->ligo_lw.table.stream.delimiter)
    {
        elt->valid = 0;
        memset(&elt->data, 0, sizeof(elt->data));
        return;
    }
    elt->valid = 1;

    switch(elt->col->data_type)
    {
    case METAIO_TYPE_REAL_4:
        elt->data.real_4 = 0.0;
        count = gzscanf(env->file->fp, "%f", &(elt->data.real_4));
        break;
    case METAIO_TYPE_REAL_8:
        elt->data.real_8 = 0.0;
        count = gzscanf(env->file->fp, "%lf", &(elt->data.real_8));
        break;
    case METAIO_TYPE_INT_4S:
        elt->data.int_4s = 0;
        count = gzscanf(env->file->fp, "%d", &(elt->data.int_4s));
        break;
    case METAIO_TYPE_INT_4U:
        elt->data.int_4u = 0;
        count = gzscanf(env->file->fp, "%u", &(elt->data.int_4u));
        break;
    case METAIO_TYPE_INT_2S:
        elt->data.int_2s = 0;
        count = gzscanf(env->file->fp, "%hd", &(elt->data.int_2s));
        break;
    case METAIO_TYPE_INT_2U:
        elt->data.int_2u = 0;
        count = gzscanf(env->file->fp, "%hu", &(elt->data.int_2u));
        break;
    case METAIO_TYPE_INT_8S:
        elt->data.int_8s = 0LL;
        count = gzscanf(env->file->fp, "%lld", &(elt->data.int_8s));
        break;
    case METAIO_TYPE_INT_8U:
        elt->data.int_8u = 0LL;
        count = gzscanf(env->file->fp, "%llu", &(elt->data.int_8u));
        break;
    case METAIO_TYPE_COMPLEX_8: {
        METAIO_REAL_4 re = 0.0, im = 0.0;
        count = gzscanf(env->file->fp, "%f+i%f", &re, &im);
        elt->data.complex_8 = re + I * im;
	count = count < 0 ? count : count < 2 ? 0 : 1;
        break;
    }
    case METAIO_TYPE_COMPLEX_16: {
        METAIO_REAL_8 re = 0.0, im = 0.0;
        count = gzscanf(env->file->fp, "%lf+i%lf", &re, &im);
        elt->data.complex_16 = re + I * im;
	count = count < 0 ? count : count < 2 ? 0 : 1;
        break;
    }
    default:
        /* cannot get here */
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fputs("impossible column type\n", stderr);
        exit(1);
    }

    if(count < 1)
        parse_error(env, -1, "failure parsing numeric value");
}

static
int match_lstring(MetaioParseEnv const env,
                   struct MetaioRowElement* const elt)
{
    int c = skip_whitespace(env);
    char terminators[] = {env->ligo_lw.table.stream.delimiter, '<', '\0'};
    if(c < 0)
        parse_error(env, -1, "failure reading lstring:  premature EOF");

    if((c == '\"') || (c == '\''))
    {
        /* Found quoted string, read everything up to the matching
         * unescaped quote */
        elt->data.lstring.len = 0;
        if(fscanf_lstring(env, &(elt->data.lstring), c) < 0)
            parse_error(env, -1, "error parsing string element");
        elt->valid = 1;

        /* Consume the terminating quote */
        if(get_char(env) != c)
            parse_error(env, -1, "unmatched quote when reading string");

        /* Skip trailing white space */
        c = skip_whitespace(env);
        if(c < 0)
            parse_error(env, -1, "failure reading lstring:  premature EOF");
        else if(!strchr(terminators, c))
            parse_error(env, -1, "text following quote when reading string");
        else
            /* Unconsume the terminator */
            unget_char(env, c);
    }
    else if(strchr(terminators, c))
    {
        /* Whitespace between two delimiters --> null value */
        unget_char(env, c);
        elt->valid = 0;
        /* Clear any old data */
        if(!elt->data.lstring.datasize)
            string_resize(&elt->data.lstring, 1);
        elt->data.lstring.len = 0;
        elt->data.lstring.data[0] = '\0';
    }
    else
    {
        /* A string must be quoted. May as well unget what we read. */
        unget_char(env, c);
        parse_error(env, -1, "missing quote when reading string");
    }

    return 0;
}

static
int match_ilwd_char_u(MetaioParseEnv const env,
                       struct MetaioRowElement* const elt)
{
    unsigned char *cptr;
    int c = skip_whitespace(env);
    char terminators[] = {env->ligo_lw.table.stream.delimiter, '<', '\0'};
    char specials[] = {env->ligo_lw.table.stream.delimiter, '\\', ' ', '\0'};
    if(c < 0)
        parse_error(env, -1, "failure reading ilwd_char_u:  premature EOF");

    if (c == '\"')
    {
        int quote = c;

        if(fscanf_ilwd_char_u(env, &(elt->data.blob), terminators, specials) < 0)
            parse_error(env, -1, "error parsing ilwd:char_u element");
        elt->valid = 1;

        /*-- Ignore everything from the terminating quote onward --*/
        for(cptr = elt->data.blob.data + elt->data.blob.len - 1; cptr >= elt->data.blob.data; cptr--)
            if(*cptr == quote)
                break;

        if(cptr < elt->data.blob.data)
            parse_error(env, -1, "unmatched quote when reading ilwd:char_u");
        else
        {
            *cptr = '\0';
            elt->data.blob.len = cptr - elt->data.blob.data;
        }

    }
    else if ( strchr(terminators, c) )
    {
        /* Whitespace between two delimiters --> null value */
        unget_char(env, c);
        elt->valid = 0;
        /* Clear any old data */
        elt->data.blob.len = 0;
    }
    else
    {
        /* An ilwd:char_u *must* be quoted. May as well unget what we read. */
        unget_char(env, c);
        parse_error(env, -1, "missing quote when reading ilwd:char_u");
    }

    return 0;
}


static int match_blob(
    MetaioParseEnv env,
    struct MetaioRowElement *elt
)
{
    int c = skip_whitespace(env);
    char terminators[] = {env->ligo_lw.table.stream.delimiter, '<', '\0'};
    if(c < 0)
        parse_error(env, -1, "failure reading blob:  premature EOF");

    if((c == '\"') || (c == '\''))
    {
        /* Found quoted string, read everything up to the matching
         * quote */
        if(fscanf_blob(env, &(elt->data.blob), c) < 0)
            parse_error(env, -1, "error parsing blob element");
        elt->valid = 1;

        /* Consume the terminating quote */
        if(get_char(env) != c)
            parse_error(env, -1, "unmatched quote when reading blob");

        /* Skip trailing white space */
        c = skip_whitespace(env);
        if(c < 0)
            parse_error(env, -1, "failure reading blob:  premature EOF");
        else if(!strchr(terminators, c))
            parse_error(env, -1, "text following quote when reading blob");
        else
            /* Unconsume the terminator */
            unget_char(env, c);
    }
    else if(strchr(terminators, c))
    {
        /* Whitespace between two delimiters --> null value */
        unget_char(env, c);
        elt->valid = 0;
        /* Clear any old data */
        elt->data.blob.len = 0;
    }
    else
    {
        /* A blob must be quoted. May as well unget what we read. */
        unget_char(env, c);
        parse_error(env, -1, "missing quote when reading blob");
    }

    return 0;
}


static
void match_delimiter(MetaioParseEnv const env)
{
    int c = skip_whitespace(env);

    if (c != env->ligo_lw.table.stream.delimiter)
    {
        unget_char(env, c);
        parse_error(env, -1, (c < 0) ? "premature EOF, expected delimiter character '%c'" : "expected delimiter character '%c'", env->ligo_lw.table.stream.delimiter);
    }
}

/*
 * The following group of functions correspond to expanding the production
 * rules of the parser.
 */

static
void row_element(MetaioParseEnv const env, struct MetaioRowElement* const elt)
{
    /*-- Mark this item as invalid, until we successfully parse it --*/
    elt->valid = 0;

    switch (elt->col->data_type)
    {
    case METAIO_TYPE_REAL_4:
    case METAIO_TYPE_REAL_8:
    case METAIO_TYPE_INT_4S:
    case METAIO_TYPE_INT_4U:
    case METAIO_TYPE_INT_2S:
    case METAIO_TYPE_INT_2U:
    case METAIO_TYPE_INT_8S:
    case METAIO_TYPE_INT_8U:
    case METAIO_TYPE_COMPLEX_8:
    case METAIO_TYPE_COMPLEX_16:
        match_numeric(env, elt);
        break;
    case METAIO_TYPE_LSTRING:
    case METAIO_TYPE_ILWD_CHAR:
    case METAIO_TYPE_CHAR_S:
    case METAIO_TYPE_CHAR_V:
        match_lstring(env, elt);
        break;
    case METAIO_TYPE_BLOB:
        match_blob(env, elt);
        break;
    case METAIO_TYPE_ILWD_CHAR_U:
        match_ilwd_char_u(env, elt);
        break;
    default:
        parse_error(env, -1, "row element has unknown type");
        break;
    }
}

static
int row(MetaioParseEnv const env)
{
    int numcols = env->ligo_lw.table.numcols;
    int col;
    /* Peek ahead to the next non-whitespace character */
    int c = skip_whitespace(env);
    if(c < 0)
        parse_error(env, -1, "failure reading row:  premature EOF");

    unget_char(env, c);

    if (c == '<')
        /* '<' --> start of new element = end of stream text */
        return 0;

    /* Process the whole row */
    for (col = 0; col < numcols - 1; col++)
    {
        row_element(env, &(env->ligo_lw.table.elt[col]));
        match_delimiter(env);
    }
    row_element(env, &(env->ligo_lw.table.elt[col]));

    /* finished reading 1 row */
    return 1;
}

static
void stream_attr(MetaioParseEnv const env)
{
    int attr_mask = 0;

    env->ligo_lw.table.stream.delimiter = ',';	/* default delimiter */

    do {
        switch(env->token)
        {
        case DELIMITER:
            if(attr_mask & 1)
                parse_error(env, -1, "duplicate Delimiter attribute for Stream");
            delimiter(env, &(env->ligo_lw.table.stream.delimiter));
            DEBUGMSG2("DELIMITER = \"%c\"\n", env->ligo_lw.table.stream.delimiter);
            attr_mask |= 1;
            continue;

        case NAME:
            if(attr_mask & 2)
                parse_error(env, -1, "duplicate Name attribute for Stream");
            name(env, &(env->ligo_lw.table.stream.name));
            DEBUGMSG2("NAME = \"%s\"\n", env->ligo_lw.table.stream.name);
            attr_mask |= 2;
            continue;

        case TYPE:
            if(attr_mask & 4)
                parse_error(env, -1, "duplicate Type attribute for Stream");
            type(env, &(env->ligo_lw.table.stream.type));
            DEBUGMSG2("TYPE = \"%s\"\n", env->ligo_lw.table.stream.type);
            attr_mask |= 4;
            continue;

        default:
            break;
        }
        break;
    } while(1);

    /*
      This is a special case. We would normally do a match to
      GREATER_THAN but that would also trigger an advance to the
      next token. Since we expect to be reading data from the
      stream object after this, the next data read shouldn't be tokenized.
      Instead we match the GREATER_THAN after we return from reading
      the stream

      It is a little annoying that the stream_attr function needs to
      be clairvoyant but in practise the initial parsing will only read
      to here anyway. Reading the rows will be done on an individual basis
      by the user.
      
      Ideally what we would do is unget all the text read by match(), but
      unfortunately ungetting is only guaranteed for one character.
    */

    /*-- If the stream is empty, the end of the tag will be "/>" --*/
    if (env->token == FORWARD_SLASH)
        match(env, FORWARD_SLASH);

    if (env->token != GREATER_THAN)
        unexpected_token(env, env->token);

    /*
     * We are now at the end of the stream attributes for this table and
     * are about to begin reading rows
     */
    env->file->nrows = 0;
}

/*
 * This function is used to when we encounter a table which is not the one
 * specified in tablename. The function scans through the body of the
 * table until it finds the end of the table's STREAM, then looks for the
 * tokens to close the table. At the end we should be in the correct position
 * in the file to look for the next table (and so on, until we get to the
 * right table or run out of file). The body of the table is not parsed.
 */

static
void table_consume(MetaioParseEnv const env)
{
#if 1	/* blast through with no validation (fast) */
    while (env->token != CLOSE_STREAM)
        get_next_token(env);
    get_next_token(env);
    match(env, GREATER_THAN);
  
    /* Close the <TABLE> */
    match(env, CLOSE_TABLE);
    match(env, GREATER_THAN);
#else	/* validate the table while consuming it (slow) */
    /* consume remaining rows */
    while (row(env) != 0)
    {
        int c = skip_whitespace(env);
        if(c < 0)
            parse_error(env, -1, "failure reading row:  premature EOF");
        else if (c != env->ligo_lw.table.stream.delimiter)
            unget_char(env, c);
    }

    /*-- Skip any tokens found until we get to the CLOSE_TABLE.  The
      actual set of tokens encountered will vary depending on whether the
      stream was empty (in which case there is no separate </STREAM> tag)
      or non-empty. --*/
    while ( env->token != CLOSE_TABLE || env->token == END_OF_FILE )
        get_next_token(env);

    /* Close the <TABLE> */
    match(env, CLOSE_TABLE);
    match(env, GREATER_THAN);
#endif
}

static
void stream(MetaioParseEnv const env)
{
    match(env, STREAM);
    stream_attr(env);
}

static
void table_attr(MetaioParseEnv const env)
{
    name(env, &(env->ligo_lw.table.name));
    DEBUGMSG2("NAME = \"%s\"\n", env->ligo_lw.table.name); 

    match(env, GREATER_THAN);
}

static
void table_body(MetaioParseEnv const env)
{
    comment(env, &(env->ligo_lw.table.comment));
    DEBUGMSG2("COMMENT = \"%s\"\n", env->ligo_lw.table.comment); 

    env->ligo_lw.table.numcols = 0;
    while (env->token == COLUMN)
        column(env);

    stream(env);
}

static
void table(MetaioParseEnv const env)
{
    match(env, TABLE);
    table_attr(env);
    table_body(env);
    if (!match_tablename(env))
        table_consume(env);
}

static
void ligo_lw_attr(MetaioParseEnv const env)
{
    name(env, &(env->ligo_lw.name));
    DEBUGMSG2("NAME = \"%s\"\n", env->ligo_lw.name); 

    match(env, GREATER_THAN);
}

static
void ligo_lw_body(MetaioParseEnv const env)
{
    comment(env, &(env->ligo_lw.comment));
    DEBUGMSG2("COMMENT = \"%s\"\n", env->ligo_lw.comment); 

    while (env->token == TABLE)
        table(env);

    if (env->token != GREATER_THAN)
        parse_error(env, -1, "table not found: \"%s\"", env->file->tablename);
}

static
void ligo_lw(MetaioParseEnv const env)
{
    match(env, LIGO_LW);
    ligo_lw_attr(env);
    ligo_lw_body(env);
}

/*
 * This function consumes all of the uninteresting boilerplate at the
 * beginning of a LIGO_LW XML file, up to the <LIGO_LW token.
 */

static
void leading_junk(MetaioParseEnv const env)
{
    do
        get_next_token(env);
    while(env->token != LIGO_LW && env->token != END_OF_FILE);
}

/*
 * Various useful functions
 */

static
int putbinary(FILE *fp, const METAIO_CHAR_U *buf, size_t len)
{
    if(fputc('\"', fp) == EOF)
        return -1;
    while(len--)
        if(fprintf(fp, "\\%03hho", *(buf++)) < 0)
            return -1;
    if(fputc('\"', fp) == EOF)
        return -1;
    return 0;
}

static
int putblob(FILE *fp, const METAIO_CHAR_U *buf, size_t len)
{
    char *b64 = NULL;
    size_t b64len;

    b64len = base64_encode_alloc((char *) buf, len, &b64);
    if(!b64)
        return -1;

    if(fprintf(fp, "\"%s\"", b64) < 0)
    {
        free(b64);
        return -1;
    }

    free(b64);
    return 0;
}

static
int putstring(FILE *fp, const char *string, const char *escaped)
{
    for(; *string; string++)
        switch(*string)
        {
        case '>':
            if(fputs("&gt;", fp) == EOF)
                return -1;
            break;
        case '<':
            if(fputs("&lt;", fp) == EOF)
                return -1;
            break;
        case '&':
            if(fputs("&amp;", fp) == EOF)
                return -1;
            break;
        default:
            if(!isprint(*string) || *string < 0)
                /* Unprintable character or non-UTF-8-safe --> error */
                return -1;
            if(strchr(escaped, *string))
                /* Character that must be escaped --> precede with '\\' */
                if(fputc('\\', fp) == EOF)
                    return -1;
            /* Print character */
            if(fputc(*string, fp) == EOF)
                return -1;
            break;
        }

    return 0;
}

int MetaioFprintElement(FILE *fp, const struct MetaioRowElement *elt)
{
    /* null value --> print nothing */
    if(!elt->valid)
        return 0;

    switch(elt->col->data_type)
    {
    case METAIO_TYPE_INT_4S:
        if(fprintf(fp, "%d", elt->data.int_4s) < 0)
            return -1;
        break;
    case METAIO_TYPE_INT_4U:
        if(fprintf(fp, "%u", elt->data.int_4u) < 0)
            return -1;
        break;
    case METAIO_TYPE_INT_2S:
        if(fprintf(fp, "%hd", elt->data.int_2s) < 0)
            return -1;
        break;
    case METAIO_TYPE_INT_2U:
        if(fprintf(fp, "%hu", elt->data.int_2u) < 0)
            return -1;
        break;
    case METAIO_TYPE_INT_8S:
        if(fprintf(fp, "%lld", elt->data.int_8s) < 0)
            return -1;
        break;
    case METAIO_TYPE_INT_8U:
        if(fprintf(fp, "%llu", elt->data.int_8u) < 0)
            return -1;
        break;
    case METAIO_TYPE_REAL_4:
        if(fprintf(fp, "%.9g", elt->data.real_4) < 0)
            return -1;
        break;
    case METAIO_TYPE_REAL_8:
        if(fprintf(fp, "%.17g", elt->data.real_8) < 0)
            return -1;
        break;
    case METAIO_TYPE_COMPLEX_8:
        if(fprintf(fp, "%.9g+i%9g", crealf(elt->data.complex_8), cimagf(elt->data.complex_8)) < 0)
            return -1;
        break;
    case METAIO_TYPE_COMPLEX_16:
        if(fprintf(fp, "%.17g+i%17g", creal(elt->data.complex_16), cimag(elt->data.complex_16)) < 0)
            return -1;
        break;
    case METAIO_TYPE_LSTRING:
    case METAIO_TYPE_ILWD_CHAR:
    case METAIO_TYPE_CHAR_S:
    case METAIO_TYPE_CHAR_V:
        if(fputc('\"', fp) == EOF)
            return -1;
        if(putstring(fp, elt->data.lstring.data, "\\\"") < 0)
            return -1;
        if(fputc('\"', fp) == EOF)
            return -1;
        break;
    case METAIO_TYPE_BLOB:
        if(putblob(fp, elt->data.blob.data, elt->data.blob.len) < 0)
            return -1;
        break;
    case METAIO_TYPE_ILWD_CHAR_U:
        if(putbinary(fp, elt->data.blob.data, elt->data.blob.len) < 0)
            return -1;
        break;
    default:
        return -1;
    }

    return 0;
}

static
void putheader( const MetaioParseEnv env )
{
    FILE *fp = env->file->fp;
    struct MetaioTable *table = &(env->ligo_lw.table);
    int icol;

    fprintf(fp, "\n\t<Table Name=\"%s\">", table->name);

    /* Comment (if any) */
    if(table->comment)
    {
        fputs("\n\t\t<Comment>", fp);
        putstring(fp, table->comment, "\\");
        fputs("</Comment>", fp);
    }

    /* Column elements */
    for(icol = 0; icol < table->numcols; icol++)
        fprintf(fp, "\n\t\t<Column Name=\"%s\" Type=\"%s\"/>", table->col[icol].name, TypeText[table->col[icol].data_type].name[0]);

    /* Stream element start tag */
    fprintf(fp, "\n\t\t<Stream Name=\"%s\" Type=\"Local\" Delimiter=\"%c\">", table->name, ',');

    return;
}

int MetaioOpenFile(MetaioParseEnv const env, const char* const filename)
{
    int result;

    result = setjmp(env->jmp_env);
    if(result)
        /* We longjmp'ed to here --> parse error */
        return result;
    return init_parse_env(env, filename, "r");
}

int MetaioOpen(MetaioParseEnv const env, const char * const filename)
{
    /*
     * This function is now replaced by MetaioOpenTable(), which
     * is identical to MetaioOpen when the tablename is a null pointer
     */
    return MetaioOpenTable(env, filename, 0);
}

int MetaioOpenTableOnly(MetaioParseEnv const env, const char * const tablename)
{
    int result;

    result = setjmp(env->jmp_env);
    if(result)
        /* We longjmp'ed to here --> parse error */
        return result;

    /* If file is not open, return an error */
    if(env->fileRec.fp == 0)
        return 1;
    if(tablename)
        assign_cstr(&env->file->tablename, tablename);
    leading_junk(env);
    ligo_lw(env);

    return 0;
}

int MetaioOpenTable(MetaioParseEnv const env, const char * const filename,
                    const char * const tablename)
{
    int result;

    result = setjmp(env->jmp_env);
    if(result)
        /* We longjmp'ed to here --> parse error */
        return result;

    result = init_parse_env(env, filename, "r");
    if(result)
        return result;
    if(tablename)
        assign_cstr(&env->file->tablename, tablename);
    leading_junk(env);
    ligo_lw(env);

    return 0;
}

int MetaioGetRow(MetaioParseEnv const env)
{
    int result;
    int c;

    result = setjmp(env->jmp_env);
    if(result)
        /* We longjmp'ed to here --> parse error */
        return result;

    result = row(env);
    if(result == 0)
        /* end of table */
	return 0;

    c = skip_whitespace(env);
    if(c < 0)
        parse_error(env, -1, "failure reading row:  premature EOF");
    else if (c != env->ligo_lw.table.stream.delimiter)
        unget_char(env, c);
    /* Increment the count of the number of rows */
    env->file->nrows++;

    /* Success */
    return 1;
}

int MetaioClose(MetaioParseEnv const env)
{
    int result;

    /* file has already been closed, or the open failed --> no-op */
    if ( !env->file->fp )
        return 0;

    /* Reset the position to which errors jump */
    result = setjmp(env->jmp_env);
    if(result)
        /* We longjmp'ed to here --> parse error */
        return destroy_parse_env(env);

    /* Try to parse the rest of the file (input files only). */
    if ( env->file->mode == 'r' )
    {
	/* FIXME:  we don't check that the rest of the document is valid,
	 * we just skip over all of it. could call table_consume() in a
	 * loop like is done in table() */
        /* Find the end of the LIGO_LW element */
        while ( env->token != CLOSE_LIGO_LW || env->token == END_OF_FILE )
            get_next_token(env);

        /* Close the <LIGO_LW> */
        match(env, CLOSE_LIGO_LW);
        match(env, GREATER_THAN);
        match(env, END_OF_FILE);
    }
    /*-- Handle output file --*/
    else if ( env->file->mode == 'w' )
    {
        /*-- If header was never written out, write it out now --*/
        if ( env->file->nrows == 0 )
            putheader( env );

        /*-- Write out the file trailer stuff before closing --*/
        fputs( "\n\t\t</Stream>\n\t</Table>\n</LIGO_LW>", env->file->fp );
    }
    else
    {
        /* !?  unknown file mode */
    }

    return destroy_parse_env(env);
}

int MetaioAbort(MetaioParseEnv const env)
{
    return destroy_parse_env(env);
}

char *MetaioColumnName( const MetaioParseEnv env, int icol )
/*--
  Written 31 Jan 2001 by Peter Shawhan.
  Returns a pointer to the column name, ignoring any "prefix" string(s)
  delimited by colons.  For instance, if the file contains a column named
  "processgroup:process:program", this routine returns a pointer to the
  first letter of "program".  If an invalid column number is passed,
  this routine returns 0.
--*/
{
    char *colnamePtr, *colonPtr;

    if ( icol < 0 || icol > env->ligo_lw.table.numcols-1 )
        return 0;

    colnamePtr =  env->ligo_lw.table.col[icol].name;
    colonPtr = strrchr( colnamePtr, ':' );
    return colonPtr == 0 ? colnamePtr : colonPtr + 1;
}


int MetaioFindColumn( const MetaioParseEnv env, const char *name )
/*--
  Written 31 Jan 2001 by Peter Shawhan.
  Returns the index of the column with the specified name, or -1 if no such
  column is found.  The name comparison is case-insensitive and ignores any
  "prefix" strings, delimited by colons, in the column name in the file.
--*/
{
    int icol;
    char *cptr;

    for ( icol = 0; icol < env->ligo_lw.table.numcols; icol++ )
    {
        cptr = MetaioColumnName( env, icol );
        if ( cptr == 0 )
            return -1;
        if ( strcasecmp( name, cptr ) == 0 )
            return icol;
    }

    /*-- Column was not found --*/
    return -1;
}


int MetaioCompareElements( struct MetaioRowElement *elt1,
                           struct MetaioRowElement *elt2 )
/*--
  Written 1 Feb 2001 by Peter Shawhan.
  Does an appropriate comparison of the element values, which depends on the
  particular data type.  Returns 0 if the values are equal; returns -1 if
  the first value is "less" than the second value; returns 1 if the first
  value is "greater" than the second value.  If the two elements cannot be
  compared (e.g. they have different types), returns 2.  If two complex
  numbers are not equal, the return value is always -1.
--*/
{
    int retval;
    enum METAIO_Type type1 = -1, type2 = -1;
    METAIO_INT_8S ival1 = 0, ival2 = 0;
    METAIO_INT_8U uval1 = 0, uval2 = 0;
    METAIO_REAL_8 dval1 = 0, dval2 = 0;
    METAIO_COMPLEX_16 cval1 = 0, cval2 = 0;
    size_t size1 = 0, size2 = 0;
    const void *ptr1 = NULL, *ptr2 = NULL;

    /* If one or the other is a null value, treat as "zero" */
    if(elt1->valid && !elt2->valid)
        return 1;
    if(!elt1->valid && elt2->valid)
        return -1;
    if(!elt1->valid && !elt2->valid)
        return 0;

    /*
     * bring object 1 to canonical form for comparison.
     */
    switch ( elt1->col->data_type ) {
    case METAIO_TYPE_LSTRING:
    case METAIO_TYPE_ILWD_CHAR:
    case METAIO_TYPE_CHAR_S:
    case METAIO_TYPE_CHAR_V:
        ptr1 = elt1->data.lstring.data;
        size1 = elt1->data.lstring.len;
        type1 = METAIO_TYPE_LSTRING;
        break;
    case METAIO_TYPE_BLOB:
    case METAIO_TYPE_ILWD_CHAR_U:
        ptr1 = elt1->data.blob.data;
        size1 = elt1->data.blob.len;
        type1 = METAIO_TYPE_BLOB;
        break;
    case METAIO_TYPE_INT_4S:
        ival1 = elt1->data.int_4s;
        type1 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_4U:
        uval1 = elt1->data.int_4u;
        type1 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_INT_2S:
        ival1 = elt1->data.int_2s;
        type1 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_2U:
        uval1 = elt1->data.int_2u;
        type1 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_INT_8S:
        ival1 = elt1->data.int_8s;
        type1 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_8U:
        uval1 = elt1->data.int_8u;
        type1 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_REAL_4:
        dval1 = elt1->data.real_4;
        type1 = METAIO_TYPE_REAL_8;
        break;
    case METAIO_TYPE_REAL_8:
        dval1 = elt1->data.real_8;
        type1 = METAIO_TYPE_REAL_8;
        break;
    case METAIO_TYPE_COMPLEX_8:
        cval1 = elt1->data.complex_8;
        type1 = METAIO_TYPE_COMPLEX_16;
        break;
    case METAIO_TYPE_COMPLEX_16:
        cval1 = elt1->data.complex_16;
        type1 = METAIO_TYPE_COMPLEX_16;
        break;

    default:
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fprintf(stderr, "unknown data type %d\n", type1);
        exit(1);
    }

    /*
     * bring object 2 to canonical form for comparison.
     */
    switch ( elt2->col->data_type ) {
    case METAIO_TYPE_LSTRING:
    case METAIO_TYPE_ILWD_CHAR:
    case METAIO_TYPE_CHAR_S:
    case METAIO_TYPE_CHAR_V:
        ptr2 = elt2->data.lstring.data;
        size2 = elt2->data.lstring.len;
        type2 = METAIO_TYPE_LSTRING;
        break;
    case METAIO_TYPE_BLOB:
    case METAIO_TYPE_ILWD_CHAR_U:
        ptr2 = elt2->data.blob.data;
        size2 = elt2->data.blob.len;
        type2 = METAIO_TYPE_BLOB;
        break;
    case METAIO_TYPE_INT_4S:
        ival2 = elt2->data.int_4s;
        type2 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_4U:
        uval2 = elt2->data.int_4u;
        type2 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_INT_2S:
        ival2 = elt2->data.int_2s;
        type2 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_2U:
        uval2 = elt2->data.int_2u;
        type2 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_INT_8S:
        ival2 = elt2->data.int_8s;
        type2 = METAIO_TYPE_INT_8S;
        break;
    case METAIO_TYPE_INT_8U:
        uval2 = elt2->data.int_8u;
        type2 = METAIO_TYPE_INT_8U;
        break;
    case METAIO_TYPE_REAL_4:
        dval2 = elt2->data.real_4;
        type2 = METAIO_TYPE_REAL_8;
        break;
    case METAIO_TYPE_REAL_8:
        dval2 = elt2->data.real_8;
        type2 = METAIO_TYPE_REAL_8;
        break;
    case METAIO_TYPE_COMPLEX_8:
        cval2 = elt1->data.complex_8;
        type2 = METAIO_TYPE_COMPLEX_16;
        break;
    case METAIO_TYPE_COMPLEX_16:
        cval2 = elt1->data.complex_16;
        type2 = METAIO_TYPE_COMPLEX_16;
        break;

    default:
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fprintf(stderr, "unknown data type %d\n", type2);
        exit(1);
    }

    if(type1 != type2)
    {
        /* FIXME:  some type mismatches, for example ints with floats,
         * could be handled through conversions */
        return 2;
    }

    /*-- Now compare numerical or lexical values --*/
    switch ( type1 ) {
    case METAIO_TYPE_COMPLEX_16:
        retval = cval1 == cval2 ? 0 : -1;
        break;

    case METAIO_TYPE_REAL_8:
        retval = dval1 > dval2 ? 1 : dval1 < dval2 ? -1 : 0;
        break;

    case METAIO_TYPE_INT_8S:
        retval = ival1 > ival2 ? 1 : ival1 < ival2 ? -1 : 0;
        break;

    case METAIO_TYPE_INT_8U:
        retval = uval1 > uval2 ? 1 : uval1 < uval2 ? -1 : 0;
        break;

    case METAIO_TYPE_LSTRING:
        retval = strncmp(ptr1, ptr2, size1 < size2 ? size1 : size2);
        /* If arrays are the same up to the point where the shorter one
         * ends, then consider the longer array to be "greater" */
        if(!retval)
            retval = size1 > size2 ? 1 : size1 < size2 ? -1 : 0;
        break;

    case METAIO_TYPE_BLOB:
        /* compares them as unsigned bytes */
        retval = memcmp(ptr1, ptr2, size1 < size2 ? size1 : size2);
        /* If arrays are the same up to the point where the shorter one
         * ends, then consider the longer array to be "greater" */
        if(!retval)
            retval = size1 > size2 ? 1 : size1 < size2 ? -1 : 0;
        break;

    default:
        fprintf(stderr, "BUG at %s line %d\n", __FILE__, __LINE__);
        fputs("impossible case in switch statement", stderr);
        exit(1);
    }

    return retval;
}


int MetaioCreate( const MetaioParseEnv env, const char* const filename )
/*--
  Written 15 Jul 2002 by Peter Shawhan.
  Opens a file for writing, and writes the LIGO_LW header.
  Returns 0 if successful, nonzero if there was an error creating the file.
--*/
{
    int status;

    status = init_parse_env( env, filename, "w" );
    if ( status )
        return status;

    /*-- Write out the LIGO_LW header --*/
    fputs( MetaIO_Header, env->file->fp);

    return 0;
}


int MetaioCopyEnv( const MetaioParseEnv dest, const MetaioParseEnv source )
/*--
  Copies column definitions, etc., from one metaio environment to another.
  Returns 0 if successful, nonzero if there was an error.
  Written 15 Jul 2002 by Peter Shawhan.
--*/
{
    int icol;
    struct MetaioRowElement* elt;

    dest->ligo_lw.table.name = strdup(source->ligo_lw.table.name);
    dest->ligo_lw.table.comment = strdup(source->ligo_lw.table.comment);
    dest->ligo_lw.table.numcols = source->ligo_lw.table.numcols;

    for ( icol = 0; icol < dest->ligo_lw.table.numcols; icol++ )
    {
        dest->ligo_lw.table.col[icol].name =
            strdup(source->ligo_lw.table.col[icol].name );

        dest->ligo_lw.table.col[icol].data_type = 
            source->ligo_lw.table.col[icol].data_type;

        elt = &(dest->ligo_lw.table.elt[icol]);

        elt->col = &(dest->ligo_lw.table.col[icol]);
        elt->data.int_4s = 0;
        elt->data.int_4u = 0;
        elt->data.int_2s = 0;
        elt->data.int_2u = 0;
        elt->data.int_8s = 0LL;
        elt->data.int_8u = 0LL;
        elt->data.real_4 = 0.0;
        elt->data.real_8 = 0.0;
        elt->data.complex_8 = 0.0;
        elt->data.complex_16 = 0.0;
        elt->data.lstring.data = 0;
        elt->data.lstring.len = 0;
        elt->data.lstring.datasize = 0;
        elt->data.blob.data = 0;
        elt->data.blob.len = 0;
        elt->data.blob.datasize = 0;
    }

    return 0;
}


int MetaioCopyRow( const MetaioParseEnv dest, const MetaioParseEnv source )
/*--
  Copies row contents from one metaio stream to another.
  Returns 0 if successful, nonzero if there was an error.
  Written 15 Jul 2002 by Peter Shawhan.
--*/
{
    int icol;
    struct MetaioRowElement *selt, *delt;
    int copysize;

    for ( icol = 0; icol < dest->ligo_lw.table.numcols; icol++ )
    {
        /* Get pointers to source and dest elements, for convenience */
        selt = &(source->ligo_lw.table.elt[icol]);
        delt = &(dest->ligo_lw.table.elt[icol]);

        /* Don't try to copy data for null elements */
        delt->valid = selt->valid;
        if(!delt->valid)
        {
            memset(&delt->data, 0, sizeof(delt->data));
            continue;
        }


        switch ( dest->ligo_lw.table.col[icol].data_type )
        {
        case METAIO_TYPE_BLOB:
        case METAIO_TYPE_ILWD_CHAR_U:
            if(delt->data.blob.datasize < selt->data.blob.datasize)
                stringu_resize(&(delt->data.blob), selt->data.blob.datasize);
            copysize = selt->data.blob.len + 1;
            if(copysize > selt->data.blob.datasize)
                copysize = selt->data.blob.datasize;
            memcpy(delt->data.blob.data, selt->data.blob.data, copysize);
            delt->data.blob.len = selt->data.blob.len;
            break;

        case METAIO_TYPE_LSTRING:
        case METAIO_TYPE_ILWD_CHAR:
        case METAIO_TYPE_CHAR_S:
        case METAIO_TYPE_CHAR_V:
            if ( delt->data.lstring.datasize < selt->data.lstring.datasize )
                string_resize( &(delt->data.lstring), selt->data.lstring.datasize );
            copysize = selt->data.lstring.len + 1;
            if ( copysize > selt->data.lstring.datasize )
                copysize = selt->data.lstring.datasize;
            memcpy( delt->data.lstring.data, selt->data.lstring.data, copysize );
            delt->data.lstring.len = selt->data.lstring.len;
            break;

        default:
            delt->data = selt->data;
            break;
        }

    }

    return 0;
}


int MetaioPutRow( const MetaioParseEnv env )
/*--
  Writes out the current row.
  Returns 0 if successful, nonzero if there was an error.
  Written 15 Jul 2002 by Peter Shawhan.
--*/
{
    FILE *fp = env->file->fp;
    const struct MetaioTable *table = &env->ligo_lw.table;
    int icol;

    /* If file is not open for writing, just return */
    if(!fp)
        return 0;

    if(env->file->mode != 'w')
        return 1;

    /* If we have not yet written out any rows write the table header
     * otherwise write a delimiter */
    if(env->file->nrows == 0)
        putheader(env);
    else
        fputs(",", fp);

    /* Write out the data for this row */
    for(icol = 0; icol < table->numcols; icol++)
    {
        fputs(icol ? "," : "\n\t\t\t", fp);
        MetaioFprintElement(fp, &table->elt[icol]);
    }

    /*-- Increment the count of the number of rows written out --*/
    env->file->nrows++;

    return 0;
}
