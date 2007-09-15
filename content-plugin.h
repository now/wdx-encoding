typedef enum TCFieldTypeOrStatus
{
	TCFieldStatusSetCancel = -6,
	TCFieldStatusNotSupported,
	TCFieldStatusOnDemand,
	TCFieldStatusFieldEmpty,
	TCFieldStatusFileError,
	TCFieldStatusNoSuchField,
	TCFieldStatusDelayed,
	TCFieldStatusSetSuccess = 0,
	TCFieldTypeNoMoreFields = 0,
	TCFieldTypeNumeric32,
	TCFieldTypeNumeric64,
	TCFieldTypeNumericFloating,
	TCFieldTypeDate,
	TCFieldTypeTime,
	TCFieldTypeBoolean,
	TCFieldTypeMultipleChoice,
	TCFieldTypeString,
	TCFieldTypeFullText,
};

typedef enum TCContentFlag
{
	TCContentFlagDelayIfSlow = 1 << 0,
	TCContentFlagPassThrough = 1 << 1,
};

typedef struct _TCContentDefaultParamStruct TCContentDefaultParamStruct;

struct _TCContentDefaultParamStruct
{
    int size;
	DWORD interface_version_low;
	DWORD interface_version_high;
	char default_ini_name[MAX_PATH];
};

typedef struct _TCDateFormat TCDateFormat;

struct _TCDateFormat
{
	WORD year;
	WORD month;
	WORD day;
};

typedef struct _TCTimeFormat TCTimeFormat;

struct _TCTimeFormat
{
	WORD hour;
	WORD minute;
	WORD second;
};

typedef int TCFieldFlags;

enum _TCFieldFlags
{
	TCFieldFlagsNone = 0,
	TCFieldFlagsEdit = 1,
	TCFieldFlagsSubstSize = 2,
	TCFieldFlagsDateTime = 4,
	TCFieldFlagsSubstDate = 6,
	TCFieldFlagsSubstTime = 8,
	TCFieldFlagsSubstAttributes = 10,
	TCFieldFlagsSubstAttributeStr = 12,
	TCFieldFlagsPassThroughSizeFloat = 14,
	TCFieldFlagsSubstMask = 14,
	TCFieldsFlagsFieldEdit = 16
};

typedef enum TCContentSetValueFlags
{
	TCContentSetValueFlagFirstAttribute = 1,
	TCContentSetValueFlagLastAttribute = 2,
	TCContentSetValueFlagOnlyDate = 4
};
