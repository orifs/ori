
#undef INTERFACE

typedef struct Blob Blob;
void blob_swap(Blob *pLeft,Blob *pRight);
unsigned int blob_read(Blob *pIn,void *pDest,unsigned int nLen);
void shell_escape(Blob *pBlob,const char *zIn);
void blob_remove_cr(Blob *p);
#if defined(_WIN32)
void blob_add_cr(Blob *p);
#endif
int blob_uncompress(Blob *pIn,Blob *pOut);
void blob_compress2(Blob *pIn1,Blob *pIn2,Blob *pOut);

struct Blob {
  unsigned int nUsed;            /* Number of bytes used in aData[] */
  unsigned int nAlloc;           /* Number of bytes allocated for aData[] */
  unsigned int iCursor;          /* Next character of input to parse */
  char *aData;                   /* Where the information is stored */
  void (*xRealloc)(Blob*, unsigned int); /* Function to reallocate the buffer */
};

void blob_compress(Blob *pIn,Blob *pOut);
int file_mkdir(const char *zName,int forceFlag);
int file_simplify_name(char *z,int n,int slash);
char *mprintf(const char *zFormat,...);
int fossil_utf8_to_console(const char *zUtf8,int nByte,int toStdErr);
int blob_write_to_file(Blob *pBlob,const char *zFilename);
int blob_read_link(Blob *pBlob,const char *zFilename);
FILE *fossil_fopen(const char *zName,const char *zMode);
int64_t file_wd_size(const char *zFilename);
int blob_read_from_file(Blob *pBlob,const char *zFilename);
int blob_read_from_channel(Blob *pBlob,FILE *in,int nToRead);
void blob_vappendf(Blob *pBlob,const char *zFormat,va_list ap);
int vxprintf(Blob *pBlob,const char *fmt,va_list ap);
void blob_appendf(Blob *pBlob,const char *zFormat,...);
int blob_tokenize(Blob *pIn,Blob *aToken,int nToken);
void blobarray_reset(Blob *aBlob,int n);
void blobarray_zero(Blob *aBlob,int n);
int blob_is_int(Blob *pBlob,int *pValue);
int blob_is_uuid_n(Blob *pBlob,int n);
int validate16(const char *zIn,int nIn);
#define UUID_SIZE 40
int blob_is_uuid(Blob *pBlob);
void blob_copy_lines(Blob *pTo,Blob *pFrom,int N);
int blob_tail(Blob *pFrom,Blob *pTo);
int blob_sqltoken(Blob *pFrom,Blob *pTo);
int blob_token(Blob *pFrom,Blob *pTo);
int blob_trim(Blob *p);
int blob_line(Blob *pFrom,Blob *pTo);
int blob_tell(Blob *p);
int blob_seek(Blob *p,int offset,int whence);
void blob_rewind(Blob *p);
int blob_extract(Blob *pFrom,int N,Blob *pTo);
int dehttpize(char *z);
void blob_dehttpize(Blob *pBlob);
void blob_resize(Blob *pBlob,unsigned int newSize);
# define blob_eq(B,S) \
     ((B)->nUsed==sizeof(S)-1 && memcmp((B)->aData,S,sizeof(S)-1)==0)
int blob_eq_str(Blob *pBlob,const char *z,int n);
int blob_constant_time_cmp(Blob *pA,Blob *pB);
int blob_compare(Blob *pA,Blob *pB);
char *blob_terminate(Blob *p);
char *blob_materialize(Blob *pBlob);
char *blob_str(Blob *p);
void blob_copy(Blob *pTo,Blob *pFrom);
void blob_append(Blob *pBlob,const char *aData,int nData);
void blob_zero(Blob *pBlob);
void blob_set(Blob *pBlob,const char *zStr);
void blob_init(Blob *pBlob,const char *zData,int size);
int blob_is_reset(Blob *pBlob);
void blob_reset(Blob *pBlob);
extern const Blob empty_blob;
void blobReallocMalloc(Blob *pBlob,unsigned int newSize);
#define BLOB_INITIALIZER  {0,0,0,0,blobReallocMalloc}
void fossil_print(const char *zFormat,...);
void isspace_cmd(void);
int fossil_isalnum(char c);
int fossil_isalpha(char c);
int fossil_toupper(char c);
int fossil_tolower(char c);
int fossil_isdigit(char c);
int fossil_isupper(char c);
int fossil_islower(char c);
int fossil_isspace(char c);
#define BLOB_SEEK_END 3
#define BLOB_SEEK_CUR 2
#define BLOB_SEEK_SET 1
#define blob_buffer(X)  ((X)->aData)
#define blob_size(X)  ((X)->nUsed)
#define INTERFACE 0
