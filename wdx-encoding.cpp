#include "stdafx.h"
#include "content-plugin.h"
#include "wdx-encoding.h"
#include "line-endings.h"
#include "encoding.h"

#include <strsafe.h>

/* Will be set to true ContentStopGetValue() if the ContentGetValue()
 * procedure should be aborted as soon as possible. */
BOOL g_get_value_aborted;

/* The maximum number of bytes to map of a file. */
#define MAX_MAP_SIZE	(256 * 1024)

/* The file name that the cached data of the fields refers to. */
static char *s_cached_filename;

/* The names of line endings. */
static char const * const line_ending_names[] = {
	"-",
	"LF",
	"CR+LF",
	"CR",
	"LS",
	"NEL",
};

typedef unsigned int iconv_t;

typedef iconv_t (*IconvOpenFunc)(char const *, char const *);
typedef size_t (*IconvFunc)(iconv_t, char const **, size_t *, char **, size_t *);
typedef int (*IconvCloseFunc)(iconv_t);

static IconvOpenFunc iconv_open;
static IconvFunc iconv;
static IconvCloseFunc iconv_close;

/* The indexes into the array of fields we provide. */
typedef enum FieldIndex
{
	FieldIndexEncoding,
	FieldIndexLineEnding,
};

/* A function associated with a field for setting that fields units. */
typedef void (*FieldSetUnitsFunc)(char *, int);

typedef TCFieldFlags (*FieldSetFlagsFunc)(void);

/* A field supplied by this plugin.
 *
 * NAME is the name of the field.
 * SET_UNITS is the function used for setting the fields units.
 * TYPE is the type of the field.
 * IS_SLOW specifies whether this field is slow to retrieve or not.
 * CACHED_DATA keeps the cached value for this field for S_CACHED_FILENAME. */
typedef struct _Field Field;

struct _Field
{
	const char * const name;
	FieldSetUnitsFunc set_units;
	TCFieldTypeOrStatus type;
	FieldSetFlagsFunc set_flags;
	bool is_slow;
	void const *cached_data;
};

/* Used as a helper method for joining strings together, separated
 * by | characters, as used by the content plugin interface for,
 * for example, units. */
static void
StringsJoin(char *joined, int size, char const *string)
{
	if (joined[0] == '\0') {
		StringCbCopy(joined, size, string);
		return;
	}

	StringCbCat(joined, size, "|");
	StringCbCat(joined, size, string);
}

/* A closure used when iterating over encodings to generate a units
 * specification for the Encoding field.
 *
 * UNITS keeps the joined string of units.
 * SIZE is the maximum number of bytes we can fit in UNITS. */
typedef struct _EncodingFieldSetUnitsClosure EncodingFieldSetUnitsClosure;

struct _EncodingFieldSetUnitsClosure
{
	char *units;
	int size;
};

/* Iterator creating the units specification for the Encoding field. */
BOOL
EncodingFieldSetUnitsIterator(Encoding const *encoding, VOID *void_closure)
{
	EncodingFieldSetUnitsClosure *closure = (EncodingFieldSetUnitsClosure *)void_closure;
	
	StringsJoin(closure->units, closure->size, EncodingName(encoding));

	return TRUE;
}

/* The FieldSetUnitsFunc used for the Encoding field. */
static void
EncodingFieldSetUnits(char *units, int size)
{
	EncodingFieldSetUnitsClosure closure = { units, size };

	EncodingsEach(EncodingFieldSetUnitsIterator, &closure);
}

/* The FieldSetUnitsFunc used for the Line Endings field. */
static void
LineEndingsFieldSetUnits(char *units, int size)
{
	for (int i = 0; i < _countof(line_ending_names); i++)
		StringsJoin(units, size, line_ending_names[i]);
}

static TCFieldFlags
EncodingFieldSetFlags(void)
{
	return TCFieldFlagsEdit | TCFieldFlagsSubstAttributeStr;
}

static TCFieldFlags
LineEndingsFieldSetFlags(void)
{
	return TCFieldFlagsEdit | TCFieldFlagsSubstAttributeStr;
}

/* These are the fields that this plugin provides. */
Field s_fields[] = {
	{ "Encoding", EncodingFieldSetUnits, TCFieldTypeMultipleChoice, EncodingFieldSetFlags, TRUE },
	{ "Line Endings", LineEndingsFieldSetUnits, TCFieldTypeMultipleChoice, LineEndingsFieldSetFlags, TRUE },
};

/* This function is called by Total Commander to retrieve information
 * about the fields provided by this plugin. */
