/*=============================================================================
lwtprint - A utility to print out selected element(s) from a LIGO_LW table file
Written Feb 2001 by Peter Shawhan
Uses the "Metaio" parsing code by Philip Charlton
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "metaio.h"

#define PSEUDOCOL_ROW -99

/*===========================================================================*/
void PrintUsage( void )
{
  printf( "Usage: lwtprint <file> [-r <rowspec>] [-c <colspec>] [-x <colspec>] [-t <table>]\n" );
  printf( "                       [-d <delim>] [-u float]\n" );
  printf( "<rowspec> can be a number, a number range separated by a hyphen (e.g. '11-20'),\n" );
  printf( "    or a list of numbers and/or ranges separated by commas and/or spaces.\n" );
  printf( "    Rows are numbered starting with 1.  Rows may be specified in any order,\n" );
  printf( "    but will always be printed in the order in which they appear in the file.\n" );
  printf( "    A given row will be printed at most once.  A specification of the form\n" );
  printf( "    '11-' means to print from row 11 through the end of the file.\n" );
  printf( "    If there is no row specification, then all rows will be printed.\n" );
  printf( "<colspec> can be a column name, or a list of column names separated by commas\n" );
  printf( "    and/or spaces.  Column names are case-insensitive.  If the '-c' option is\n" );
  printf( "    used, then the specified columns' values are printed in the specified order\n" );
  printf( "    (regardless of their order in the file).  The special column name of '%%row'\n" );
  printf( "    causes the row number to be printed.  If the '-c' option is not used, then\n" );
  printf( "    all columns will be printed; however, any columns specified after the\n" );
  printf( "    '-x' option will NOT be printed (even if specified with '-c').  It is not\n" );
  printf( "    an error to specify a nonexistent column after the '-x', but has no effect.\n" );
  printf( "<table> lets you specify (by name) a particular table to read from a file\n" );
  printf( "    containing multiple tables.  If omitted, the first table in the file is read\n" );
  printf( "<delim> is the delimiter character (or short string) to use between the values\n" );
  printf( "    printed out.  By default, this is a comma.  Specify 't' to get a tab.\n" );
  printf( "If '-u float' is specified, binary data is interpreted as an array of floats\n" );
  printf( "    and is printed, one floating-point value per line.\n" );
  return;
}


