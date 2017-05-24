/*=============================================================================
lwtcut - Select certain rows from a LIGO_LW table file
Written 4 Nov 2003 by Peter Shawhan
Uses the "Metaio" parsing code by Philip Charlton
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "metaio.h"

/*===========================================================================*/
void PrintUsage( int flag )
{
  printf( "Usage: lwtcut <infile> [-t <table>] [<condition>] [-r <rowspec>] [-o <outfile>]\n" );
  if ( flag == 0 ) {
    printf( "Type 'lwtcut' without arguments for full usage information\n" );
    return;
  }

  printf( "This utility lets you select a subset of rows from a LIGO_LW table file, based\n" );
  printf( "    on row number and/or a numeric or string comparison involving one of the\n" );
  printf( "    column values.  The number of matching rows is printed to standard output,\n" );
  printf( "    and (optionally) the matching rows are written to a new LIGO_LW file.\n" );
  printf( "<infile> must be a LIGO_LW file containing one or more Table objects.\n" );
  printf( "<table> lets you specify (by name) a particular table to read from the input\n" );
  printf( "    file, which is useful if the file contains multiple tables.  If omitted,\n" );
  printf( "    then the first table in the file is read.\n" );
  printf( "<rowspec> can be a number, a number range separated by a hyphen (e.g. '11-20'),\n" );
  printf( "    or a list of numbers and/or ranges separated by commas and/or spaces.\n" );
  printf( "    Rows are numbered starting with 1.  Rows may be specified in any order,\n" );
  printf( "    but will always be handled in the order in which they appear in the input\n" );
  printf( "    file.  A given row will be handled at most once.  A specification of the\n" );
  printf( "    form '11-' means to handle row 11 through the end of the file.\n" );
  printf( "    If there is no row specification, then all rows will be handled.\n" );
  printf( "<condition> must have the form of a column name (not case sensitive), followed\n" );
  printf( "    by a comparison operator (one of:  <  ==  >  <=  >=  <>  !=  ), followed by\n" );
  printf( "    a numeric constant or a string.  Only the '==' and '!=' operators are\n" );
  printf( "    permitted for string comparisons.  Generally, you should enclose the entire\n" );
  printf( "    condition in quotes to prevent the shell from interpreting the operator as\n" );
  printf( "    an i/o redirection.  (The string in a string comparison can be quoted [with\n" );
  printf( "    the other type of quotes], but it is not required.)  If no condition is\n" );
  printf( "    specified, then only the row specification (if any) is used to select rows.\n" );
  printf( "<outfile> is the name of the file to generate.  A LIGO_LW document, containing\n" );
  printf( "    the table with only the rows satisfying the condition, is written to this\n" );
  printf( "    file.  If no output file is specified, then this utility simply counts the\n" );
  printf( "    number of matching rows and prints it to standard output.\n" );
  printf( "Examples:\n" );
  printf( "  lwtcut myevents.xml 'snr > 8'\n" );
  printf( "  lwtcut myevents.xml 'ifo==L1' -o myL1events.xml\n" );
  printf( "  lwtcut myevents.xml -r 1-100 myevents_first100.xml\n" );
  return;
}