TCFieldTypeOrStatus __stdcall
ContentGetSupportedField(int index, char *name, char *units, int size)
{
	/* I really dont know when INDEX would be less than 0, but the example
	 * plugin had this test in there, so we best keep it. (INDEX should be
	 * an unsigned int in my opinion.) */
	if (index < 0 || index >= _countof(s_fields))
		return TCFieldTypeNoMoreFields;

	/* Make sure that the string is empty, so that calling StringsJoin will
	 * work properly. */
	units[0] = '\0';

	Field field = s_fields[index];

	StringCbCopy(name, size, field.name);
	field.set_units(units, size);
	
	return field.type;
}

TCFieldFlags __stdcall
ContentGetSupportedFieldFlags(int index)
{
	if (index < 0 || index >= _countof(s_fields))
		return TCFieldFlagsEdit | TCFieldFlagsSubstMask;

	return s_fields[index].set_flags();
}

/* Checks if the cache contains an entry for FILENAME. */
static BOOL
CacheContains(char const *filename)
{
	return lstrcmpi(s_cached_filename, filename) == 0;
}

/* Clears the cached data retained in the fields. */
static void
CacheClear(void)
{
	for (size_t i = 0; i < _countof(s_fields); i++)
		s_fields[i].cached_data = NULL;

	if (s_cached_filename == NULL)
		return;

	HeapFree(GetProcessHeap(), 0, s_cached_filename);
	s_cached_filename = NULL;
}

/* Retrieves the given fields cached value, if one exists. */
static TCFieldTypeOrStatus
CacheGet(int field_index, void *field_value, int field_value_size)
{
	Field field = s_fields[field_index];

	if (field.cached_data == NULL)
		return TCFieldStatusFieldEmpty;

	/* TODO: Finish up with the other types of fields that we can have. */
	switch (field.type) {
#if 0
	case TCFieldTypeNumeric32:
		*((int *)field_value) = (int)field.cached_data;
		break;
	case TCFieldTypeNumeric64:
		*((__int64 *)field_value) = *((_int64 *)field.cached_data);
		break;
	case TCFieldTypeNumericFloating:
		*((double *)field_value) = (double)field.cached_data;
		break;
	case TCFieldTypeDate:
	case TCFieldTypeTime:
		break;
	case TCFieldTypeBoolean:
		*((BOOL *)field_value) = (BOOL)field.cached_data;
		break;
#endif
	case TCFieldTypeMultipleChoice:
#if 0
	case TCFieldTypeString:
	case TCFieldTypeFullText:
#endif
		if (!SUCCEEDED(StringCbCopy((char *)field_value, field_value_size,
									(char *)field.cached_data)))
			return TCFieldStatusFieldEmpty;
		break;
#if 0
	case TCFieldTypeDateTime:
		break;
#endif
	default:
		return TCFieldStatusFieldEmpty;
	}

	return field.type;
}

/* Stores field values in ENCODING associated with FILENAME in the cache. */ 
static void
CachePut(char const *filename, Encoding const *encoding, LineEnding line_ending)
{
	CacheClear();

	size_t filename_length = lstrlen(filename);
	s_cached_filename = (char *)HeapAlloc(GetProcessHeap(), 0, filename_length + 1);
	if (s_cached_filename == NULL)
		return;

	if (!SUCCEEDED(StringCbCopy(s_cached_filename, filename_length + 1, filename)))
		CacheClear();

	s_fields[FieldIndexEncoding].cached_data = EncodingName(encoding);
	s_fields[FieldIndexLineEnding].cached_data = line_ending_names[line_ending];
}

typedef struct _FileMapping FileMapping;

struct _FileMapping
{
	HANDLE file;
	HANDLE map;
	unsigned char const *bytes;
	size_t n_bytes;
};

static TCFieldTypeOrStatus
MapFile(char const *filename, FileMapping *mapping, size_t max_size)
{
	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
							 OPEN_EXISTING,
							 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
							 NULL);
	if (file == INVALID_HANDLE_VALUE)
		return TCFieldStatusFileError;

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(file, &file_size) ||
		(file_size.LowPart == 0 && file_size.HighPart == 0)) {
		/* TODO: This should be the Unknown encoding. */
		CloseHandle(file);
		return TCFieldStatusFieldEmpty;
	}

	size_t n_bytes = max_size == 0 ? file_size.LowPart : min(file_size.LowPart, max_size);
	HANDLE map = CreateFileMapping(file, NULL, PAGE_READONLY, 0, n_bytes, NULL);
	if (map == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(file);
		return TCFieldStatusFileError;
	}

	/* TODO: This crashes when a (CSV) file is locked by microsoft excel when importing data. */

	unsigned char *bytes = (unsigned char *)MapViewOfFile(map, FILE_MAP_READ,
														  0, 0, n_bytes);
	MEMORY_BASIC_INFORMATION mbi;
	if (bytes == NULL ||
		VirtualQuery(bytes, &mbi, sizeof(mbi)) < sizeof(mbi) ||
		mbi.State != MEM_COMMIT ||
		mbi.BaseAddress != bytes ||
		mbi.RegionSize < n_bytes) {
		CloseHandle(map);
		CloseHandle(file);
		return TCFieldStatusFileError;
	}

	mapping->file = file;
	mapping->map = map;
	mapping->bytes = bytes;
	mapping->n_bytes = n_bytes;

	return TCFieldStatusSetSuccess;
}

