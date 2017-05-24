/*=============================================================================
_getMetaLoopHelper - C program to extract information needed to figure out
how to modify a database query to retrieve additional rows, after running into
LDAS's limit.
Written Feb 2002 by Peter Shawhan
Uses the "Metaio" parsing code by Philip Charlton
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "metaio.h"

#define DEBUG 0
#define MAXSORTCOL 16
#define MAXCOLNAMELENGTH 256
#define MAXLISTSIZE 1024
#define MAXVALBUF 65536
#define MAXITEMLENGTH 2048
#define MAXQUALLENGTH 32768

/*===========================================================================*/
void PrintUsage( void )
{
  printf( "Usage: _getMetaLoopHelper <file>\n" );
  return;
}


/*===========================================================================*/
void GetPtrAndLen( struct MetaioRowElement *elt, void **ptrptr, int *lenptr )
{
  /*-- Location and size of data depends on the data type --*/

  switch ( elt->col->data_type )
  {
  case METAIO_TYPE_LSTRING:
  case METAIO_TYPE_ILWD_CHAR:
  case METAIO_TYPE_CHAR_S:
  case METAIO_TYPE_CHAR_V:
    *ptrptr = (void *) elt->data.lstring.data;
    *lenptr = elt->data.lstring.len + 1;
    break;
  case METAIO_TYPE_BLOB:
  case METAIO_TYPE_ILWD_CHAR_U:
    *ptrptr = (void *) elt->data.blob.data;
    *lenptr = elt->data.blob.len;
    break;
  case METAIO_TYPE_INT_4S:
    *ptrptr = (void *) &(elt->data.int_4s);
    *lenptr = 4;
    break;
  case METAIO_TYPE_INT_4U:
    *ptrptr = (void *) &(elt->data.int_4u);
    *lenptr = 4;
    break;
  case METAIO_TYPE_INT_2S:
    *ptrptr = (void *) &(elt->data.int_2s);
    *lenptr = 2;
    break;
  case METAIO_TYPE_INT_2U:
    *ptrptr = (void *) &(elt->data.int_2u);
    *lenptr = 2;
    break;
  case METAIO_TYPE_INT_8S:
    *ptrptr = (void *) &(elt->data.int_8s);
    *lenptr = 8;
    break;
  case METAIO_TYPE_INT_8U:
    *ptrptr = (void *) &(elt->data.int_8u);
    *lenptr = 8;
    break;
  case METAIO_TYPE_REAL_4:
    *ptrptr = (void *) &(elt->data.real_4);
    *lenptr = 4;
    break;
  case METAIO_TYPE_REAL_8:
    *ptrptr = (void *) &(elt->data.real_8);
    *lenptr = 8;
    break;
  default:
    /*-- Error --*/
    printf( "Error: Unknown type in GetPtrAndLen??\n" );
    exit(2);
  }

  return;
}


/*===========================================================================*/
int FormatVal( char *optr, int maxsize, enum METAIO_Type data_type, void *vptr, int len )
{
  if ( maxsize < 32 ) {
    /* Not enough space in output array */
    return 1;
  }

  switch ( data_type )
  {
  case METAIO_TYPE_INT_4S:
    sprintf( optr, "%d", (int) *((METAIO_INT_4S *) vptr) );
    break;
  case METAIO_TYPE_INT_4U:
    sprintf( optr, "%u", (unsigned int) *((METAIO_INT_4U *) vptr) );
    break;
  case METAIO_TYPE_INT_2S:
    sprintf( optr, "%hd", (short) *((METAIO_INT_2S *) vptr) );
    break;
  case METAIO_TYPE_INT_2U:
    sprintf( optr, "%hd", (unsigned short) *((METAIO_INT_2U *) vptr) );
    break;
  case METAIO_TYPE_INT_8S:
    sprintf( optr, "%lld", (long long) *((METAIO_INT_8S *) vptr) );
    break;
  case METAIO_TYPE_INT_8U:
    sprintf( optr, "%llu", (unsigned long long) *((METAIO_INT_8U *) vptr) );
    break;
  case METAIO_TYPE_REAL_4:
    sprintf( optr, "%.8g", *((METAIO_REAL_4 *) vptr) );
    break;
  case METAIO_TYPE_REAL_8:
    sprintf( optr, "%.17g", *((METAIO_REAL_8 *) vptr) );
    break;
  case METAIO_TYPE_LSTRING:
  case METAIO_TYPE_ILWD_CHAR:
  case METAIO_TYPE_CHAR_S:
  case METAIO_TYPE_CHAR_V:
    if ( maxsize < len+2 ) {
      /* Not enough space in output array */
      return 1;
    }
    sprintf( optr, "'%s'", (METAIO_CHAR *) vptr );
    break;
  case METAIO_TYPE_BLOB:
  case METAIO_TYPE_ILWD_CHAR_U:
    if ( maxsize < 2*len+3 ) {
      /* Not enough space in output array */
      return 1;
    }
    sprintf( optr, "x\'" ); optr += 2;
    {
      METAIO_CHAR_U *uptr = (METAIO_CHAR_U *) vptr;
      size_t i = 0;
      for (i = 0; i < len; i++)
	{
	  sprintf( optr, "%02x", (unsigned int) uptr[i] ); optr+=strlen(optr);
	}
    }
    sprintf( optr, "\'" );
    break;
  default:
    /*-- Error --*/
    printf( "Error: Unknown type in GetPtrAndLen??\n" );
    exit(2);
  }

  return 0;
}


