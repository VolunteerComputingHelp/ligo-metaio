/*=============================================================================
lwtscan - A utility to scan a LIGO_LW table file and print some basic info
Written Feb 2001 by Peter Shawhan
Uses the "Metaio" parsing code by Philip Charlton
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "metaio.h"

#define MYDEBUG

/*===========================================================================*/
void PrintUsage( void )
{
  printf( "Usage: lwtscan <file> [-t <table>]\n" );
  printf( "<table> lets you scan a particular table from a file"
	  " containing multiple\ntables.  If omitted, the first table"
	  " in the file is scanned.\n" );
  return;
}


/*===========================================================================*/
int main( int argc, char **argv )
{
  int iarg, icol, status;
  char *arg, *val;
  char opt='\0';
  char *file=NULL;
  char *tablename=NULL;
  size_t vallen;
  int nvals;
  int nrows=0;

  struct MetaioParseEnvironment parseEnv;
  const MetaioParseEnv env = &parseEnv;

  /*------ Beginning of code ------*/

  if ( argc <= 1 ) {
    PrintUsage(); return 0;
  }

  /*-- Parse command-line arguments --*/
  for ( iarg=1; iarg<argc; iarg++ ) {
    arg = argv[iarg];
    if ( strlen(arg) == 0 ) { continue; }

    /*-- See whether this introduces an option --*/
    if ( arg[0] == '-' && arg[1] != '\0' ) {
      /*-- There are no valid multi-letter options, so the rest of the
	argument (if any) must be the value associated with the option --*/
      opt = arg[1];
      if ( strlen(arg) > 2 ) {
	val = arg+2;
	vallen = strlen(arg) - 2;
      } else {
	val = NULL;
	vallen = 0;
      }
      nvals = 0;
    } else {
      val = arg;
      vallen = strlen(val);
      nvals++;
    }

    switch (opt) {

    case '\0':   /*-- Filename --*/
      if ( file != NULL && val != NULL ) {
	printf( "Error: multiple filenames specified: %s & %s\n", file, val );
	PrintUsage(); return 1;
      }
      file = val;
      break;

    case 't':   /*-- Table name --*/
      if ( tablename != NULL && val != NULL ) {
	printf( "Error: multiple table names specified: %s & %s\n",
		tablename, val );
	PrintUsage(); return 1;
      }
      tablename = val;
      if ( val != NULL ) { opt = '\0'; }
      break;

    default:
      printf( "Invalid option -%c\n", opt );
      PrintUsage(); return 1;
    }

  }

  /*-- Make sure a file was specified --*/
  if ( file == NULL ) {
    printf( "No file was specified\n" );
    PrintUsage(); return 1;
  }


  /*-- Open the file --*/
  status = MetaioOpenTable( env, file, tablename );
  if ( status != 0 ) {
    if ( tablename == NULL ) {
      printf( "Error opening file %s\n", file );
    } else {
      printf( "Error opening table %s in file %s\n", tablename, file );
    }
    printf( "%s\n", env->mierrmsg.data );
    MetaioAbort( env );
    return 2;
  }

  /*-- Print out the table name --*/
  if ( env->ligo_lw.table.name != 0 ) {
    if ( strlen(env->ligo_lw.table.name) > 0 ) {
      printf( "Table %s\n", env->ligo_lw.table.name );
    }
  }

  /*-- Print out the comment, if any --*/
  if ( env->ligo_lw.comment != 0 ) {
    if ( strlen(env->ligo_lw.comment) > 0 ) {
      printf( "Comment: %s\n", env->ligo_lw.comment );
    }
  }

  /*-- Print out column names and types --*/
  for ( icol=0; icol < env->ligo_lw.table.numcols; icol++ ) {
    printf ( "  %-20s %s\n",
	     MetaioColumnName( env, icol ),
	     MetaioTypeText( env->ligo_lw.table.col[icol].data_type )
	     );
  }

  /*-- Loop over rows in the file --*/
  while ( (status=MetaioGetRow(env)) == 1 ) {
    nrows++;
  }    /*-- End of loop over rows in the file --*/

  if ( status == -1 ) {
    printf( "Parsing error at row %d\n", nrows+1 );
    printf( "%s\n", env->mierrmsg.data );
  } else {
    /*-- Print out number of rows --*/
    printf( "%d rows\n", nrows );
  }

  MetaioClose(env);

  return 0;
}
