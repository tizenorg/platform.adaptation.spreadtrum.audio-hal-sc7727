lib_LTLIBRARIES = libtizen-audio.la

libtizen_audio_la_SOURCES = tizen-audio.c \
		tizen-audio-volume.c \
		tizen-audio-routing.c \
		tizen-audio-stream.c \
		tizen-audio-pcm.c \
		tizen-audio-modem.c \
		tizen-audio-comm.c \
		tizen-audio-util.c \
		tizen-audio-impl-pcm.c \
		tizen-audio-impl-ucm.c \
		tizen-audio-impl-ctrl.c
libtizen_audio_la_LDFLAGS = $(AM_LDFLAGS) -disable-static -avoid-version
libtizen_audio_la_LIBADD = $(AM_LDADD) $(ASOUNDLIB_LIBS) $(VCONF_LIBS) $(DLOG_LIBS) $(INIPARSER_LIBS) $(EXPAT_LIBS)
libtizen_audio_la_CFLAGS = $(AM_CFLAGS) $(ASOUNDLIB_CFLAGS) $(VCONF_CFLAGS) $(DLOG_CFLAGS) $(INIPARSER_CFLAGS) $(EXPAT_CFLAGS) -DUSE_DLOG

if USE_TINYALSA
libtizen_audio_la_LIBADD += $(TINYALSA_LIBS)
libtizen_audio_la_CFLAGS += $(TINYALSA_CFLAGS) -D__USE_TINYALSA__
endif