/*===========================================================================*/
void HackPrintVal( int data_type, void *vptr, int len )
{
  switch ( data_type )
  {
  case METAIO_TYPE_INT_4S:
    printf( "%d", *((METAIO_INT_4S *) vptr) );
    break;
  case METAIO_TYPE_INT_2S:
    printf( "%hd", *((METAIO_INT_2S *) vptr) );
    break;
  case METAIO_TYPE_INT_8S:
    printf( "%lld", *((METAIO_INT_8S *) vptr) );
    break;
  case METAIO_TYPE_REAL_4:
    printf( "%.8g", *((METAIO_REAL_4 *) vptr) );
    break;
  case METAIO_TYPE_REAL_8:
    printf( "%.17g", *((METAIO_REAL_8 *) vptr) );
    break;
  case METAIO_TYPE_LSTRING:
    printf( "%s", (METAIO_CHAR *) vptr );
    break;
  case METAIO_TYPE_ILWD_CHAR:
    {
      METAIO_CHAR *cptr = (METAIO_CHAR *) vptr;
      size_t i = 0;
      for (i = 0; i < len; i++)
	{
	  printf( "%c", cptr[i] );
	}
    }
    break;
  case METAIO_TYPE_ILWD_CHAR_U:
    {
      METAIO_CHAR_U *uptr = (METAIO_CHAR_U *) vptr;
      size_t i = 0;
      for (i = 0; i < len; i++)
	{
	  printf( "\\%03o", uptr[i] );
	}
    }
    break;
  }

  return;
}


