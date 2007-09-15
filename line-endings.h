/* The types of line endings we can detect. */
typedef enum LineEnding
{
	LineEndingUnknown,
	LineEndingLF,
	LineEndingCRLF,
	LineEndingCR,
	LineEndingLS,
	LineEndingNEL,
};

/* The type of Unicode characters. */
typedef int unichar;

#define UNICHAR_EOF	((unichar)-1)

/* A character iterator for stepping through a string of bytes,
 * retrieving characters.
 *
 * P points to the current byte in the string, that is, where the iterator is.
 * END points one byte beyond the last byte of the string.
 * GETC should get you the next unichar in the stream of characters. */
typedef struct _CharacterIterator CharacterIterator;

/* A function retrieving the next unichar from the characters provided by the
 * given CharacterIterator. */
typedef unichar (*GetCharacterFunc)(CharacterIterator *);

struct _CharacterIterator
{
	unsigned char const *p;
	unsigned char const *end;
	GetCharacterFunc getc;
};

LineEnding LineEndingFind(unsigned char const * const bytes, size_t n_bytes, GetCharacterFunc getc);
