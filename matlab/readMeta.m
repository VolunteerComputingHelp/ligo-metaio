% readMeta --- Read LIGO metadata table into a MATLAB structure
% By Peter Shawhan (shawhan_p@ligo.caltech.edu), April 2001
% Revised by Peter Shawhan, Dec 2002, to automatically unpack spectra from BLOBs
% Revised by Thomas Cokelaer and Peter Shawhan, Apr 2006, to improve messages
% Uses "metaio" parsing library by Philip Charlton
% 
% If readMeta() is called with no left-hand-side variables, then it simply
% prints the column names and types and the number of rows in the table.
% If called with one or more left-hand-side variable, the function returns a
% MATLAB structure with fields which correspond to columns in the LIGO_LW table.
% Each numeric column is converted to a vector of double-precision values,
% except that an int_8s column is stored as a vector of 8-byte integers
% (Matlab class mxINT64_CLASS) to preserve full precision.
% Each string or binary column is converted to a cell array containing
% individual char arrays.
% The left-hand side can have optional second and third elements, which are
% filled with the number of rows read (expressed as a double-precision value)
% and the error message (which is the null string if no error occurred).
%
% Usage:  [data,nrows,errmsg] = readMeta( file [,table] [,row [,col]] [,opts] )
%
%  file  is the name of a "LIGO lightweight" XML file containing a Table object.
%  table (optional) specifies the name of the table to be read; this allows a
%          specific table to be read from a file containing multiple tables.  If
%          omitted, the first table is read. The table name is case-insensitive.
%  row   is a row number (counting from 1), or a vector of row numbers
%          (in ascending order), e.g. [2:5,10] gives rows 2 through 5 and
%          row 10. If omitted, or equal to 0, then all rows are included.
%  col   is a string containing one or more column names, separated by
%          commas and/or spaces.  If omitted, all columns are included.
%  opts  is a optional string beginning with '-' and followed by one or more
%          letters which control the operation of the function.  At present,
%          the only valid letter is 'q' (i.e. you should pass '-q' as the
%          argument), which suppresses warning and informational messages.
%
% Although most of the arguments are optional, any arguments given must be
% given in the proper order.  For instance, the opts argument must be last.
%
% The number of rows read from the file is printed to the screen UNLESS the
% 'q' option was given.
%
% Spectrum BLOBs (from the summ_spectrum table) are automatically unpacked into
% numeric arrays, assuming the mimetype is one of the standard LIGO ones.
% Note: to access the contents of a Matlab cell array, use curly braces around
% the index.  For example, "a.spectrum{1}" gives the first spectrum array.
%
% ERROR HANDLING:
%
% * Syntax errors and out-of-memory errors always cause an error message to be
%     printed and the function to return without assigning any output.
% * Attempting to read from a nonexistent or unreadable file, or from a file
%     which does not have the standard LIGO_LW header, causes an error message
%     to be printed, and causes the function to return an empty structure.  If
%     the optional 2nd and 3rd output arguments are given, they are filled with
%     0 and the string "Error opening file", respectively.
% * If a valid LIGO_LW file was opened but it does not contain a table with the
%     specified name (or does not contain any table at all), then a warning
%     message is printed UNLESS the 'q' option was given, and the output is
%     assigned to be an empty structure.  If the optional 2nd and 3rd output
%     arguments are given, they are filled with 0 and the string "Table does
%     not exist in file", respectively.
% * If a column (or more than one) is specified but is not found in the table
%     read from the file, then that column name is ignored.  A warning message
%     is printed to the screen UNLESS the 'q' option was given.
% * If readMeta() encounters a formatting error while reading the Stream, then
%     an error message is printed to the screen; the output data structure is
%     filled normally with data from rows read prior to the row with the
%     formatting error; the optional 2nd argument is filled with the number of
%     rows read up to the time of the error; and the optional 3rd argument is
%     filled with the message associated with the error.
