/*=============================================================================
lwtdiff - A utility to compare two LIGO_LW table files
Written Feb 2001 by Peter Shawhan
Uses the "Metaio" parsing code by Philip Charlton
Modified Sep 2001 by Peter Shawhan: Add "-x" option
=============================================================================*/

#include <stdio.h>
#include <string.h>
#include "metaio.h"

#define MYDEBUG

/*===========================================================================*/
void PrintUsage( void )
{
  printf( "Usage:  lwtdiff <file1> <file2> [-c <colspec>] [-x <colspec>] [-t <table>]\n" );
  printf( "  Compares two LIGO_LW table files, allowing for formatting variations which\n" );
  printf( "  do no affect the information content.\n" );
  printf( "<colspec> can be a column name, or a list of column names separated by commas\n" );
  printf( "    and/or spaces.  Column names are case-insensitive.  If the '-c' option is\n" );
  printf( "    used, then only the specified columns are compared between the two files.\n" );
  printf( "    If a specified column is missing from both files, then an error message\n" );
  printf( "    will be printed.  If the '-x' option is used, then the specified columns\n" );
  printf( "    will NOT be compared (even if specified with '-c').  It is not an error\n" );
  printf( "    to specify a nonexistent column after '-x'; it simply has no effect.\n" );
  printf( "<table> lets you specify (by name) a particular table to be compared, in case\n" );
  printf( "    the files contain multiple tables.  If omitted, the first tables in the\n" );
  printf( "    files are compared\n" );

  return;
}

