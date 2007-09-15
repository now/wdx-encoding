/* An opaque structure, keeping information about an Encoding. */
typedef struct _Encoding Encoding;

/* An iterator over Encodings. */
typedef BOOL (*EncodingsIterator)(Encoding const *, VOID *closure);

VOID EncodingsEach(EncodingsIterator iterator, VOID *closure);

Encoding const *EncodingFind(unsigned char const * const bytes, size_t n_bytes);

char const *EncodingName(Encoding const *encoding);
LineEnding EncodingLineEndings(Encoding const *encoding, unsigned char const * const bytes, size_t n_bytes);
Encoding const *EncodingsGet(unsigned int index);
char const *EncodingIconvName(Encoding const *encoding);
char const *EncodingBOM(Encoding const *encoding);
