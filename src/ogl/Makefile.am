if CLIENT_INCLUDED
noinst_LTLIBRARIES = libGLKit.la
endif

AM_CPPFLAGS = \
	$(SDL_CFLAGS) \
	$(CONF_CPPFLAGS) \
	-I$(top_srcdir)/include
AM_CFLAGS = $(CONF_CFLAGS)
AM_CXXFLAGS = $(CONF_CXXFLAGS)

MAINTAINERCLEANFILES = \
	Makefile.in

libGLKit_la_LDFLAGS = -static
libGLKit_la_SOURCES =			\
	OpenGLAPI.cxx			\
	OpenGLFramebuffer.cxx		\
	OpenGLGState.cxx		\
	OpenGLLight.cxx			\
	OpenGLMaterial.cxx		\
	OpenGLTexture.cxx		\
	OpenGLUtils.cxx		\
	RenderNode.cxx

EXTRA_DIST = \
	README