/*===========================================================================*/
int main( int argc, char **argv )
{
  char *arg, *val;
  char opt='\0';
  char *file=NULL;
  char *tablename=NULL;
  char colspec[1024], colexcl[1024];
  size_t vallen;
  int colspeclen=-1;  /*-- Special value -1 means print all columns --*/
  int colexcllen=0;
  int nvals=0;
  int valid, status;
  int rsmin[1024], rsmax[1024];   /*-- Row specification list --*/
  int nrs=-1; /*-- Number of row ranges; special value -1 means all rows --*/
  char *vptr, *dptr, *hptr, *tptr, *endptr1, *endptr2;
  char spaceString[] = " ";
  char dsave;
  char delim[16] = ",";    /*-- Delimiter string for output --*/
  char* char_u_format = "";
  int istart, iend, iovr1, iovr2;
  int delta, irange;
  int ncols=0, collist[256];
  int iarg, irow, i, j, icol, excol, active, target;

  struct MetaioParseEnvironment parseEnv;
  const MetaioParseEnv env = &parseEnv;

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

    /*-- If this is the last argument, and the filename has not yet been
      specified, and this argument is not unambiguously a row spec or
      column spec (i.e. it does not immediately follow -c or -x or -r and does
      not contain any commas or spaces, and it does not look like a valid
      row specification), then force it to be interpreted as the filename.
      --*/
    if ( iarg == (argc-1) && file == NULL && nvals > 1
	 && strpbrk(val,", ") == NULL ) {
      if ( opt == 'c' || opt == 'x' ) { opt = '\0'; }
      if ( opt == 'r' && strspn(val,"0123456789-, ") != vallen ) {
	opt = '\0';
      }
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

    case 'r':    /*-- Row specification --*/
      if ( nrs == -1 ) { nrs = 0; }
      if ( val == NULL ) { break; }

      /*-- Loop over tokens separated by commas and/or spaces.
	Operate on the string in place, rather than copying it. --*/
      vptr = val;    /*-- vptr points to the current token --*/
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

	valid = 1;

	/*-- Check for a hyphen in the token --*/
	hptr = strchr( vptr, '-' );
	if ( hptr != NULL ) {
	  /*-- Temporarily replace the hyphen with a null character --*/
	  *hptr = '\0';
	}

	/*-- Parse the values on the left side of the hyphen (or the only
	  value, if there is no hyphen).  Note that strtol() will return 0
	  if no conversion can be performed, which will lead to an error
	  later on. --*/
	if ( strlen(vptr) > 0 ) {
	  istart = strtol( vptr, &endptr1, 10 );
	  if ( *endptr1 != '\0' ) { valid = 0; }
	} else {
	  istart = 1;
	  /*-- Set endptr1 to some arbitrary non-null memory location --*/
	  endptr1 = spaceString;
	}

	if ( hptr != NULL ) {
	  /*-- Parse the value to the right of the hyphen --*/
	  if ( strlen(hptr+1) > 0 ) {
	    iend = strtol( (hptr+1), &endptr2, 10 );
	    if ( *endptr2 != '\0' ) { valid = 0; }
	  } else {
	    iend = 1000000000;
	    /*-- Set endptr2 to some arbitrary non-null memory location --*/
	    endptr2 = spaceString;
	  }
	  /*-- Restore the hyphen --*/
	  *hptr = '-';
	} else {
	  /*-- Use same value for both start and end of the range --*/
	  iend = istart;
	  endptr2 = endptr1;
	}

	/*-- Check that the range looks reasonable --*/
	if ( istart <= 0 || iend <= 0 || iend < istart ) { valid = 0; }

	if ( valid == 0 ) {
	  printf( "Invalid row number or range: %s\n", vptr );
	  PrintUsage(); return 1;
	}
	    
	/*-- OK, now we have valid start and end row numbers --*/

	/*-- Figure out which existing ranges overlap with this new one
	  (as INTEGER ranges; reals would use slightly different logic) --*/
	for ( iovr2=nrs-1; iovr2 >= 0; iovr2-- ) {
	  if ( iend >= rsmin[iovr2] - 1 ) { break; }
	}
	for ( iovr1=iovr2+1; iovr1 > 0; iovr1-- ) {
	  if ( istart > rsmax[iovr1-1] + 1 ) { break; }
	}

	/*-- Now update list of ranges --*/
	delta = iovr1 - iovr2;   /*-- +1, 0, or a negative number --*/
	if ( delta == 1 ) {

	  if ( (unsigned int)nrs >= sizeof(rsmax)/sizeof(int) ) {
	    printf( "Row specification has too many disjoint intervals (max %d)\n",
		    (int)(sizeof(rsmax)/sizeof(int)) );
	    return 1;
	  }

	  /*-- Insert this new range into the stack at the proper place --*/
	  for ( irange=nrs; irange>iovr1; irange-- ) {
	    rsmin[irange] = rsmin[irange-1];
	    rsmax[irange] = rsmax[irange-1];
	  }
	  rsmin[iovr1] = istart;
	  rsmax[iovr1] = iend;
	  nrs++;

	} else {

	  /*-- Update the range at index iovr1 --*/
	  if ( istart < rsmin[iovr1] ) { rsmin[iovr1] = istart; }
	  if ( iend > rsmax[iovr1] ) { rsmax[iovr1] = iend; }
	  if ( rsmax[iovr2] > rsmax[iovr1] ) { rsmax[iovr1] = rsmax[iovr2]; }

	  if ( delta < 0 ) {
	    /*-- Collapse the stack (due to ranges being subsumed) --*/
	    for ( irange=iovr1+1; irange<nrs+delta; irange++ ) {
	      rsmin[irange] = rsmin[irange-delta];
	      rsmax[irange] = rsmax[irange-delta];
	    }
	    nrs += delta;    /*-- (delta is <= 0) --*/
	  }

	}

	if ( dptr != NULL ) {
	  /*-- Restore the delimiter and get ready to parse next token --*/
	  *dptr = dsave;
	  vptr = dptr + 1;
	}

      } while ( dptr != NULL );

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

    case 'd':    /*-- Delimiter for output --*/
      if ( val == NULL ) { break; }
      if ( strlen(val) > sizeof(delim)-1 ) {
	printf( "Error: delimiter string is too long (max %d bytes)\n",
		(int)sizeof(delim)-1 );
	return 1;
      }
      strcpy( delim, val );
      /*-- Replace 't' with a tab character --*/
      while ( (tptr=strchr(delim,'t')) != NULL ) { *tptr = '\t'; }
      opt = '\0';
      break;

    case 'u':    /*-- Format to map CHAR_U* to --*/
	if ( val == NULL ) { break; }
        if ( strcmp(val, "float") == 0 ) {
	    char_u_format = "float";
	}
	else {
	  printf( "Error: invalid char_u format %s\n", val);
	  return 1;
	}
	opt = '\0';
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


  /*hack*/
  /*
  printf( "File is %s\n", file );
  printf( "Row specification is:\n" );
  for ( irange=0; irange < nrs; irange++ ) {
    printf( "%6d  to %6d\n", rsmin[irange], rsmax[irange] );
  }
  printf( "Col specification is %s\n", colspec );
  */


  /*-- Open the file --*/
  status = MetaioOpenTable( env, file, tablename );
  if ( status != 0 ) {
    if ( tablename == NULL ) {
      printf( "Error opening file %s\n", file );
      printf( "%s\n", env->mierrmsg.data );
    } else {
      printf( "Error opening table %s in file %s\n", tablename, file );
      printf( "%s\n", env->mierrmsg.data );
    }
    MetaioAbort( env );
    return 2;
  }

  /*-- Determine column index for each specified column to print --*/
  if ( colspeclen == -1 ) {
    /*-- Print all columns (except those specified with '-x') --*/

    ncols = env->ligo_lw.table.numcols;
    if ( (unsigned int)ncols >= sizeof(collist)/sizeof(int) ) {
      printf( "Too many columns specified (max %d)\n",
	      (int)(sizeof(collist)/sizeof(int)) );
      MetaioAbort(env);
      return 1;
    }
    for ( icol=0; icol < ncols; icol++ ) {
      collist[icol] = icol;
    }

  } else {

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

	/*-- Check for a special pseudo-filename --*/
	if ( strcasecmp( vptr, "%row" ) == 0 ) {
	  icol = PSEUDOCOL_ROW;
	} else {
	  /*-- Find the index of the column with the matching name --*/
	  icol = MetaioFindColumn( env, vptr );
	  if ( icol < 0 ) {
	    printf( "Column does not exist: %s\n", vptr );
	    MetaioAbort(env);
	    return 1;
	  }
	}

	if ( (unsigned int)ncols >= sizeof(collist)/sizeof(int) ) {
	  printf( "Too many columns specified (max %d)\n",
		  (int)(sizeof(collist)/sizeof(int)) );
	  MetaioAbort(env);
	  return 1;
	}

	/*-- Append this column index to the list --*/
	collist[ncols] = icol;
	ncols++;

      }

      if ( dptr != NULL ) {
	/*-- Restore the delimiter and get ready to parse next token --*/
	*dptr = dsave;
	vptr = dptr + 1;
      }

    } while ( dptr != NULL );

  }    /*-- End of if (colspeclen==0) --*/


  /*-- Modify list to remove columns marked by the user for exclusion --*/
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
	/*-- Find the index of the column with the matching name --*/
	excol = MetaioFindColumn( env, vptr );
	if ( excol >= 0 ) {
	  /*-- Remove this column from the list, if it is there --*/
	  for ( i=0; i < ncols; i++ ) {
	    while ( collist[i] == excol ) {
	      for ( j=i; j < ncols-1; j++ ) {
		collist[j] = collist[j+1];
	      }
	      ncols--;
	      if ( i >= ncols ) { break; }
	    }
	  }
	}
      }

      if ( dptr != NULL ) {
	/*-- Restore the delimiter and get ready to parse next token --*/
	*dptr = dsave;
	vptr = dptr + 1;
      }

    } while ( dptr != NULL );

  }


  /*-- Loop over rows in the file --*/

  irow = 0;
  if ( nrs == -1 ) {
    active = 1;
    target = 1000000000;
  } else if ( nrs == 0 ) {
    active = 1;
    target = 1;
  } else {
    active = 0;
    target = rsmin[0];
    irange = 0;
  }

  while ( (status=MetaioGetRow(env)) == 1 ) {
    irow++;

    if ( irow == target ) {
      if ( active == 0 ) {
	active = 1;
	target = rsmax[irange] + 1;
      } else {
	active = 0;
	irange++;
	if ( irange >= nrs ) {
	  /*-- Immediately close the input file without reading the rest --*/
	  MetaioAbort( env );
	  break;    /*-- We don't want any more rows from this file --*/
	}
	target = rsmin[irange];
      }
    }

    if ( active ) {
      for ( i=0; i<ncols; i++ ) {
	icol = collist[i];
	if ( i > 0 )
          printf( delim );
	if ( icol == PSEUDOCOL_ROW )
	  fprintf( stdout, "%d", irow );
	else
          MetaioFprintElement( stdout, &(env->ligo_lw.table.elt[icol]) );
      }
      printf( "\n" );
    }

  }    /*-- End of loop over rows in the file --*/

  if ( status == -1 ) {
    printf( "Parsing error at row %d\n", irow+1 );
    printf( "%s", env->mierrmsg.data );
  }

  /*-- Read to the end of the file, then close is --*/
  MetaioClose(env);

  return 0;
}
