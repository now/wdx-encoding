#include "stdafx.h"
#include "line-endings.h"
#include "encoding.h"

#include <strsafe.h>

/* A function determining if a string of bytes uses a given encoding. */
typedef BOOL (*IsEncodingFunc)(unsigned char const *, size_t);

/* An encoding.
 *
 * NAME is the name of the encoding, such as UTF-8 or similar.
 * IS_ENCODING is the function used by this encoding to check if it matches.
 * GETC is the function for reading characters in this encoding. */
struct _Encoding
{
	char const * const name;
	char const * const iconv_name;
	char const * const bom;
	IsEncodingFunc is_encoding;
	GetCharacterFunc getc;
};

/* Byte orders (used for UTF-16). */
typedef enum ByteOrder {
	ByteOrderBigEndian,
	ByteOrderLittleEndian
};

/* Simple flags for bytes that occur in various byte encodings that
 * we test for.
 *
 * F doesnt appear in any of the byte encodings.
 * T occurs in ASCII.
 * X occurs in non-ISO-extened ASCIIs. 
 * I occurs in ISO-8859-* encodings.
 */
typedef enum ByteEncodingType
{
	F,
	T,
	X,
	I
};

/* A table of ByteEncodingTypes for determining if a byte is encoded
 * using a certain byte encoding. */
static ByteEncodingType ByteEncodings[] = { 
    F, F, F, F, F, F, F, T, T, T, T, F, T, T, F, F,  
    F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  
    T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  
    X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,  
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I
};

/* A function that tests if a byte is encoded using a certain byte encoding. */
typedef BOOL (*IsByteEncodingFunc)(unsigned char);

/* Determines if a BYTE is encoded using ASCII. */
static BOOL
is_ascii(unsigned char byte)
{
	return ByteEncodings[byte] == T;
}

/* Determines if a BYTE is encoded using an ISO-8859-* encoding. */
static BOOL
is_iso8859(unsigned char byte)
{
	return is_ascii(byte) || ByteEncodings[byte] == I;
}

/* Determines if a BYTE is encoded using a non-ISO-extended ASCII. */
static BOOL
is_noniso(unsigned char byte)
{
	return is_iso8859(byte) || ByteEncodings[byte] == X;
}

/* Checks if it looks like N_BYTES of BYTES are encoded using IS_ENCODING. */
static BOOL
looks_like(unsigned char const * const bytes, size_t n_bytes,
		   IsByteEncodingFunc is_encoding)
{
	unsigned char const *end = bytes + n_bytes;

	for (unsigned char const *p = bytes; p < end; p++)
		if (g_get_value_aborted || !is_encoding(*p))
			return FALSE;

	return TRUE;
}

/* The following functions are the functions used for determining if
 * a string of bytes looks to be encoded using a given encoding. */

/* Determines whether it looks like N_BYTES of BYTES are encoded using
 * ASCII. */
