#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metaio.h"

const char* const default_filename = "gdstrig10.xml";

int quiet_mode = 0;

void
print_help()
{
    fprintf(stderr,
	    "Usage: parse_test [ -h ] [ -q ] [ file ]\n"
	    "Options:\n"
	    "  -h   : print this message\n"
	    "  -q   : don't print rows\n"
	    "  file : filename (default filename is %s)\n", default_filename);
}

int
main(int argc, char** argv)
{
    int exitval = 0;

    struct MetaioParseEnvironment parseEnvironment;
    MetaioParseEnv const env = &parseEnvironment;
    int count = 0;
    const char* filename = default_filename;
    int i = 0;
    int ret = 0;

    if (argc > 3)
    {
	fprintf(stderr, "too many options\n");
	print_help();
	exit(1);
    }
    
    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    if (strcmp(argv[i], "-q") == 0)
	    {
		quiet_mode = 1;
	    }
	    else if (strcmp(argv[i], "-h") == 0)
	    {
		print_help();
		exit(0);
	    }
	    else
	    {
		fprintf(stderr, "unknown option: %s\n", argv[i]);
		print_help();
		exit(1);
	    }
	}
	else
	{
	    filename = argv[i];
	    break;
	}
    }

    if ((ret = MetaioOpen(env, filename)) != 0)
    {
	fprintf(stderr, "%s\n", env->mierrmsg.data);
	exit(ret);
    }
  
    while ((ret = MetaioGetRow(env)) > 0)
    {
	count++;
	if (quiet_mode == 0)
	{
	    printf("ROW %d:\n", count);
	    for (i = 0; i < env->ligo_lw.table.numcols; i++)
	    {
		printf("  Name = <%s> Type = <%s> Data = <", 
		       env->ligo_lw.table.col[i].name,
		       MetaioTypeText(env->ligo_lw.table.col[i].data_type));
		MetaioFprintElement(stdout, &env->ligo_lw.table.elt[i]);
		printf(">\n");
	    }
	    printf("---------------------------------------------------------"
		   "----------------------\n");
	}
    }

    if (ret < 0)
    {
	fprintf(stderr, "Error from MetaioGetRow(): %s\n", env->mierrmsg.data);
	exit(ret);
    }

    ret = MetaioClose(env);

    if (ret < 0)
    {
	fprintf(stderr, "Error from MetaioClose(): %s\n", env->mierrmsg.data);
	exit(ret);
    }

    return exitval;
}