/*===========================================================================*/
int main( int argc, char **argv )
{
  int iarg;
  char *arg, *val;
  char opt='\0';
  int retval = 0;
  char *file1=NULL, *file2=NULL;
  char *tablename=NULL;
  char colspec[1024], colexcl[1024];
  size_t vallen;
  int colspeclen=-1;  /*-- Special value -1 means compare all columns --*/
  int colexcllen=0;
  int nvals;
  char *vptr, *dptr, dsave;
  int icol, jcol, irow, idiff;
  int tndiff, ndiff, firstdiff;
  int changed;
  int tdifflist[METAIOMAXCOLS], difflist[METAIOMAXCOLS];
  int stat1, stat2, status;
  char *icptr, *jcptr;
  int initval;
  int imatch[METAIOMAXCOLS], jmatch[METAIOMAXCOLS];

  struct MetaioParseEnvironment parseEnv1, parseEnv2;
  const MetaioParseEnv env1 = &parseEnv1;
  const MetaioParseEnv env2 = &parseEnv2;

  /*------ Beginning of code ------*/

  colspec[0] = '\0'; 
  colexcl[0] = '\0';

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

    /*-- If this is the last or second-to-last argument, and one or both
      filenames (respectively) has not yet been specified, and this argument
      is not unambiguously a column spec (i.e. it does not immediately follow
      -c or -x and does not contain any commas or spaces), then force it to be
      interpreted as a filename. --*/
    if ( ( (iarg == (argc-2) && file1 == NULL) ||
	   (iarg == (argc-1) && file2 == NULL)   )
	 && nvals > 1 && strpbrk(val,", ") == NULL ) {
      if ( opt == 'c' || opt == 'x' ) { opt = '\0'; }
    }

    switch (opt) {

    case '\0':   /*-- Filename --*/
      if ( file2 != NULL && val != NULL ) {
	printf( "Error: more than two filenames specified\n" );
	PrintUsage(); return 1;
      }
      if ( file1 == NULL ) {
	file1 = val;
      } else {
	file2 = val;
      }
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

    case 'c':    /*-- Column specification --*/
      if ( colspeclen == -1 ) { colspeclen = 0; }
      if ( val == NULL ) { break; }
      if ( colspeclen+vallen > sizeof(colspec)-2 ) {
	printf( "Error: column specification is too long (max %d bytes)\n",
		(int)sizeof(colspec)-1 );
	return 1;
      }
      if ( colspeclen > 0 ) {
	colspec[colspeclen] = ' ';
	colspeclen++;
	colspec[colspeclen] = '\0';
      }
      strcat( (colspec+colspeclen), val );
      colspeclen += vallen;
      break;

    case 'x':    /*-- Columns to EXCLUDE --*/
      if ( val == NULL ) { break; }
      if ( colexcllen+vallen > sizeof(colexcl)-2 ) {
	printf( "Error: column exclusion list is too long (max %d bytes)\n",
		(int)sizeof(colexcl)-1 );
	return 1;
      }
      if ( colexcllen > 0 ) {
	colexcl[colexcllen] = ' ';
	colexcllen++;
	colexcl[colexcllen] = '\0';
      }
      strcat( (colexcl+colexcllen), val );
      colexcllen += vallen;
      break;

    default:
      printf( "Invalid option -%c\n", opt );
      PrintUsage(); return 1;
    }

  }

  /*-- Make sure both files were specified --*/
  if ( file1 == NULL ) {
    printf( "No file was specified\n" );
    PrintUsage(); return 1;
  } else if ( file2 == NULL ) {
    printf( "Only one file was specified\n" );
    PrintUsage(); return 1;
  }

  status = MetaioOpenTable( env1, file1, tablename );
  if ( status != 0 ) {
    if ( tablename == NULL ) {
      printf( "Error opening file %s\n", file1 );
    } else {
      printf( "Error opening table %s in file %s\n", tablename, file1 );
    }
    MetaioAbort( env1 );
    return 2;
  }

  status = MetaioOpenTable( env2, file2, tablename );
  if ( status != 0 ) {
    if ( tablename == NULL ) {
      printf( "Error opening file %s\n", file2 );
    } else {
      printf( "Error opening table %s in file %s\n", tablename, file2 );
    }
    MetaioAbort( env1 );
    MetaioAbort( env2 );
    return 2;
  }

  /*-- Initialize "match" indexes for all columns in both tables --*/
  if ( colspeclen == -1 ) {
    initval = -1;   /* Means 'no match' */
  } else {
    initval = -2;   /* Means 'ignore this column' */
  }
  for ( icol=0; icol < env1->ligo_lw.table.numcols; icol++ ) {
    imatch[icol] = initval;
  }
  for ( jcol=0; jcol < env2->ligo_lw.table.numcols; jcol++ ) {
    jmatch[jcol] = initval;
  }

  /*-- Determine "match" index for each column to compare --*/
  if ( colspeclen == -1 ) {
    /*-- Compare all columns (except those specified with '-x') --*/

    for ( icol=0; icol < env1->ligo_lw.table.numcols; icol++ ) {
      icptr = MetaioColumnName( env1, icol );
      for ( jcol=0; jcol < env2->ligo_lw.table.numcols; jcol++ ) {
	jcptr = MetaioColumnName( env2, jcol );
	if ( strcasecmp(icptr,jcptr) == 0 ) {
	  imatch[icol] = jcol;
	  jmatch[jcol] = icol;
	}
      }
    }

  } else {
    /*-- Only compare the columns specified with '-c' --*/

    /*-- Loop over tokens separated by commas and/or spaces.
      Operate on the string in place, rather than copying it. --*/
    vptr = colspec;    /*-- vptr points to the current token --*/
    do {
      /*-- Find the position of the next delimiter (if any) --*/
      dptr = strpbrk( vptr, ", " );
      if ( dptr != NULL ) {
	/*-- If the first character is a delimiter, just continue --*/
	if ( dptr == vptr ) { vptr++; continue; }

	/*-- Temporarily replace the delimiter with a null character,
	  to form a null-terminated token --*/
	dsave = *dptr;
	*dptr = '\0';
      }

      if ( strlen(vptr) > 0 ) {

	/*-- Find the index of the column with the matching name --*/
	icol = MetaioFindColumn( env1, vptr );
	jcol = MetaioFindColumn( env2, vptr );
	if ( icol >= 0 ) {
	  if ( jcol >= 0 ) {
	    imatch[icol] = jcol;
	    jmatch[jcol] = icol;
	  } else {
	    /* In file1, but not in file2 */
	    imatch[icol] = -1;
	  }
	} else if ( jcol >= 0 ) {
	  /* In file2, but not in file1 */
	  jmatch[jcol] = -1;
	} else {
	  /* Not in either file */
	  printf( "Error: column '%s' is not in either input file\n", vptr );
	  return 1;
	}

      }

      if ( dptr != NULL ) {
	/*-- Restore the delimiter and get ready to parse next token --*/
	*dptr = dsave;
	vptr = dptr + 1;
      }

    } while ( dptr != NULL );

  }


  /*-- Modify indexes to remove columns marked by the user for exclusion --*/
  if ( colexcllen > 0 ) {
    vptr = colexcl;    /*-- vptr points to the current token --*/
    do {
      /*-- Find the position of the next delimiter (if any) --*/
      dptr = strpbrk( vptr, ", " );
      if ( dptr != NULL ) {
	/*-- If the first character is a delimiter, just continue --*/
	if ( dptr == vptr ) { vptr++; continue; }

	/*-- Temporarily replace the delimiter with a null character,
	  to form a null-terminated token --*/
	dsave = *dptr;
	*dptr = '\0';
      }

      if ( strlen(vptr) > 0 ) {
	/*-- Find the index of the column, and mark it to be ignored --*/
	icol = MetaioFindColumn( env1, vptr );
	jcol = MetaioFindColumn( env2, vptr );
	if ( icol >= 0 ) {
	  imatch[icol] = -2;
	}
	if ( jcol >= 0 ) {
	  jmatch[jcol] = -2;
	}
      }

      if ( dptr != NULL ) {
	/*-- Restore the delimiter and get ready to parse next token --*/
	*dptr = dsave;
	vptr = dptr + 1;
      }

    } while ( dptr != NULL );

  }


  /*-- Print note about any column in one file but not in the other, unless
    that column is marked to be ignored (i.e. match index == -2) --*/
  for ( icol=0; icol < env1->ligo_lw.table.numcols; icol++ ) {
    if ( imatch[icol] == -1 ) {
      printf( "< extra column: %s\n", MetaioColumnName(env1,icol) );
      retval = 0;
    }
  }

  for ( jcol=0; jcol < env2->ligo_lw.table.numcols; jcol++ ) {
    if ( jmatch[jcol] == -1 ) {
      printf( "> extra column: %s\n", MetaioColumnName(env2,jcol) );
      retval = 0;
    }
  }

  /*--------------------------------------*/
  /*-- Main loop over rows in each file --*/

  irow = 0;
  ndiff = 0;

  while ( 1 ) {

    /*-- Try to read a row from each file --*/
    stat1 = MetaioGetRow(env1);
    stat2 = MetaioGetRow(env2);

    irow++;

    if ( stat1==1 && stat2==1 ) {
      /*-- Successfully read lines from both files --*/

      /*-- Compare all values in this row (for columns in both files) --*/
      tndiff = 0;
      for ( icol=0; icol < env1->ligo_lw.table.numcols; icol++ ) {
	jcol = imatch[icol];
	if ( jcol >= 0 ) {
	  status = MetaioCompareElements( &(env1->ligo_lw.table.elt[icol]),
					  &(env2->ligo_lw.table.elt[jcol]) );
	  if ( status == 2 ) {
	    printf( "! column has incompatible types: %s\n",
		    MetaioColumnName(env1,icol) );
	    /*-- Remove this column from the comparison list --*/
	    imatch[icol] = -1;
	    jmatch[jcol] = -1;
	  } else if ( status != 0 ) {
	    tdifflist[tndiff] = icol;
	    tndiff++;
	  }
	}
      }

    } else {
      /*-- Set tndiff to a special value to force message to be printed --*/
      tndiff = -1;
    }

    /*-- If there has been a change in the difference list, print out
      what the difference list was up to this point --*/
    if ( tndiff != ndiff ) {
      changed = 1;
    } else {
      changed = 0;
      for ( idiff=0; idiff < ndiff; idiff++ ) {
	if ( tdifflist[idiff] != difflist[idiff] ) { changed = 1; }
      }
    }

    if ( changed ) {
      /*-- Print out a description of the difference (if any) --*/
      if ( ndiff > 0 ) {
	if ( irow-1 == firstdiff ) {
	  printf( "! row %d", irow-1 );
	} else {
	  printf( "! rows %d-%d", firstdiff, irow-1 );
	}
	printf( " (%s", MetaioColumnName(env1,difflist[0]) );
	for ( idiff=1; idiff < ndiff; idiff++ ) {
	  printf( ",%s", MetaioColumnName(env1,difflist[idiff]) );
	}
	printf( ")\n" );
	retval = 1;
      }

      /*-- Store the new difference list --*/
      firstdiff = irow;
      ndiff = tndiff;
      for ( idiff=0; idiff < ndiff; idiff++ ) {
	difflist[idiff] = tdifflist[idiff];
      }
    }

    /*-- Handle error(s) and/or end-of-file(s) --*/

    switch ( 10*stat1 + stat2 ) {
    case -11:
      printf( "! parsing errors at row %d\n", irow );
      printf( "%s\n", env1->mierrmsg.data );
      printf( "%s\n", env2->mierrmsg.data );
      retval = 2; break;
    case -10:
    case -9:
      printf( "< parsing error at row %d\n", irow );
      printf( "%s\n", env1->mierrmsg.data );
      retval = 2; break;
    case -1:
    case 9:
      printf( "> parsing error at row %d\n", irow );
      printf( "%s\n", env2->mierrmsg.data );
      retval = 2; break;
    case 0:
    case 11:
      break;
    case 1:
      printf( "> extra row(s)\n" );
      retval = 1; break;
    case 10:
      printf( "< extra row(s)\n" );
      retval = 1; break;
    }

    /*-- If either file ended or had a parsing error, break --*/
    if ( stat1 != 1 || stat2 != 1 ) { break; }

  }

  MetaioClose(env1);
  MetaioClose(env2);

  return retval;
}