static BOOL
looks_like_ascii(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like(bytes, n_bytes, is_ascii);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using
 * ISO-8859-1. */
static BOOL
looks_like_iso8859(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like(bytes, n_bytes, is_iso8859);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using
 * non-ISO-extended ASCII. */
static BOOL
looks_like_noniso(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like(bytes, n_bytes, is_noniso);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-8,
 * checking, if it is, if it begins with a byte-order mark (BOM). */
static BOOL
looks_like_utf8(unsigned char const * const bytes, size_t n_bytes, BOOL want_bom)
{
	BOOL got_one = FALSE;
	BOOL had_bom = FALSE;

	if (want_bom) {
		/* I dont know if I like this way of testing it. */
		if (n_bytes < 3 || bytes[0] != 0xef || bytes[1] != 0xbb || bytes[2] != 0xbf)
			return FALSE;

		had_bom = TRUE;
	}

	unsigned char const *end = bytes + n_bytes;
	for (unsigned char const *p = bytes; p < end; p++) {
		if (g_get_value_aborted)
			return FALSE;

		unsigned char byte = *p;

		if ((byte & 0x80) == 0) {
			if (!is_ascii(byte))
				return FALSE;
		} else if ((byte & 0x40) == 0) {
			return FALSE;
		} else {
			int following = 0;
			if ((byte & 0x20) == 0)
				following = 1;
			else if ((byte & 0x10) == 0)
				following = 2;
			else if ((byte & 0x08) == 0)
				following = 3;
			else if ((byte & 0x04) == 0)
				following = 4;
			else if ((byte & 0x02) == 0)
				following = 5;
			else
				return FALSE;

			for (int j = 0; j < following; j++) {
				p++;
				if (p >= end)
					return got_one;

				byte = *p;
				if ((byte & 0x80) == 0 || (byte & 0x40) == 1)
					return FALSE;
			}

			got_one = TRUE;
		}
	}

	return got_one && (want_bom ? had_bom : TRUE);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-8,
 * beginning with a byte-order mark (BOM). */
static BOOL
looks_like_utf8_with_bom(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like_utf8(bytes, n_bytes, TRUE);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-8,
 * without a byte-order mark (BOM). */
static BOOL
looks_like_utf8_without_bom(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like_utf8(bytes, n_bytes, FALSE);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-16,
 * in BYTE_ORDER. */
static BOOL
looks_like_utf16(unsigned char const * const bytes, size_t n_bytes,
				 ByteOrder byte_order)
{
	if (n_bytes < 2 || n_bytes % 2 != 0)
		return FALSE;

	unsigned char byte0 = bytes[0];
	unsigned char byte1 = bytes[1];

	bool has_bom_big = (byte0 == 0xfe && byte1 == 0xff);
	bool has_bom_little = (byte0 == 0xff && byte1 == 0xfe);

	if (!(has_bom_big || has_bom_little) ||
		(has_bom_big && byte_order != ByteOrderBigEndian) ||
		(has_bom_little && byte_order != ByteOrderLittleEndian))
		  return FALSE;

	unsigned char const *end = bytes + n_bytes;
	for (unsigned char const *p = bytes; p < end; p += 2) {
		if (g_get_value_aborted)
			return FALSE;

		byte0 = p[0];
		byte1 = p[1];

		if (byte_order == ByteOrderBigEndian) {
			unsigned char t = byte0;
			byte0 = byte1;
			byte1 = t;
		}

		int c = byte0 + 256 * byte1;
		if (c == 0xfffe || (c < 128 && !is_ascii((unsigned char)c)))
			return FALSE;
	}
	
	return TRUE;
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-16,
 * in big-endian byte order. */
static BOOL
looks_like_utf16be(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like_utf16(bytes, n_bytes, ByteOrderBigEndian);
}

/* Determines whether it looks like N_BYTES of BYTES are encoded using UTF-16,
 * in little-endian byte order. */
static BOOL
looks_like_utf16le(unsigned char const * const bytes, size_t n_bytes)
{
	return looks_like_utf16(bytes, n_bytes, ByteOrderLittleEndian);
}

/* This is a NULL IsEncodingFunc that always returns TRUE. */
static BOOL
looks_like_unknown(unsigned char const * const bytes, size_t n_bytes)
{
	UNREFERENCED_PARAMETER(bytes);
	UNREFERENCED_PARAMETER(n_bytes);

	return TRUE;
}

/* Gets the next unichar from a string of characters encoded using ASCII. */
static unichar
getc_ascii(CharacterIterator *iterator)
{
	if (iterator->p >= iterator->end)
		return UNICHAR_EOF;

	return *(iterator->p++);
}

/* Gets the next unichar from a string of characters encoded using UTF-8. */
static unichar
getc_utf8(CharacterIterator *iterator)
{
	if (iterator->p >= iterator->end)
		return UNICHAR_EOF;

	int c = *(iterator->p++);

	if (c & 0x80) {
		int n = 1;
		while (c & (0x80 >> n))
			n++;

		c &= (1 << (8 - n)) - 1;

		while (--n > 0) {
			if (iterator->p >= iterator->end)
				return UNICHAR_EOF;

			int t = *(iterator->p++);
			if ((t & 0x80) == 0 || (t & 0x40) == 1)
				return UNICHAR_EOF;

			c = (c << 6) | (t & 0x3f);
		}
	}

	return c;
}

/* Gets the next unichar from a string of characters encoded using UTF-16BE. */
static unichar
getc_utf16be(CharacterIterator *iterator)
{
	if (iterator->p >= iterator->end - 1)
		return UNICHAR_EOF;

	unsigned char byte0 = *(iterator->p++);
	unsigned char byte1 = *(iterator->p++);

	return byte1 + 256 * byte0;
}

/* Gets the next unichar from a string of characters encoded using UTF-16LE. */
static unichar
getc_utf16le(CharacterIterator *iterator)
{
	if (iterator->p >= iterator->end - 1)
		return UNICHAR_EOF;

	unsigned char byte0 = *(iterator->p++);
	unsigned char byte1 = *(iterator->p++);

	return byte0 + 256 * byte1;
}

/* Gets the next unichar from a string of unknown encoding,
 * thus always returning UNICHAR_EOF. */
static unichar
getc_unknown(CharacterIterator *iterator)
{
	return UNICHAR_EOF;
}

/* Gets the unmodifiable name of the given ENCODING. */
char const *
EncodingName(Encoding const *encoding)
{
	return encoding->name;
}

/* Gets the LineEnding of N_BYTES of BYTES encoded using ENCODING. */
LineEnding
EncodingLineEndings(Encoding const *encoding, unsigned char const * const bytes, size_t n_bytes)
{
	return LineEndingFind(bytes, n_bytes, encoding->getc);
}

char const *
EncodingIconvName(Encoding const *encoding)
{
	return encoding->iconv_name;
}

char const *
EncodingBOM(Encoding const *encoding)
{
	return encoding->bom;
}

/* These are the encodings that we can try to detect. */
Encoding encodings[] = {
	{ "ASCII", "ASCII", "", looks_like_ascii, getc_ascii },
	{ "UTF-8 / BOM", "UTF-8", "\357\273\277", looks_like_utf8_with_bom, getc_utf8 },
	{ "UTF-8", "UTF-8", "", looks_like_utf8_without_bom, getc_utf8 },
	{ "UTF-16BE", "UTF-16BE", "\376\377", looks_like_utf16be, getc_utf16be },
	{ "UTF-16LE", "UTF-16LE", "\377\376", looks_like_utf16le, getc_utf16le },
	{ "ISO-8859", "ISO-8859-1", "", looks_like_iso8859, getc_ascii },
	{ "ASCII++", NULL, "", looks_like_noniso, getc_ascii },
	{ "Unknown", NULL, "", looks_like_unknown, getc_unknown }
};

/* Iterates over each defined encoding using ITERATOR, passing it
 * CLOSURE along with the encoding. */
void
EncodingsEach(EncodingsIterator iterator, VOID *closure)
{
	for (int i = 0; i < _countof(encodings); i++)
		if (!iterator(&encodings[i], closure))
			return;
}

/* Closure used for finding an Encoding.
 *
 * BYTES is the string of bytes we need to check against.
 * N_BYTES is the number of bytes in BYTES.
 * ENCODING will point to the encoding found. */
typedef struct _EncodingFindClosure EncodingFindClosure;

struct _EncodingFindClosure
{
	unsigned char const *bytes;
	size_t n_bytes;
	Encoding const *encoding;
};

/* Iterator for finding an ENCODING for CLOSURE; breaks as soon as
 * one is found. */
static BOOL
EncodingFindIterator(Encoding const *encoding, VOID *closure)
{
	EncodingFindClosure *find_closure = (EncodingFindClosure *)closure;

	if (!encoding->is_encoding(find_closure->bytes, find_closure->n_bytes))
		return TRUE;

	find_closure->encoding = encoding;

	return FALSE;
}

/* Finds an Encoding for N_BYTES of BYTES. */
Encoding const *
EncodingFind(unsigned char const * const bytes, size_t n_bytes)
{
	EncodingFindClosure closure = { bytes, n_bytes, NULL };

	EncodingsEach(EncodingFindIterator, &closure);

	return closure.encoding;
}

Encoding const *
EncodingsGet(unsigned int index)
{
	if (index > _countof(encodings))
		return NULL;

	return &encodings[index];
}
