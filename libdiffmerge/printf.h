
/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
int fossil_stricmp(const char *zA,const char *zB);
int fossil_strnicmp(const char *zA,const char *zB,int nByte);
int fossil_strncmp(const char *zA,const char *zB,int nByte);
int fossil_strcmp(const char *zA,const char *zB);
void cgi_vprintf(const char *zFormat,va_list ap);
void fossil_print(const char *zFormat,...);
int fossil_utf8_to_console(const char *zUtf8,int nByte,int toStdErr);
void fossil_puts(const char *z,int toStdErr);
void fossil_error_reset(void);
void fossil_error(int iPriority,const char *zFormat,...);
char *vmprintf(const char *zFormat,va_list ap);
char *mprintf(const char *zFormat,...);
#define WIKI_INLINE         0x004  /* Do not surround with <p>..</p> */
void wiki_convert(Blob *pIn,Blob *pOut,int flags);
char *fossilize(const char *zIn,int nIn);
char *urlize(const char *z,int n);
char *httpize(const char *z,int n);
char *htmlize(const char *zIn,int n);
int vxprintf(Blob *pBlob,const char *fmt,va_list ap);