/*===========================================================================*/
int main( int argc, char **argv )
{
  char *arg, *val;
  int iarg;
  char opt='\0';
  char *file=NULL;
  char *tablename=NULL;
  char *condition=NULL;
  char *outfile=NULL;
  size_t vallen;
  int nvals;
  int valid, status, colindex;
  enum METAIO_Type coltype;
  int nmatch = 0;
  int rsmin[1024], rsmax[1024];   /*-- Row specification list --*/
  int nrs=-1; /*-- Number of row ranges; special value -1 means all rows --*/
  char *vptr, *dptr, *hptr, *endptr1, *endptr2;
  char spaceString[] = " ";
  char dsave;
  char colname[256];
  int namelen, oplen;
  int istart, iend, iovr1, iovr2;
  int delta, irange;
  int irow, active, target;

  enum opcodes { LESS_THAN, EQUAL_TO, GREATER_THAN, LESS_THAN_OR_EQUAL_TO,
		   GREATER_THAN_OR_EQUAL_TO, NOT_EQUAL_TO };
  int opcode;
  double compare_to, compval;
  char *scompare_to, *scompval;
  int scompare_length, scompvallength;
  int numeric_compare_ok, column_is_numeric;

  struct MetaioParseEnvironment parseEnv, outParseEnv;
  const MetaioParseEnv inEnv = &parseEnv;
  const MetaioParseEnv outEnv = &outParseEnv;
  struct MetaioTable *table;

  /*------ Beginning of code ------*/

  if ( argc <= 1 ) {
    PrintUsage(1); return 0;
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

    case '\0':   /*-- Positional argument --*/
      if ( file == NULL ) {
	file = arg;
      } else if ( condition == NULL ) {
	condition = arg;
      } else {
	printf( "Error: too many positional arguments\n" );
	PrintUsage(0); return 1;
      }
      break;

    case 't':   /*-- Table name --*/
      if ( tablename != NULL && val != NULL ) {
	printf( "Error: multiple table names specified: %s & %s\n",
		tablename, val );
	PrintUsage(0); return 1;
      }
      tablename = val;
      if ( val != NULL ) { opt = '\0'; }
      break;

    case 'o':   /*-- Output file name --*/
      if ( outfile != NULL && val != NULL ) {
	printf( "Error: multiple output file names specified: %s & %s\n",
		outfile, val );
	PrintUsage(0); return 1;
      }
      outfile = val;
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
	  PrintUsage(0); return 1;
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

    default:
      printf( "Invalid option -%c\n", opt );
      PrintUsage(0); return 1;

    }

  }

  /*-- Make sure required arguments were specified --*/
  if ( file == NULL ) {
    printf( "Input file was not specified\n" );
    PrintUsage(0); return 1;
  }

  if ( condition ) {
    /*-- Parse the condition --*/
    /* Skip over any initial spaces */
    while ( *condition == ' ' ) { condition++; }

    /* Separate the parts of the condition */
    dptr = condition;
    namelen = 0;
    while ( *dptr != '<' && *dptr != '=' && *dptr != '>' && *dptr != '!' &&
	    *dptr != ' ' ) {
      if ( *dptr == '\0' ) {
	printf( "Condition must have an operator and a value\n" );
	PrintUsage(0); return 1;
      }
      dptr++;
      namelen++;
    }
    if ( namelen == 0 ) {
	printf( "Condition must start with a column name\n" );
	PrintUsage(0); return 1;
    }
    strncpy( colname, condition, namelen );
    colname[namelen] = '\0';

    /*-- Skip any spaces --*/
    while ( *dptr == ' ' ) { dptr++; }
    /* dptr is now a pointer to the begining of the operator */

    vptr = dptr;
    oplen = 0;
    while ( *vptr == '<' || *vptr == '=' || *vptr == '>' || *vptr == '!' ) {
      vptr++;
      oplen++;
    }
    /*-- Check the validity of the operator, and convert it to a code --*/
    if ( oplen == 0 || oplen > 2 ) {
	printf( "Condition must use a recognized operator\n" );
	PrintUsage(0); return 1;
    }
    if ( oplen == 1 ) {
      if ( *dptr == '<' ) {
	opcode = LESS_THAN ;
      } else if ( *dptr == '=' ) {
	opcode = EQUAL_TO ;
      } else if ( *dptr == '>' ) {
	opcode = GREATER_THAN ;
      } else {
	printf( "Condition must use a recognized operator\n" );
	PrintUsage(0); return 1;
      }
    } else if ( strncmp(dptr,"==",2) == 0 ) {
      opcode = EQUAL_TO ;
    } else if ( strncmp(dptr,"<=",2) == 0 ) {
      opcode = LESS_THAN_OR_EQUAL_TO ;
    } else if ( strncmp(dptr,">=",2) == 0 ) {
      opcode = GREATER_THAN_OR_EQUAL_TO ;
    } else if ( strncmp(dptr,"<>",2) == 0 ) {
      opcode = NOT_EQUAL_TO ;
    } else if ( strncmp(dptr,"!=",2) == 0 ) {
      opcode = NOT_EQUAL_TO ;
    } else {
      printf( "Condition must use a recognized operator\n" );
      PrintUsage(0); return 1;
    }

    /*-- Skip any spaces --*/
    while ( *vptr == ' ' ) { vptr++; }
    /* vptr is now a pointer to the begining of the value */

    /*-- Trim off any spaces at the end --*/
    endptr1 = vptr + strlen(vptr);
    while ( endptr1 != vptr && *(endptr1-1) == ' ' ) {
      endptr1--;
      *endptr1 = '\0';
    }

    /*-- Try to parse the value as a double --*/
    compare_to = strtod( vptr, &endptr2 );
    if ( *vptr != '\0' && *endptr2 == '\0' ) {
      /*-- The whole string was successfully converted --*/
      numeric_compare_ok = 1;
    } else {
      numeric_compare_ok = 0;
    }

    /*-- If value isn't numeric, only certain comparisons are allowed --*/
    if ( ! numeric_compare_ok && opcode!=EQUAL_TO && opcode!=NOT_EQUAL_TO ) {
      printf( "Invalid string comparison (only == and != are allowed)\n" );
      PrintUsage(0); return 1;
    }

    /*-- If string value is quoted, strip away the quotes --*/
    if ( (*vptr=='"' && *(endptr1-1)=='"') ||
	 (*vptr=='\'' && *(endptr1-1)=='\'') ) {
      vptr++;
      endptr1--;
      *endptr1 = '\0';
    }

    /*-- Store a pointer to the string value, and the length of the string --*/
    scompare_to = vptr;
    scompare_length = strlen(scompare_to);

  }

  /*-- Open the file --*/
  status = MetaioOpenTable( inEnv, file, tablename );
  if ( status != 0 ) {
    if ( tablename == NULL ) {
      printf( "Error opening file %s\n", file );
      printf( "%s\n", inEnv->mierrmsg.data );
    } else {
      printf( "Error opening table %s in file %s\n", tablename, file );
      printf( "%s\n", inEnv->mierrmsg.data );
    }
    MetaioAbort( inEnv );
    return 2;
  }

  if ( condition ) {
    /*-- Determine column index for the column in the condition --*/
    colindex = MetaioFindColumn( inEnv, colname );
    if ( colindex < 0 ) {
      printf( "There is no '%s' column in the file %s\n", colname, file );
      MetaioAbort(inEnv);
      return 1;
    }
    coltype = inEnv->ligo_lw.table.col[colindex].data_type;
  }

  if ( outfile ) {
    /*-- Open the output file --*/
    status = MetaioCreate( outEnv, outfile );
    if ( status != 0 ) {
      printf( "Error opening output file %s\n", outfile );
      MetaioAbort( inEnv );
      return 2;
    }
    MetaioCopyEnv( outEnv, inEnv );
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

  while ( (status=MetaioGetRow(inEnv)) == 1 ) {
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
	  MetaioAbort( inEnv );
	  break;    /*-- We don't want any more rows from this file --*/
	}
	target = rsmin[irange];
      }
    }

    if ( ! active ) continue;

    /*-- Check the condition (if any) --*/
    if ( condition ) {

      table = &(inEnv->ligo_lw.table);
      column_is_numeric = 1;
      switch ( coltype ) {
      case METAIO_TYPE_INT_2S:
	compval = (double) table->elt[colindex].data.int_2s ;
	break;
      case METAIO_TYPE_INT_4S:
	compval = (double) table->elt[colindex].data.int_4s ;
	break;
      case METAIO_TYPE_REAL_4:
	compval = (double) table->elt[colindex].data.real_4 ;
	break;
      case METAIO_TYPE_REAL_8:
	compval = table->elt[colindex].data.real_8 ;
	break;
      case METAIO_TYPE_LSTRING:
      case METAIO_TYPE_ILWD_CHAR:
      case METAIO_TYPE_CHAR_S:
      case METAIO_TYPE_CHAR_V:
	scompval = table->elt[colindex].data.lstring.data;
	scompvallength = table->elt[colindex].data.lstring.len;
	column_is_numeric = 0;
	break;
      default:
	printf( "Column value must be an integer, real number, or string\n" );
	MetaioAbort(inEnv);
	if ( outfile ) break;
	return 1;
      }

      /*-- Make sure a comparison is possible --*/
      if ( column_is_numeric && ! numeric_compare_ok ) {
	printf( "Cannot perform string comparison with numeric-valued column\n" );
	MetaioAbort(inEnv);
	if ( outfile ) break;
	return 1;
      }
      if ( ! column_is_numeric && opcode!=EQUAL_TO && opcode!=NOT_EQUAL_TO ) {
	printf( "Invalid string comparison (only == and != are allowed)\n" );
	MetaioAbort(inEnv);
	if ( outfile ) break;
	return 1;
      }

      /*-- Do the comparison --*/
      switch ( opcode ) {
      case EQUAL_TO:
	if ( column_is_numeric ) {
	  if ( compval != compare_to ) continue;
	} else {
	  if ( scompvallength != scompare_length ) continue;
	  if ( strncmp(scompval,scompare_to,scompvallength) ) continue;
	}
	break;
      case LESS_THAN:
	if ( compval >= compare_to ) continue;
	break;
      case GREATER_THAN:
	if ( compval <= compare_to ) continue;
	break;
      case LESS_THAN_OR_EQUAL_TO:
	if ( compval > compare_to ) continue;
	break;
      case GREATER_THAN_OR_EQUAL_TO:
	if ( compval < compare_to ) continue;
	break;
      case NOT_EQUAL_TO:
	if ( column_is_numeric ) {
	  if ( compval == compare_to ) continue;
	} else {
	  if ( scompvallength == scompare_length ) {
	    if ( strncmp(scompval,scompare_to,scompvallength) == 0 ) continue;
	  }
	}
	break;
      }

    }

    /*-- Increment count of number of matching rows --*/
    nmatch++;

    if ( outfile ) {
      /*-- Copy row to output file --*/
      status = MetaioCopyRow( outEnv, inEnv );
      status = MetaioPutRow( outEnv );
    }

  }    /*-- End of loop over rows in the file --*/

  if ( status == -1 ) {
    printf( "Parsing error at row %d\n", irow+1 );
    printf( "%s\n", inEnv->mierrmsg.data );
  }

  /*-- Read to the end of the file, then close it --*/
  MetaioClose(inEnv);

  if ( outfile ) {
    /*-- Close output file --*/
    MetaioClose( outEnv );
  }

  /*-- Print number of rows matched --*/
  if ( outfile ) {
    printf( "%d rows written to %s\n", nmatch, outfile );
  } else {
    printf( "%d rows\n", nmatch );
  }

  return 0;
}
