ligotools-recursive:
ifneq ($(RECURSIVE_TARGETS),ligotools-recursive)
ligotools-recursive:
	$(MAKE) $(AM_MAKEFLAGS) RECURSIVE_TARGETS=ligotools-recursive ligotools-recursive
endif

ligotools: ligotools-recursive ligotools-am

ligotools-am: ligotools-local

ligotools-local:
