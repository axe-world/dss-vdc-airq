ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = vdc-airq
vdc_airq_SOURCES = main.c network.c configuration.c vdsd.c util.c icons.c airq.h incbin.h

vdc_airq_CFLAGS = \
    $(PTHREAD_CFLAGS) \
    $(LIBCONFIG_CFLAGS) \
    $(JSONC_CFLAGS) \
    $(CRYPTO_CFLAGS) \
    $(SSL_CFLAGS) \
    $(CURL_CFLAGS) \
    $(LIBDSVDC_CFLAGS) \
    $(LIBDSUID_CFLAGS)

vdc_airq_LDADD = \
    $(PTHREAD_LIBS) \
    $(LIBCONFIG_LIBS) \
    $(JSONC_LIBS) \
    $(CRYPTO_LIBS) \
    $(SSL_LIBS) \
    $(CURL_LIBS) \
    $(LIBDSVDC_LIBS) \
    $(LIBDSUID_LIBS)
