#include "stdafx.h"
#include "line-endings.h"

#include <strsafe.h>

/* Obvious names for a couple of Unicode characters. */
#define UNICODE_NEXT_LINE		0x0085
#define UNICODE_LINE_SEPARATOR	0x2028

/* Gets the LineEnding associated with C. */
static LineEnding
unichar_to_line_ending(unichar c)
{
	switch (c) {
	case '\r':
		return LineEndingCR;
	case '\n':
		return LineEndingLF;
	case UNICODE_NEXT_LINE:
		return LineEndingNEL;
	case UNICODE_LINE_SEPARATOR:
		return LineEndingLS;
	}

	return LineEndingUnknown;
}

/* Figure out what LineEnding is being used in N_BYTES of BYTES,
 * using GETC to retrieve unichar characters from those BYTES. */
LineEnding
LineEndingFind(unsigned char const * const bytes, size_t n_bytes, GetCharacterFunc getc)
{
	CharacterIterator iterator = { bytes, bytes + n_bytes, getc };

	while (iterator.p < iterator.end) {
		if (g_get_value_aborted)
			break;

		unichar c = iterator.getc(&iterator);
		if (c == UNICHAR_EOF)
			break;

		LineEnding line_ending = unichar_to_line_ending(c);
		switch (line_ending) {
		case LineEndingUnknown:
			break;
		case LineEndingCR:
			c = iterator.getc(&iterator);
			if (c == UNICHAR_EOF)
				return LineEndingCR;
			return (unichar_to_line_ending(c) == LineEndingLF) ?
					LineEndingCRLF : LineEndingCR;
		default:
			return line_ending;
		}
	}

	return LineEndingUnknown;
}
