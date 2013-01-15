
/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <dirent.h>
int fossil_utf8_to_console(const char *zUtf8,int nByte,int toStdErr);
char *fossil_utf8_to_mbcs(const char *zUtf8);
char *fossil_mbcs_to_utf8(const char *zMbcs);
#if defined(_WIN32)
# define closedir _wclosedir
# define readdir _wreaddir
# define opendir _wopendir
# define DIR _WDIR
# define dirent _wdirent
#endif
#define INTERFACE 0
int file_is_the_same(Blob *pContent,const char *zName);
char *fossil_getenv(const char *zName);
void file_tempname(int nBuf,char *zBuf);
void file_parse_uri(const char *zUri,Blob *pScheme,Blob *pHost,int *pPort,Blob *pPath);
void cmd_test_tree_name(void);
int file_tree_name(const char *zOrigName,Blob *pOut,int errFatal);
void cmd_test_relative_name(void);
int fossil_tolower(char c);
char *file_without_drive_letter(char *zIn);
int file_is_canonical(const char *z);
void cmd_test_canonical_name(void);
int fossil_toupper(char c);
int fossil_isalpha(char c);
void file_canonical_name(const char *zOrigName,Blob *pOut,int slash);
int file_is_absolute_path(const char *zPath);
char *fossil_unicode_to_utf8(const void *zUnicode);
void file_getcwd(char *zBuf,int nBuf);
void cmd_test_simplify_name(void);
int file_is_simple_pathname(const char *z);
void file_delete(const char *zFilename);
void fossil_print(const char *zFormat,...);
void test_set_mtime(void);
void file_set_mtime(const char *zFilename,int64_t newMTime);
int file_wd_setexe(const char *zFilename,int onoff);
FILE *fossil_fopen(const char *zName,const char *zMode);
void file_copy(const char *zFrom,const char *zTo);
const char *file_tail(const char *z);
void file_relative_name(const char *zOrigName,Blob *pOut,int slash);
void fossil_free(void *p);
char *file_newname(const char *zBase,const char *zSuffix,int relFlag);
int file_access(const char *zFilename,int flags);
int file_wd_isdir(const char *zFilename);
int file_isdir(const char *zFilename);
int file_wd_islink(const char *zFilename);
int file_wd_isexe(const char *zFilename);
#define PERM_LNK          2     /*  symlink       */
#define PERM_EXE          1     /*  executable    */
#define PERM_REG          0     /*  regular file  */
int file_wd_perm(const char *zFilename);
void symlink_copy(const char *zFrom,const char *zTo);
int file_mkdir(const char *zName,int forceFlag);
int file_simplify_name(char *z,int n,int slash);
char *mprintf(const char *zFormat,...);
void symlink_create(const char *zTargetFile,const char *zLinkFile);
int file_wd_isfile(const char *zFilename);
int file_isfile(const char *zFilename);
int file_wd_isfile_or_link(const char *zFilename);
int64_t file_wd_mtime(const char *zFilename);
int64_t file_mtime(const char *zFilename);
int64_t file_wd_size(const char *zFilename);
int64_t file_size(const char *zFilename);
void fossil_mbcs_free(void *zOld);
void *fossil_utf8_to_unicode(const char *zUtf8);
