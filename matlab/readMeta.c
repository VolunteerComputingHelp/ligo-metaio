/*----------------------------------------------------------------
readMeta.c
Written by Peter Shawhan, April 2001
Uses the "metaio" library from Philip Charlton
In Matlab, type 'readMeta' without any arguments to see usage info
Modified 12/30/02 by PSS to parse spectrum blobs according to mimetype.
Modified 04/21/05 by PSS to store INT_8S values in Matlab mxINT64 class.

Ideas for future improvements:
*  Allow field name to be something other than the column name.
      (I can't see how to rename an existing field in Matlab)

----------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mex.h"
#include "metaio.h"

void  mexFunction( int nlhs,       mxArray *plhs[],
                   int nrhs, const mxArray *prhs[])
{
  int status;
  int verbose = 1;
  int ncols, icol, nusecols, iusecol;
  int icolSpectrum, icolMimetype, mimetypeCode;
  char *mimetype;
  int collist[METAIOMAXCOLS];
  int nrows, nread, irow;

  int allrows, bailed, allcols;
  double *rowlist;
  int nrowlist, irowlist;

  char colspec[1024];

  int datasize[METAIOMAXCOLS], dataused[METAIOMAXCOLS];
  void *dataptr[METAIOMAXCOLS], *newptr;
  int size, length, i, j;
  char file[256], tablename[64], opts[64];
  int optadj, argadj;
  char msg[1024];
  char *cptr, *cptr2, *vptr, *dptr;
  char dsave;
  unsigned char *ucptr;
  unsigned char *tempptr;
  unsigned char usave;
  float *fptr;
  const char *colnames_p[METAIOMAXCOLS];
  int coltype[METAIOMAXCOLS];

  mxArray *fout, *carr;
  mxArray **mxap;
  mxChar *mxcptr;
  double *mxrptr;
  double *mxiptr;
  int dims[2], dims2[2];

  struct MetaioParseEnvironment parseEnv;
  const MetaioParseEnv env = &parseEnv;
  struct MetaioRowElement *elt;

  /*-- Byte-swapping unit --*/
  int swap4, swap8;
  union swapcheck_tag {
    int ival;
    char sval[4];
  } swapcheck;

  /*------ Set default parameter values ------*/
  allrows = 1;
  bailed = 0;
  allcols = 1;
  msg[0] = '\0';

  /*------ Check validity of arguments, and get their values ------*/
  if ( nrhs == 0 || nrhs > 5 ) {
    mexPrintf( "\n  Usage:  a = readMeta( file [,table_name] [,row [,col]] [,opts] )"
	       "\n  Type 'help readMeta' for details.\n\n" );
    if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
    return;
  }

  if( ! mxIsChar(prhs[0]) ) {
    mexPrintf( "\n  The first argument must be a filename"
	       "\n  Type 'help readMeta' for usage information.\n\n" );
    if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
    return;
  }
  status = mxGetString( prhs[0], file, sizeof(file) );
  if( status != 0 ) {
    mexPrintf( "\n  Error getting filename"
	       "\n  Type 'help readMeta' for usage information.\n\n" );
    if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
    return;
  }

  tablename[0] = '\0';
  opts[0] = '\0';
  optadj = 0;
  argadj = 0;

  /*-- If there are exactly five arguments, or if the last argument is a string
    beginning with '-', then interpret the last arg as an options string --*/
  if ( nrhs == 5 ) {
    if ( mxIsChar(prhs[4]) ) {
      /*-- Read the options string --*/
      status = mxGetString( prhs[4], opts, sizeof(opts) );
      if( status != 0 ) {
	mexPrintf( "\n  Error getting options string"
		   "\n  Type 'help readMeta' for usage information.\n\n" );
	if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
	return;
      }
      optadj = 1;
    } else {
      mexPrintf( "\n  Found a numeric value where the options string should be"
		 "\n  Type 'help readMeta' for usage information.\n\n" );
      if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
      return;
    }
  } else if ( mxIsChar(prhs[nrhs-1]) ) {
    status = mxGetString( prhs[nrhs-1], opts, sizeof(opts) );
    if( status == 0 && opts[0] == '-' ) {
      /*-- This looks like a valid options string --*/
      optadj = 1;
    } else {
      /*-- This doesn't have the form of an options string --*/
      opts[0] = '\0';
    }
  }

  /*-- If an options string was given, parse it --*/
  if ( strlen(opts) ) {
    if ( opts[0] != '-' ) {
      mexPrintf( "\n  Options string must begin with a '-'"
		 "\n  Type 'help readMeta' for usage information.\n\n" );
      if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
      return;
    }
    for ( cptr = opts+1; *cptr != '\0'; cptr++ ) {
      switch ( *cptr ) {
      case 'q':
	verbose = 0;
	break;
      default:
	mexPrintf( "\n  Invalid option '%c'"
		   "\n  Type 'help readMeta' for usage information.\n\n",
		   *cptr );
	if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
	return;
      }
    }
  }

  if ( nrhs-optadj > 1 ) {
    if ( mxIsChar(prhs[1]) ) {
      /*-- The user specified a table name as the (optional) 2nd argument --*/
      status = mxGetString( prhs[1], tablename, sizeof(tablename) );
      if( status != 0 ) {
	mexPrintf( "\n  Error getting table name"
		   "\n  Type 'help readMeta' for usage information.\n\n" );
	if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
	return;
      }
      argadj = 1;
    }
  }

  if ( nrhs-optadj > 1+argadj ) {
    if( mxIsChar(prhs[1+argadj]) ) {
      mexPrintf( "\n  Found a string where the row number(s) should be"
		 "\n  Type 'help readMeta' for usage information.\n\n" );
      if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
      return;
    }

    allrows = 0;
    rowlist = mxGetPr( prhs[1+argadj] );
    nrowlist = mxGetM( prhs[1+argadj] ) * mxGetN( prhs[1+argadj] );
    irowlist = 0;
    /* If the row list is the scalar 0, then return all rows */
    if ( nrowlist==1 && (int)*rowlist == 0 ) { allrows = 1; }
  }

  if ( nrhs-optadj > 2+argadj ) {
    if( ! mxIsChar(prhs[2+argadj]) ) {
      mexPrintf( "\n  The last argument (other than a possible options string) must be a\n  string containing one or more column names"
		 "\n  Type 'help readMeta' for usage information.\n\n" );
      if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
      return;
    }

    status = mxGetString( prhs[2+argadj], colspec, sizeof(colspec) );
    if( status != 0 ) {
      mexPrintf( "\n  Error getting column specification"
		 "\n  Type 'help readMeta' for usage information.\n\n" );
      if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Syntax error" ); }
      return;
    }
    /* See if string is non-blank */
    cptr = colspec;
    while ( *cptr == ' ' ) { cptr++; }
    if ( strlen(cptr) ) {
      allcols = 0;
    }
  }

  /*------ open file ------*/
  /*mexPrintf( "Opening file %s\n", file );*/

  status = MetaioOpenFile( env, file );
  if ( status != 0 ) {
    sprintf( msg, "Error opening file %s for reading\n", file );
    mexPrintf( msg );
    if ( verbose ) {
      if ( nlhs > 0 ) { mexPrintf( "Returning an empty structure\n" ); }
    }
    /*-- Return an empty structure --*/
    if ( nlhs > 0 ) { plhs[0] = mxCreateStructMatrix( 1, 1 , 0, NULL ); }
    if ( nlhs > 1 ) { plhs[1] = mxCreateDoubleScalar( 0 ); }
    if ( nlhs > 2 ) { plhs[2] = mxCreateString( "Error opening file" ); }
    return;
  }

  status = MetaioOpenTableOnly( env, tablename );
  if ( status != 0 ) {
    if ( strlen(tablename) ) {
      sprintf( msg, "Warning: file %s does not contain a table named '%s'\n",
	       file, tablename );
    } else {
      sprintf( msg, "Warning: file %s does not contain a table\n", file );
    }
    if ( verbose ) {
      mexPrintf( msg );
      if ( nlhs > 0 ) { mexPrintf( "Returning an empty structure\n" ); }
    }
    MetaioAbort( env );
    /*-- Return an empty structure --*/
    if ( nlhs > 0 ) { plhs[0] = mxCreateStructMatrix( 1, 1 , 0, NULL ); }
    if ( nlhs > 1 ) { plhs[1] = mxCreateDoubleScalar( 0 ); }
    if ( nlhs > 2 ) {
      plhs[2] = mxCreateString( "Table does not exist in file" );
    }
    return;
  }


  /*-- Clear data pointers for all columns --*/
  ncols = env->ligo_lw.table.numcols;
  for ( icol=0; icol<ncols; icol++ ) {
    dataptr[icol] = NULL;
  }


  /*------ Figure out which columns to process ------*/

  if ( allcols == 1 ) {

    nusecols = ncols;
    for ( icol=0; icol < ncols; icol++ ) {
      collist[icol] = icol;
    }

  } else {

    nusecols = 0;

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
	icol = MetaioFindColumn( env, vptr );
	if ( icol < 0 ) {
	  if ( verbose ) {
	    mexPrintf( "Warning: column '%s' does not exist in table '%s'\n",
		       vptr, env->ligo_lw.table );
	  }
	} else {
	  /*-- Append this column index to the list --*/
	  collist[nusecols] = icol;
	  nusecols++;
	}
      }

      if ( dptr != NULL ) {
	/*-- Restore the delimiter and get ready to parse next token --*/
	*dptr = dsave;
	vptr = dptr + 1;
      }

    } while ( dptr != NULL );

    if ( nusecols == 0 && verbose ) {
      mexPrintf( "Warning: none of the specified columns exist in table '%s'\n",
		 env->ligo_lw.table );
    }

  }

  /*-- Locate the "spectrum" and "mimetype" columns, if they exist --*/
  icolSpectrum = MetaioFindColumn( env, "spectrum" );
  icolMimetype = MetaioFindColumn( env, "mimetype" );


  /*------ Initialize space to store values read from the table ------*/

  for ( iusecol=0; iusecol<nusecols; iusecol++ ) {
    icol = collist[iusecol];

    /* Get a pointer to the column name, and store it in an array for Matlab */
    cptr = MetaioColumnName( env, icol );
    colnames_p[iusecol] = cptr;

    /* Convert the column name to lowercase in-place! */
    cptr2 = cptr;
    while ( *cptr2 != '\0' ) {
      *cptr2 = (char) tolower( (int) *cptr2 );
      cptr2++;
    }

    coltype[icol] = env->ligo_lw.table.col[icol].data_type;
    if ( nlhs == 0 ) {
      /* Just report column name and type */
      mexPrintf( "  %-20s %s\n", cptr, MetaioTypeText(coltype[icol]) );
    }

    if ( nlhs > 0 ) {
      /*-- Allocate some memory for values from this column --*/
      datasize[icol] = 1024;
      dataused[icol] = 0;
      dataptr[icol] = mxMalloc( datasize[icol] );
      if ( dataptr[icol] == NULL ) {
	mexPrintf( "Error allocating memory\n" );
	MetaioAbort( env );
	return;
      }
    }
  }

  if ( nlhs > 0 ) {
    /* Create a 1x1 struct matrix for output */
    plhs[0] = mxCreateStructMatrix( 1, 1, nusecols, colnames_p );
  }


  /*------ Loop over rows in the file ------*/

  nrows = 0;
  nread = 0;
  while ( (status=MetaioGetRow(env)) == 1 ) {
    nrows++;

    /*-- Make sure we should process this row --*/
    if ( allrows == 0 ) {
      /* If we've read the last specified row, exit from the loop */
      if ( irowlist >= nrowlist ) {
	/* Immediately close the input file */
	MetaioAbort( env );
	bailed = 1;
	break;
      }

      if ( nrows != (int)rowlist[irowlist] ) { continue; }
      /* Increment row-list counter */
      irowlist++;
    }

    nread++;

    /* If there is no output variable, skip the rest of the loop */
    if ( nlhs == 0 ) { continue; }

    irow = nread - 1;
    for ( iusecol=0; iusecol<nusecols; iusecol++ ) {
      icol = collist[iusecol];

      /* All numeric values except INT_8S and INT_8U get converted to
       * double */
      switch ( coltype[icol] ) {
      case METAIO_TYPE_INT_4S:
      case METAIO_TYPE_INT_4U:
      case METAIO_TYPE_INT_2S:
      case METAIO_TYPE_INT_2U:
      case METAIO_TYPE_REAL_4:
      case METAIO_TYPE_REAL_8:
	size = 8;
	break;
      case METAIO_TYPE_INT_8S:
      case METAIO_TYPE_INT_8U:
	size = 8;
	break;
      case METAIO_TYPE_ILWD_CHAR:
      case METAIO_TYPE_LSTRING:
      case METAIO_TYPE_ILWD_CHAR_U:
	size = sizeof(mxArray *);
	break;
      default:
	size = 0;
      }

      dataused[icol] += size;
      if ( dataused[icol] > datasize[icol] ) {
	datasize[icol] *= 2;
	/*mexPrintf( "Calling mxRealloc for column %d\n", icol );*/
	newptr = mxRealloc( dataptr[icol], datasize[icol] );
	if ( newptr == NULL ) {
	  mexPrintf( "Error allocating more memory\n" );
	  MetaioAbort( env );
	  return;
	}
	dataptr[icol] = newptr;
      }

      elt = &(env->ligo_lw.table.elt[icol]);

      /*------ For numeric variables, store the value.  For character-array
	variables, create a Matlab array to hold the value, and store a
	pointer to the Matlab array. ------*/

      switch ( coltype[icol] ) {
      case METAIO_TYPE_INT_4S:
	((double *)dataptr[icol])[irow] = (double) elt->data.int_4s;
	break;
      case METAIO_TYPE_INT_4U:
	((double *)dataptr[icol])[irow] = (double) elt->data.int_4u;
	break;
      case METAIO_TYPE_INT_2S:
	((double *)dataptr[icol])[irow] = (double) elt->data.int_2s;
	break;
      case METAIO_TYPE_INT_2U:
	((double *)dataptr[icol])[irow] = (double) elt->data.int_2u;
	break;
      case METAIO_TYPE_INT_8S:
	((METAIO_INT_8S *)dataptr[icol])[irow] = elt->data.int_8s;
	break;
      case METAIO_TYPE_INT_8U:
	((METAIO_INT_8U *)dataptr[icol])[irow] = elt->data.int_8u;
	break;
      case METAIO_TYPE_REAL_4:
	((double *)dataptr[icol])[irow] = (double) elt->data.real_4;
	break;
      case METAIO_TYPE_REAL_8:
	((double *)dataptr[icol])[irow] = elt->data.real_8;
	break;

      case METAIO_TYPE_LSTRING:
      case METAIO_TYPE_ILWD_CHAR:
	((mxArray **)dataptr[icol])[irow] = mxCreateString(elt->data.lstring.data);
	break;

      case METAIO_TYPE_BLOB:
      case METAIO_TYPE_ILWD_CHAR_U:
	length = elt->data.blob.len;
	ucptr = elt->data.blob.data;

	/*-- If this is the "spectrum" column, and there is also a "mimetype"
	  column, and we know how to handle the mimetype, then unpack the
	  numeric values in the BLOB.  Otherwise, just store a char array. --*/
	
	mimetypeCode = 0;
	if ( icol == icolSpectrum && icolMimetype >= 0 ) {
	  mimetype = env->ligo_lw.table.elt[icolMimetype].data.lstring.data;
	  /*mexPrintf( "Mimetype is %s\n", mimetype );*/

	  /*-- See if this is a mimetype we know how to handle, and decide
	    whether to swap bytes (in 4's or 8's) --*/
	  swap4 = 0;
	  swap8 = 0;
	  swapcheck.ival = 1;
	  if ( strcmp(mimetype,"x-ligo/real-spectrum-big") == 0 ) {
	    mimetypeCode = 1;
	    swap4 = ( swapcheck.sval[0] == 1 );
	  } else if ( strcmp(mimetype,"x-ligo/real-spectrum-little") == 0 ) {
	    mimetypeCode = 2;
	    swap4 = ( swapcheck.sval[3] == 1 );
	  } else if ( strcmp(mimetype,"x-ligo/complex-spectrum-big") == 0 ) {
	    mimetypeCode = 3;
	    swap4 = ( swapcheck.sval[0] == 1 );
	  } else if ( strcmp(mimetype,"x-ligo/complex-spectrum-little") ==0 ) {
	    mimetypeCode = 4;
	    swap4 = ( swapcheck.sval[3] == 1 );
	  }

	  /*-- If appropriate, swap bytes in-place --*/
	  if ( swap4 ) {
	    /*-- If length not a multiple of 4, don't swap "leftover" bytes -*/
	    for ( i=0, tempptr=ucptr; i<length-3; i+=4, tempptr+=4 ) {
	      usave = tempptr[0];
	      tempptr[0] = tempptr[3];
	      tempptr[3] = usave;
	      usave = tempptr[1];
	      tempptr[1] = tempptr[2];
	      tempptr[2] = usave;
	    }
	  }

	  if ( swap8 ) {
	    /*-- If length not a multiple of 8, don't swap "leftover" bytes -*/
	    for ( i=0, tempptr=ucptr; i<length-7; i+=8, tempptr+=8 ) {
	      usave = tempptr[0];
	      tempptr[0] = tempptr[7];
	      tempptr[7] = usave;
	      usave = tempptr[1];
	      tempptr[1] = tempptr[6];
	      tempptr[6] = usave;
	      usave = tempptr[2];
	      tempptr[2] = tempptr[5];
	      tempptr[5] = usave;
	      usave = tempptr[3];
	      tempptr[3] = tempptr[4];
	      tempptr[4] = usave;
	    }
	  }

	}

	/*-- Unpack known mimetypes --*/
	switch ( mimetypeCode ) {

	case 1:
	case 2:
	  /*-- Interpret the data as an array of floats --*/
	  fptr = (float *) ucptr;
	  length = length / 4;
	  /*-- Create a Matlab array of doubles --*/
	  carr = mxCreateDoubleMatrix( length, 1, mxREAL );
	  mxrptr = mxGetPr(carr);
	  /*-- Fill the Matlab array, converting floats to doubles --*/
	  for ( i=0; i<length; i++ ) {
	    mxrptr[i] = (double) fptr[i];
	  }
	  break;

	case 3:
	case 4:
	  /*-- Interpret the data as an array of single-precision complex
	    numbers, with (real, imaginary) parts stored together --*/
	  fptr = (float *) ucptr;
	  length = length / 8;
	  /*-- Create a Matlab array of double-precision complex value --*/
	  carr = mxCreateDoubleMatrix( length, 1, mxCOMPLEX );
	  /*-- Get the addresses of the arrays of real and imaginary parts --*/
	  mxrptr = mxGetPr(carr);
	  mxiptr = mxGetPi(carr);
	  /*-- Fill the Matlab arrays, converting floats to doubles --*/
	  for ( i=0, j=0; i<length; i++, j+=2 ) {
	    mxrptr[i] = (double) fptr[j];
	    mxiptr[i] = (double) fptr[j+1];
	  }
	  break;

	default:
	  /*-- Store as a char array --*/
	  dims2[0] = 1;
	  dims2[1] = length;
	  carr = mxCreateCharArray( 2, dims2 );
	  mxcptr = (mxChar *) mxGetData(carr);
	  for ( i=0; i<length; i++ ) {
	    mxcptr[i] = (mxChar) ucptr[i];
	  }

	}

	/*-- Store the address of the Matlab array --*/
	((mxArray **)dataptr[icol])[irow] = carr;

	break;
      }

    }    /*-- End of loop over columns --*/
 }    /*-- End of loop over rows in the file --*/

  /*-- Check for an error --*/
  if ( status == -1 ) {
    sprintf( msg, "Parsing error at row %d : %s\n",
	     nrows+1, env->mierrmsg.data );
    mexPrintf( msg );
    /*-- This error message will get copied to the (optional) third
      output argument --*/
    bailed = 1;
  }

  if (verbose) {
    /*-- Print out number of rows --*/
    if ( nread == 1 ) {
      mexPrintf( "Read 1 row" );
    } else {
      mexPrintf( "Read %d rows", nread );
    }
    if ( allrows == 0 && bailed==0 ) {
      mexPrintf( " out of %d in file\n", nrows );
    } else {
      mexPrintf( "\n" );
    }
  }

  /* We can now close the input file */
  MetaioClose(env);

  /* If we were just printing out column names, then we are done */
  if ( nlhs == 0 ) {
    return;
  }


  /*------ Create the output structure ------*/

  /* Create an array for each column, and add it to the structure */
  for ( iusecol=0; iusecol<nusecols; iusecol++ ) {
    icol = collist[iusecol];

    dims[0] = nread;
    dims[1] = 1;

    switch ( coltype[icol] ) {
    case METAIO_TYPE_INT_4S:
    case METAIO_TYPE_INT_4U:
    case METAIO_TYPE_INT_2S:
    case METAIO_TYPE_INT_2U:
    case METAIO_TYPE_REAL_4:
    case METAIO_TYPE_REAL_8:
      fout = mxCreateDoubleMatrix( nread, 1, mxREAL );
      memcpy( mxGetPr(fout), dataptr[icol], 8*nread );
      break;
    case METAIO_TYPE_INT_8S:
    case METAIO_TYPE_INT_8U:
      fout = mxCreateNumericMatrix( nread, 1, mxINT64_CLASS, mxREAL );
      memcpy( mxGetData(fout), dataptr[icol], 8*nread );
      break;
    case METAIO_TYPE_LSTRING:
    case METAIO_TYPE_ILWD_CHAR:
    case METAIO_TYPE_ILWD_CHAR_U:
      fout = mxCreateCellArray( 2, dims );
      mxap = (mxArray **) dataptr[icol];
      for ( irow=0; irow<nread; irow++ ) {
	mxSetCell( fout, irow, mxap[irow] );
      }
      break;
    }

    /* Set each field in output structure */
    mxSetFieldByNumber( plhs[0], 0, iusecol, fout );

    /* Free the temporary array, which is no longer needed */
    mxFree( dataptr[icol] );

  }

  /*-- If requested, return the number of rows read --*/
  if ( nlhs > 1 ) {
    plhs[1] = mxCreateDoubleScalar( nread );
  }

  /*-- If requested, return the error string (or a null string if no error) */
  if ( nlhs > 2 ) {
    plhs[2] = mxCreateString( msg );
  }

  return;
}
