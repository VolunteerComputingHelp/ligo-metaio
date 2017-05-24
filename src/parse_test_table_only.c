#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metaio.h"

int quiet_mode = 0;

void
print_help()
{
    fprintf(stderr,
	    "Usage: parse_test_table_only [ -h ] [ -q ] filename\n"
	    "Options:\n"
	    "  -h   : print this message\n"
	    "  -q   : don't print rows\n");
}

int
test_table(const char* const filename, const char* const tablename)
{
    struct MetaioParseEnvironment parseEnvironment;
    MetaioParseEnv const env = &parseEnvironment;
    int fail = 0;
    int ret = 0;
    int count = 0;
    int i = 0;

    if ((ret = MetaioOpenFile(env, filename)) != 0)
    {
	fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
        fail = 1;
        return fail;
    }

    if ((ret = MetaioOpenTableOnly(env, tablename)) != 0)
    {
	fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
        fail = 1;
        return fail;
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
	/* Error from MetaioGetRow() */
	fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
        fail = 1;
        return fail;
    }

    ret = MetaioClose(env);

    if (ret < 0)
    {
	/* Error from MetaioClose() */
	fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
        fail = 1;
        return fail;
    }
    
    return fail;
}


int
test_no_table(const char* const filename, const char* const tablename)
{
    struct MetaioParseEnvironment parseEnvironment;
    MetaioParseEnv const env = &parseEnvironment;
    int fail = 0;
    int ret = 0;

    if ((ret = MetaioOpenFile(env, filename)) != 0)
    {
	fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
        fail = 1;
        return fail;
    }

    if ((ret = MetaioOpenTableOnly(env, tablename)) == 0)
    {
	fprintf(stderr, "FAIL: no error for missing table%s\n",
                tablename);
        fail = 1;
        return fail;
    }
    else
    {
        if (strstr(env->mierrmsg.data, "table not found") == 0)
        {
            fprintf(stderr, "FAIL: %s\n", env->mierrmsg.data);
            fail = 1;
            return fail;
        }
    }

    MetaioAbort(env);

    return fail;
}

int
main(int argc, char** argv)
{
    int fail = 0;
    int i = 0;

    if (argc > 4)
    {
	fprintf(stderr, "too many options\n");
	print_help();
	exit(1);
    }

    /* Run through any optional flags */
    i = 1;
    while (i < argc && argv[i][0] == '-')
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
        ++i;
    }

    fail |= test_table(argv[i], "row");
    if(strstr(argv[i], "gdstrig10.xml"))
        {
        fail |= test_table(argv[i], "row2");
        fail |= test_table(argv[i], "row3");
        }
    fail |= test_no_table(argv[i], "faketable");

    return fail;
}