static void
UnmapFile(FileMapping *mapping)
{
	UnmapViewOfFile(mapping->bytes);
	CloseHandle(mapping->map);
	CloseHandle(mapping->file);
}

/* Called by Total Commander to get the value of field FIELD_INDEX for
 * FILENAME.  If units are being used for this field, UNIT_INDEX will
 * point to the unit that the user has chosen to display the field in.
 * FIELD_VALUE_SIZE is the maximum number of bytes we can store in
 * FIELD_VALUE.  FLAGS are any additional flags passed to us by Total
 * Commander, such as the request to delay the calculation of a fields
 * value if it is slow to calculate (see Field.is_slow). */
TCFieldTypeOrStatus __stdcall
ContentGetValue(char *filename, int field_index, int unit_index,
				void *field_value, int field_value_size, TCContentFlag flags)
{
	/* I really dont know when INDEX would be less than 0, but the example
	 * plugin had this test in there, so we best keep it. (INDEX should be
	 * an unsigned int in my opinion.) */
	if (field_index < 0 || field_index >= _countof(s_fields))
		return TCFieldTypeNoMoreFields;

	g_get_value_aborted = FALSE;

	if ((flags & TCContentFlagDelayIfSlow) && s_fields[field_index].is_slow)
		return TCFieldStatusDelayed;

	if (CacheContains(filename))
		return CacheGet(field_index, field_value, field_value_size);

	CacheClear();

	FileMapping mapping;
	TCFieldTypeOrStatus status = MapFile(filename, &mapping, MAX_MAP_SIZE);
	if (status != TCFieldStatusSetSuccess)
		return status;

	Encoding const *encoding = EncodingFind(mapping.bytes, mapping.n_bytes);

	CachePut(filename, encoding, EncodingLineEndings(encoding, mapping.bytes, mapping.n_bytes));

	UnmapFile(&mapping);

	return CacheGet(field_index, field_value, field_value_size);
}

/* Called by Total Commander when the user has elected to stop getting values
 * of fields provided by this plugin.  This is usually done when changing
 * directories or the user press Escape. */
void __stdcall
ContentStopGetValue(char *filename)
{
	UNREFERENCED_PARAMETER(filename);

	g_get_value_aborted = TRUE;
}

static void
UnloadIconv(HMODULE iconv_dll)
{
	iconv_open = NULL;
	iconv = NULL;
	iconv_close = NULL;

	FreeLibrary(iconv_dll);
	iconv_dll = NULL;
}

static HMODULE
LoadIconv(void)
{
	HMODULE iconv_dll = LoadLibrary("iconv.dll");
	if (iconv_dll == NULL)
		return NULL;

	iconv_open = (IconvOpenFunc)GetProcAddress(iconv_dll, "libiconv_open");
	iconv = (IconvFunc)GetProcAddress(iconv_dll, "libiconv");
	iconv_close = (IconvCloseFunc)GetProcAddress(iconv_dll, "libiconv_close");

	if (iconv_open != NULL && iconv != NULL && iconv_close != NULL)
		return iconv_dll;

	UnloadIconv(iconv_dll);

	return NULL;
}

static BOOL
GenerateTemporaryFileName(char *destination)
{
	/* According to MSDN, GetTempFileName doesnt allow LPPATHNAME to be more than
	 * MAX_PATH - 14 characters long.  It doesnt (of course, seeing as how this is
	 * MSDN) specify if this includes the terminating NULL, but lets assume that it
	 * doesnt. */
#	define TEMP_PATH_LENGTH (MAX_PATH - 14 + 1)
	char temp_path[TEMP_PATH_LENGTH];
	DWORD actual_length = GetTempPath(TEMP_PATH_LENGTH, temp_path);
	if (actual_length == 0 || actual_length > TEMP_PATH_LENGTH)
		return FALSE;

	if (GetTempFileName(temp_path, "ENC", 0, destination) == 0)
		return FALSE;

	return TRUE;
}

