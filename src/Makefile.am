MAINTAINERCLEANFILES =		$(srcdir)/Makefile.in

sbin_PROGRAMS = 		meterd

bin_PROGRAMS =			meterd-createdb \
				meterd-output \
				testp1-parse

meterd_SOURCES =		meterd_main.c \
				meterd_log.c \
				meterd_log.h \
				meterd_config.c \
				meterd_config.h \
				p1_parser.c \
				p1_parser.h \
				db.c \
				db.h \
				measure.c \
				measure.h \
				comm.c \
				comm.h \ 
				tasksched.c \
				tasksched.h \
				utlist.h \
				uthash.h

meterd_LDADD =			@LIBCONFIG_LIBS@ @SQLITE3_LDFLAGS@ @PTHREAD_LIBS@

meterd_CFLAGS =			@LIBCONFIG_CFLAGS@ @SQLITE3_CFLAGS@ @PTHREAD_CFLAGS@

meterd_createdb_SOURCES =	meterd_createdb.c \
				meterd_log.c \
				meterd_log.h \
				meterd_config.c \
				meterd_config.h \
				db.c \
				db.h \
				utlist.h

meterd_createdb_CFLAGS =	@LIBCONFIG_CFLAGS@ @SQLITE3_CFLAGS@

meterd_createdb_LDADD =		@LIBCONFIG_LIBS@ @SQLITE3_LDFLAGS@

meterd_output_SOURCES =		meterd_output.c \
				meterd_log.c \
				meterd_log.h \
				meterd_config.c \
				meterd_config.h \
				db.c \
				db.h \
				utlist.h

meterd_output_CFLAGS =		@LIBCONFIG_CFLAGS@ @SQLITE3_CFLAGS@

meterd_output_LDADD =		@LIBCONFIG_LIBS@ @SQLITE3_LDFLAGS@

testp1_parse_SOURCES =		testp1_parse.c \
				p1_parser.c \
				p1_parser.h

testp1_parse_CFLAGS =		-DCMD_OUT

