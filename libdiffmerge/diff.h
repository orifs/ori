
#undef INTERFACE
int file_tree_name(const char *zOrigName,Blob *pOut,int errFatal);
void style_footer(void);
void cgi_printf(const char *zFormat,...);
void style_header(const char *zTitleFormat,...);
const char *cgi_parameter(const char *zName,const char *zDefault);
int name_to_typed_rid(const char *zName,const char *zType);
char *mprintf(const char *zFormat,...);
void compute_direct_ancestors(int rid,int N);
int name_to_rid(const char *zName);
int content_get(int rid,Blob *pBlob);
void fossil_print(const char *zFormat,...);
const char *find_option(const char *zLong,const char *zShort,int hasArg);
int diff_options(void);
int *text_diff(Blob *pA_Blob,Blob *pB_Blob,Blob *pOut,uint64_t diffFlags);
int diff_width(uint64_t diffFlags);
int diff_context_lines(uint64_t diffFlags);
void *fossil_realloc(void *p,size_t n);
void fossil_free(void *p);
char *htmlize(const char *zIn,int n);
int looks_like_binary(Blob *pContent);
int fossil_isspace(char c);
void *fossil_malloc(size_t n);
#define DIFF_CANNOT_COMPUTE_SYMLINK \
    "cannot compute difference between symlink and regular file\n"
#define DIFF_CANNOT_COMPUTE_BINARY \
    "cannot compute difference between binary files\n"
#define DIFF_INVERT       (((uint64_t)0x02)<<32) /* Invert the diff (debug) */
#define DIFF_NOOPT        (((uint64_t)0x01)<<32) /* Suppress optimizations (debug) */
#define DIFF_WS_WARNING   ((uint64_t)0x40000000) /* Warn about whitespace */
#define DIFF_LINENO       ((uint64_t)0x20000000) /* Show line numbers */
#define DIFF_HTML         ((uint64_t)0x10000000) /* Render for HTML */
#define DIFF_INLINE       ((uint64_t)0x00000000) /* Inline (not side-by-side) diff */
#define DIFF_BRIEF        ((uint64_t)0x08000000) /* Show filenames only */
#define DIFF_NEWFILE      ((uint64_t)0x04000000) /* Missing shown as empty files */
#define DIFF_SIDEBYSIDE   ((uint64_t)0x02000000) /* Generate a side-by-side diff */
#define DIFF_IGNORE_EOLWS ((uint64_t)0x01000000) /* Ignore end-of-line whitespace */
#define DIFF_WIDTH_MASK   ((uint64_t)0x00ff0000) /* side-by-side column width */
#define DIFF_CONTEXT_MASK ((uint64_t)0x0000ffff) /* Lines of context. Default if 0 */
#define INTERFACE 0

