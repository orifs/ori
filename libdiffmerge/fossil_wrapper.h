
#define fossil_malloc(_X) malloc(_X)
#define fossil_free free
#define fossil_realloc realloc
#define fossil_exit exit
#define fossil_system system

#define fossil_fatal_recursive fossil_fatal

void fossil_panic(const char *fmt, ...);
void fossil_fatal(const char *fmt, ...);
void fossil_warning(const char *fmt, ...);

