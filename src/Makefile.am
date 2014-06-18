MAINTAINERCLEANFILES =		$(srcdir)/Makefile.in

sbin_PROGRAMS = 		meterd

#bin_PROGRAMS =			meterd-output \
#				meterd-plot

meterd_CFLAGS =			@LIBCONFIG_CFLAGS@

meterd_SOURCES =		meterd_main.c \
				meterd_log.c \
				meterd_log.h \
				meterd_config.c \
				meterd_config.h \
				p1_parser.c \
				p1_parser.h \
				utlist.h \
				uthash.h

meterd_LDADD =			@LIBCONFIG_LIBS@
