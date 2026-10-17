#pragma once
#include <cstddef>
typedef struct cJSON { struct cJSON *next; struct cJSON *child; int type; char *valuestring; int valueint; double valuedouble; char *string; } cJSON;
cJSON *cJSON_CreateObject();
cJSON *cJSON_CreateArray();
cJSON *cJSON_AddNumberToObject(cJSON *, const char *, double);
cJSON *cJSON_AddStringToObject(cJSON *, const char *, const char *);
cJSON *cJSON_AddBoolToObject(cJSON *, const char *, bool);
cJSON *cJSON_AddArrayToObject(cJSON *, const char *);
bool cJSON_AddItemToArray(cJSON *, cJSON *);
char *cJSON_PrintUnformatted(const cJSON *);
void cJSON_Delete(cJSON *);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *, const char *);
cJSON *cJSON_GetArrayItem(const cJSON *, int);
int cJSON_GetArraySize(const cJSON *);
cJSON *cJSON_ParseWithLength(const char *, size_t);
bool cJSON_IsNumber(const cJSON *); bool cJSON_IsString(const cJSON *); bool cJSON_IsBool(const cJSON *);
bool cJSON_IsTrue(const cJSON *); bool cJSON_IsArray(const cJSON *); bool cJSON_IsObject(const cJSON *);
#define cJSON_ArrayForEach(element, array) for (element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)
