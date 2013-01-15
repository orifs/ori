
#undef INTERFACE
void test_obscure_cmd(void);
char *unobscure(const char *zIn);
char *obscure(const char *zIn);
void canonical16(char *z,int n);
int validate16(const char *zIn,int nIn);
int decode16(const unsigned char *zIn,unsigned char *pOut,int N);
int encode16(const unsigned char *pIn,unsigned char *zOut,int N);
void test_decode64_cmd(void);
char *decode64(const char *z64,int *pnByte);
void fossil_print(const char *zFormat,...);
void test_encode64_cmd(void);
char *encode64(const char *zData,int nData);
void defossilize(char *z);
int fossil_isspace(char c);
char *fossilize(const char *zIn,int nIn);
int dehttpize(char *z);
char *urlize(const char *z,int n);
char *httpize(const char *z,int n);
int fossil_isalnum(char c);
#define count(X)  (sizeof(X)/sizeof(X[0]))
char *htmlize(const char *zIn,int n);
