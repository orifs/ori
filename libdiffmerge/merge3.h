
#undef INTERFACE
int blob_merge(Blob *pPivot, Blob *pV1, Blob *pV2, Blob *pOut);
int merge_3way(Blob *pPivot,const char *zV1,Blob *pV2,Blob *pOut);
char *string_subst(const char *zInput,int nSubst,const char **azSubst);
int *text_diff(Blob *pA_Blob,Blob *pB_Blob,Blob *pOut,uint64_t diffFlags);