/*===========================================================================*/
int main( int argc, char **argv )
{
  struct MetaioParseEnvironment parseEnv;
  const MetaioParseEnv env = &parseEnv;

  char *file=NULL;
  int readstatus, status, flag, diff;
  int isortcol, ncols, icol, nrows, ilist, i;
  char *comment, *query, *querylc;
  char *cptr, *optr, *qptr;
  int squote, dquote, lspace;
  void *vptr;
  int len;

  int nsortcol;
  char sortcolname[MAXSORTCOL][MAXCOLNAMELENGTH];
  int sortcoltype[MAXSORTCOL];
  int sortcoli[MAXSORTCOL];
  void *sortvptr[MAXSORTCOL];
  int sortvlen[MAXSORTCOL];
  int samelen;

  /* Buffer to store character-array values */
  char valbuf[MAXVALBUF];
  int valbuflen;

  char colname[MAXCOLNAMELENGTH];

  int ucol, uclass;
  char ucolname[MAXCOLNAMELENGTH];
  int ucoltype;
  int nlist;
  void *listvptr[MAXLISTSIZE];
  int listvlen[MAXLISTSIZE];

  char item[MAXITEMLENGTH];
  char qual[MAXQUALLENGTH];
  int quallen;


  /*------ Beginning of code ------*/

  if ( argc != 2 ) {
    PrintUsage(); return 0;
  }

  file = argv[1];

  /*------ Open the file and read in the header info ------*/
  status = MetaioOpen( env, file );
  if ( status != 0 ) {
    printf( "Error opening file %s\n", file );
    MetaioAbort( env ); return 2;
  }

  /*------ Make sure the comment contains the SQL query ------*/
  comment = env->ligo_lw.table.comment;
  if ( comment == NULL ) {
    printf( "Cannot find SQL query in comment in file %s\n", file );
    MetaioAbort( env ); return 2;
  }
  if ( DEBUG ) { printf( "Comment=%s\n", comment ); }
  query = (char *) malloc( strlen(comment) + 1 );
  if ( query == NULL ) {
    printf( "Error allocating memory for copy of query\n" );
    MetaioAbort( env ); return 2;
  }

  /*------ Copy the query text to a new memory location ------*/
  if ( strncmp(comment,"SQL=",4) == 0 ) {
    strcpy( query, comment+4 );
  } else {
    strcpy( query, comment );
  }
  if ( DEBUG ) { printf( "Query=%s\n", query ); }

  /*------ Convert the query to lowercase, and delete repeated spaces ------*/
  querylc = (char *) malloc( strlen(query) + 1 );
  if ( querylc == NULL ) {
    printf( "Error allocating memory for modified query\n" );
    MetaioAbort( env ); return 2;
  }
  cptr = query;
  optr = querylc;
  squote = 0;
  dquote = 0;
  lspace = 1;
  while ( *cptr != '\0' ) {

    /*-- Ignore repeated spaces --*/
    if ( *cptr == ' ' ) {
      if ( lspace ) { cptr++; continue; }
      lspace = 1;
    } else {
      lspace = 0;
    }

    /*-- See if the in-quote vs. out-of-quote status is changing --*/
    if ( *cptr == '\'' ) { squote = 1 - squote; }
    if ( *cptr == '\"' ) { dquote = 1 - dquote; }

    /*-- Copy character to output, converting letters to lowercase (unless we
      are currently within quotes) --*/
    if ( squote || dquote ) {
      *optr = *cptr;
    } else {
      *optr = tolower(*cptr);
    }

    /*-- Increment pointers --*/
    cptr++;
    optr++;
  }

  /*-- Null-terminate the output string --*/
  *optr = '\0';
  if ( DEBUG ) { printf( "Lower-cased query=%s\n", querylc ); }


  /*------ Parse the query to see what columns were used to sort the output
    ------*/
  cptr = strstr( querylc, "order by " );
  nsortcol = 0;
  if ( cptr != NULL ) {
    cptr += 9;

    while ( 1 ) {
      if ( nsortcol >= MAXSORTCOL ) {
	printf( "Error: More than %d order-by columns\n", MAXSORTCOL );
	MetaioAbort( env ); return 2;
      }

      /*-- Skip initial spaces and commas--*/
      while ( *cptr == ' ' || *cptr == ',' ) { cptr++; }
      /*-- Read up to next space or comma --*/
      optr = sortcolname[nsortcol];
      while ( *cptr != ' ' && *cptr != ',' ) {
	*optr = *cptr; cptr++; optr++;
      }

      nsortcol++;

      /*-- Skip spaces --*/
      while ( *cptr == ' ' ) { cptr++; }
      /*-- If next non-blank character is not a comma, exit the loop --*/
      if ( *cptr != ',' ) { break; }
      cptr++;
    }

  }

  if ( DEBUG ) {
    printf("Sorted by %d columns:", nsortcol );
    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      printf( "  %s", sortcolname[isortcol] );
    }
    printf( "\n" );
  }

  free( querylc );

  /*-- There must be at least one order-by column --*/
  if ( nsortcol == 0 ) {
    printf( "Error: Query output must be ordered using at least one column\n" );
    MetaioAbort(env); return 2;
  }

  /*-- Make sure each element in the sorting list is a simple column name --*/
  for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
    icol = MetaioFindColumn( env, sortcolname[isortcol] );
    if ( icol < 0 ) {
      printf( "Error: Order-by column %s is not present in file??\n",
	      sortcolname[isortcol] );
      MetaioAbort(env); return 2;
    }
    sortcoli[isortcol] = icol;
    sortcoltype[isortcol] = env->ligo_lw.table.col[icol].data_type;
  }

  if ( DEBUG ) {
    printf("Column indexes:" );
    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      printf( "  %d", sortcoli[isortcol] );
    }
    printf( "\n" );
  }


  /*------ Try to identify a unique ID also in the table.  There is a hierarchy
    of which unique ID takes precedence. ------*/
  ucol = -1;
  uclass = 0;
  ncols = env->ligo_lw.table.numcols;
  for ( icol=0; icol<ncols; icol++ ) {

    /*-- Copy column name, converting to lowercase --*/
    cptr = env->ligo_lw.table.col[icol].name;
    optr = colname;
    while ( *cptr != '\0' ) { *optr=tolower(*cptr); cptr++; optr++; }
    *optr = '\0';

    /*-- Make sure this column is not already in the order-by list --*/
    flag = 0;
    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      if ( strcmp(colname,sortcolname[isortcol]) == 0 ) { flag=1; break; }
    }
    if ( flag == 1 ) { continue; }

    /*-- See if this column is usable as a "unique id" --*/
    if ( uclass < 4 ) {
      if ( strcmp(colname,"event_id")==0 ||
	   strcmp(colname,"coinc_id")==0 ||
	   strcmp(colname,"sngl_mime_id")==0 ||
	   strcmp(colname,"summ_mime_id")==0 ||
	   strcmp(colname,"summ_comment_id")==0 ) {
	ucol = icol;
	uclass = 4;
	break;
      }
    }

    if ( uclass < 3 ) {
      if ( strcmp(colname,"start_time_ns")==0 ||
	   strcmp(colname,"end_time_ns")==0 ) {
	ucol = icol;
	uclass = 3;
      }
    }

    if ( uclass < 2 ) {
      if ( strcmp(colname,"filter_id")==0 ||
	   strcmp(colname,"chanlist_id")==0 ) {
	ucol = icol;
	uclass = 2;
      }
    }

    if ( uclass < 1 ) {
      if ( strcmp(colname,"process_id")==0 ) {
	ucol = icol;
	uclass = 1;
      }
    }

  }

  if ( ucol < 0 ) {
    /*-- If no other suitable "unique ID" column can be identified, then allow
      event_id to be used as the unique ID --*/
    ucol = MetaioFindColumn( env, "event_id" );
    if ( ucol >= 0 && nsortcol > 1 ) {
      /*-- Remove event_id from the order-by list --*/
      for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
	if ( strcmp( sortcolname[isortcol], "event_id" ) == 0 ) {
	  for ( i=isortcol; i<nsortcol-1; i++ ) {
	    strcpy( sortcolname[i], sortcolname[i+1] );
	    sortcoli[i] = sortcoli[i+1];
	    sortcoltype[i] = sortcoltype[i+1];
	  }
	  nsortcol--;
	}
      }
    }
  }

  if ( ucol < 0 ) {
    printf( "Error: Unable to identify a suitable column to use as a unique ID\n" );
    MetaioAbort(env); return 2;
  }    

  if ( DEBUG ) {
    printf( "Using column %d (%s) as a unique ID\n",
	    ucol, env->ligo_lw.table.col[ucol].name );
  }

  /*-- Store the unique-id column name and data type --*/
  cptr = env->ligo_lw.table.col[ucol].name;
  optr = ucolname;
  while ( *cptr != '\0' ) { *optr=tolower(*cptr); cptr++; optr++; }
  *optr = '\0';
  ucoltype = env->ligo_lw.table.col[ucol].data_type;


  /*------ Now parse the file and keep track of the values ------*/
  if ( DEBUG ) { printf( "Address of valbuf is %p\n", (void *) valbuf );}

  nrows = 0;
  valbuflen = 0;
  while ( (readstatus=MetaioGetRow(env)) == 1 ) {
    nrows++;

    if ( nrows == 1 ) {
      diff = 1;
    } else {
      diff = 0;
    }

    /*-- See if all order-by values are the same as before --*/
    samelen = 0;
    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      icol = sortcoli[isortcol];
      GetPtrAndLen( &(env->ligo_lw.table.elt[icol]), &vptr, &len );

      if ( ! diff ) {
	/*-- Check whether this order-by value differs --*/
	if ( len != sortvlen[isortcol] ) {
	  diff = 1;
	} else if ( memcmp(vptr,sortvptr[isortcol],len) ) {
	  diff = 1;
	}

	/*-- When we first encounter a column that differs, reset the rest of
	  the value buffer --*/
	if ( diff ) {
	  valbuflen = samelen;
	  nlist = 0;
	  if ( DEBUG ) { printf( "different\n" ); }
	}
      }

      /*-- If different, store the new value --*/
      if ( diff ) {
	if ( valbuflen+len > MAXVALBUF ) {
	  printf( "Error: Buffer overflow while storing order-by column %d\n", icol );
	  MetaioAbort(env); return 2;
	}
	if ( DEBUG ) {
	  printf( "Storing %d bytes at address %p for order-by column %d\n",
		  len, (void *)(valbuf+valbuflen), icol );
	}
	memcpy( valbuf+valbuflen, vptr, len );
	sortvptr[isortcol] = valbuf+valbuflen;
	sortvlen[isortcol] = len;
	valbuflen += len;
      } else {
	samelen += len;
      }

    } /*-- End loop over order-by columns --*/

    if ( DEBUG ) { if ( ! diff ) printf("same\n"); }

    /*-- Add this unique-id value to the list --*/
    if ( nlist >= MAXLISTSIZE ) {
      printf( "Error: More than %d entries with identical values in all order-by columns\n", MAXLISTSIZE );
      MetaioAbort(env); return 2;
    }

    GetPtrAndLen( &(env->ligo_lw.table.elt[ucol]), &vptr, &len );

    if ( valbuflen+len > MAXVALBUF ) {
      printf( "Error: Buffer overflow while storing unique-ID value (count=%d)\n",
	      nlist+1 );
      MetaioAbort(env); return 2;
    }
    if ( DEBUG ) {
      printf( "Storing %d bytes at address %p for unique-id column %d\n",
	      len, (void *)(valbuf+valbuflen), ucol );
    }
    memcpy( valbuf+valbuflen, vptr, len );
    listvptr[nlist] = valbuf+valbuflen;
    listvlen[nlist] = len;
    valbuflen += len;
    nlist++;

  } /*-- End loop over rows in file --*/

  if ( readstatus == -1 ) {
    printf( "Error parsing file at row %d\n", nrows+1 );
    printf( "%s", env->mierrmsg.data );
  }

  /*-- Close the file --*/
  MetaioClose(env);

  if ( DEBUG ) {

    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      icol = sortcoli[isortcol];
      printf( "At end, column %d is ", icol );
      HackPrintVal( sortcoltype[isortcol],
		    sortvptr[isortcol], sortvlen[isortcol] );
      printf("\n");
    }

    printf( "Values are:" );
    for ( ilist=0; ilist<nlist; ilist++ ) {
      printf( "  " );
      HackPrintVal( ucoltype, listvptr[ilist], listvlen[ilist] );
    }
    printf( "\n" );
  }

  /*------ If there are fewer than 1000 rows in the file, then we don't need
    to construct a follow-up query ------*/
  if ( nrows < 1000 ) {
    strcpy( qual, "" );

  } else {

    /*------ Construct the query qualifier ------*/
    if ( nlist == nrows ) {
      printf( "Error: All rows in file have identical values in all order-by columns\n" );
      return 2;
    }

    qptr = qual;

    for ( isortcol=0; isortcol<nsortcol; isortcol++ ) {
      icol = sortcoli[isortcol];

      status = FormatVal( item, MAXITEMLENGTH, sortcoltype[isortcol],
			  sortvptr[isortcol], sortvlen[isortcol] );
      if ( status ) {
	printf( "Error: Item in query qualifier is longer than %d bytes\n",
		MAXITEMLENGTH );
	return 2;
      }
      sprintf( qptr, "(%s>%s Or (%s=%s And ",
	       sortcolname[isortcol], item, sortcolname[isortcol], item );
      qptr += strlen( qptr );
    }
  
    sprintf( qptr, "%s not in (", ucolname );
    len = strlen( qptr );
    qptr += len;
    quallen += len;

    for ( ilist=0; ilist<nlist; ilist++ ) {
      status = FormatVal( item, MAXITEMLENGTH, ucoltype,
			  listvptr[ilist], listvlen[ilist] );
      if ( status ) {
	printf( "Error: Item in query qualifier is longer than %d bytes\n",
		MAXITEMLENGTH );
	return 2;
      }
      if ( ilist > 0 ) { strcpy(qptr,","); qptr++; quallen++; }
      /*-- Make sure there is room in the output string --*/
      len = strlen( item );
      if ( quallen+len > MAXQUALLENGTH-50 ) {
	printf( "Error: Query qualifier is longer than ~%d bytes\n",
		MAXQUALLENGTH );
	return 2;
      }

      strcpy( qptr, item );
      qptr += len;
      quallen += len;
    }

    for ( i=0; i<2*nsortcol+1; i++ ) {
      strcpy( qptr, ")" );
      qptr++;
    }

  }

  /*------ Write the results to stdout ------*/
  printf( "OK\n" );
  printf( "%s\n", query );
  printf( "%d rows\n", nrows );
  printf( "%s\n", qual );

  free( query );

  return 0;
}
