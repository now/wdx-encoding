TCFieldTypeOrStatus __declspec(dllexport) __stdcall
ContentGetSupportedField(int index, char *name, char *units, int size);

TCFieldTypeOrStatus __declspec(dllexport) __stdcall
ContentGetValue(char *filename, int field_index, int unit_index,
				void *field_value, int field_value_size, int flags);

TCFieldTypeOrStatus __declspec(dllexport) __stdcall
ContentSetValue(char *filename, int field_index, int unit_index,
				TCFieldTypeOrStatus field_type, void *field_value,
				TCContentSetValueFlags flags);

TCFieldFlags __declspec(dllexport) __stdcall
ContentGetSupportedFieldFlags(int field_index);

void __declspec(dllexport) __stdcall
ContentStopGetValue(char *filename);

void __declspec(dllexport) __stdcall
ContentPluginUnloading(void);
