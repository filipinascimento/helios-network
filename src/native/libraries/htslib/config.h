#ifndef HTSLIB_MINIMAL_CONFIG_H
#define HTSLIB_MINIMAL_CONFIG_H

/* Disable optional backends that pull additional dependencies. */
#undef ENABLE_GCS
#undef ENABLE_PLUGINS
#undef ENABLE_S3

/* Optional compression libraries not bundled with Helios. */
#undef HAVE_LIBBZ2
#undef HAVE_LIBCURL
#undef HAVE_LIBDEFLATE
#undef HAVE_LIBLZMA

/* Zlib is required and linked elsewhere in the build. */
#define HAVE_LIBZ 1

/* POSIX helpers used by htslib; disable on targets that do not provide them. */
#ifdef __EMSCRIPTEN__
#define HAVE_POSIX_MEMALIGN 0
#else
#define HAVE_POSIX_MEMALIGN 1
#endif

/* Provide a simple version string to satisfy htslib interfaces. */
#define PACKAGE_VERSION "helios-embedded-htslib"

#endif /* HTSLIB_MINIMAL_CONFIG_H */
