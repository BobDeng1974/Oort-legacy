NULL = 

AM_CFLAGS = \
	$(OORT_CFLAGS) \
	-include $(CONFIG_HEADER) \
	-I $(top_srcdir)/vector \
	-I $(top_srcdir)/sim \
	$(LUA_CFLAGS) \
	-include oort.h \
	$(NULL)

AM_VALAFLAGS = \
	--pkg oort \
	--pkg lua \
	--pkg vector \
	--pkg util \
	--pkg gio-2.0 \
	--pkg config \
	--vapidir $(top_srcdir)/vapi \
	--vapidir $(top_srcdir)/vector \
	--vapidir $(top_srcdir)/sim \
	$(NULL)

LIBS=$(OORT_LIBS) -lm

bin_PROGRAMS = oort_dedicated
oort_dedicated_SOURCES = oort-dedicated.vala
oort_dedicated_LDADD = $(top_srcdir)/sim/libsim.a $(CJSON_LDADD) $(LUA_LDADD)
oort_dedicated_LDFLAGS = $(OORT_EXTRA_LDFLAGS)
