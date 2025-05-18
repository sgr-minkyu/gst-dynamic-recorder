#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

char* get_cctv_url(char* api_key, uint32_t index);