static TCFieldTypeOrStatus
IconvFile(char *filename, Encoding const *from, Encoding const *to)
{
	if (EncodingIconvName(from) == NULL || EncodingIconvName(to) == NULL)
		return TCFieldStatusFileError;

	char temp_file_name[MAX_PATH + 1];
	if (!GenerateTemporaryFileName(temp_file_name))
		return TCFieldStatusFileError;

	/* Why is there no STRSAFE_MAX_CB? */
	size_t from_bom_length, to_bom_length;
	if (FAILED(StringCbLength(EncodingBOM(from), STRSAFE_MAX_CCH, &from_bom_length)) ||
		FAILED(StringCbLength(EncodingBOM(to), STRSAFE_MAX_CCH, &to_bom_length)))
		return TCFieldStatusFileError;

	iconv_t cd = iconv_open(EncodingIconvName(to), EncodingIconvName(from));
	if (cd == (iconv_t)-1)
		return TCFieldStatusFileError;

	FileMapping input;
	TCFieldTypeOrStatus status = MapFile(filename, &input, 0);
	if (status != TCFieldStatusSetSuccess)
		return status;

	HANDLE output = CreateFile(temp_file_name, GENERIC_WRITE, 0, NULL,
							   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (output == INVALID_HANDLE_VALUE) {
		UnmapFile(&input);
		return TCFieldStatusFileError;
	}

#	define ICONVFILE_BUFFER_SIZE	(32768)
	char const *input_pointer = (char const *)input.bytes + from_bom_length;
	char output_buffer[ICONVFILE_BUFFER_SIZE];
	size_t remaining = input.n_bytes - from_bom_length;

	DWORD bytes_written;
	if (!WriteFile(output, EncodingBOM(to), to_bom_length, &bytes_written, NULL) ||
		bytes_written != to_bom_length) {
			UnmapFile(&input);
			CloseHandle(output);
			DeleteFile(temp_file_name);
			return TCFieldStatusFileError;
	}

	while (remaining > 0) {
		char *output_pointer = output_buffer;
		size_t output_bytes_remaining = _countof(output_buffer);

		size_t bytes_converted = iconv(cd, &input_pointer, &remaining, &output_pointer, &output_bytes_remaining);
		if (output_pointer != output_buffer) {
			DWORD bytes_to_write = output_pointer - output_buffer;
			if (!WriteFile(output, output_buffer, bytes_to_write, &bytes_written, NULL) ||
				bytes_written != bytes_to_write) {
					UnmapFile(&input);
					CloseHandle(output);
					DeleteFile(temp_file_name);
					return TCFieldStatusFileError;
			}
		}

		if (bytes_converted != (size_t)-1) {
			output_pointer = output_buffer;
			output_bytes_remaining = _countof(output_buffer);
			
			bytes_converted = iconv(cd, NULL, NULL, &output_pointer, &output_bytes_remaining);
			if (output_pointer != output_buffer) {
				DWORD bytes_to_write = output_pointer - output_buffer;
				if (!WriteFile(output, output_buffer, bytes_to_write, &bytes_written, NULL) ||
					bytes_written != bytes_to_write) {
						UnmapFile(&input);
						CloseHandle(output);
						DeleteFile(temp_file_name);
						return TCFieldStatusFileError;
				}
			}

			if (bytes_converted == (size_t)-1) {
				UnmapFile(&input);
				CloseHandle(output);
				DeleteFile(temp_file_name);
				return TCFieldStatusFileError;
			}
		} else {
			UnmapFile(&input);
			CloseHandle(output);
			DeleteFile(temp_file_name);
			return TCFieldStatusFileError;
		}
	}

	UnmapFile(&input);
	CloseHandle(output);
	CopyFile(temp_file_name, filename, FALSE);
	DeleteFile(temp_file_name);
	/* TODO: Should really restore other attributes, like time and such. */

	return TCFieldStatusSetSuccess;
}

TCFieldTypeOrStatus __stdcall
ContentSetValue(char *filename, int field_index, int unit_index,
				TCFieldTypeOrStatus field_type, void *field_value,
				TCContentSetValueFlags flags)
{
	if (field_index == FieldIndexLineEnding)
		return TCFieldStatusFileError;

	Encoding const *new_encoding = EncodingsGet(unit_index);
	if (new_encoding == NULL)
		return TCFieldStatusNoSuchField;

	FileMapping mapping;
	TCFieldTypeOrStatus status = MapFile(filename, &mapping, MAX_MAP_SIZE);
	if (status != TCFieldStatusSetSuccess)
		return status;

	Encoding const *old_encoding = EncodingFind(mapping.bytes, mapping.n_bytes);

	UnmapFile(&mapping);

	HMODULE iconv_dll = LoadIconv();
	if (iconv_dll == NULL)
		return TCFieldStatusFileError;

	status = IconvFile(filename, old_encoding, new_encoding);
	if (status != TCFieldStatusSetSuccess) {
		UnloadIconv(iconv_dll);
		return TCFieldStatusFileError;
	}

	UnloadIconv(iconv_dll);

	return TCFieldStatusSetSuccess;
}

/* Entry point into the plugin. */
BOOL APIENTRY
DllMain(HANDLE module, DWORD reason_for_call, LPVOID reserved)
{
	UNREFERENCED_PARAMETER(module);
	UNREFERENCED_PARAMETER(reserved);

    return TRUE;
}
