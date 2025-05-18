#include "./url_parser.h"

typedef struct _MemoryStruct {
    char *received_str;
    size_t size;
} MemoryStruct;

static size_t write_call_back(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->received_str, mem->size + totalSize + 1);
    if(ptr == NULL) {
        fprintf(stderr, "failed to allocate memory\n");
        return 0;
    }

    mem->received_str = ptr;
    memcpy(&(mem->received_str[mem->size]), contents, totalSize);
    mem->size += totalSize;
    mem->received_str[mem->size] = 0;

    return totalSize;
}

char* extract_url(const char *json_str, uint32_t index) {
    struct json_object *parsed_json = NULL;
    struct json_object *response = NULL, *data_array = NULL;
    struct json_object *item = NULL, *url_obj = NULL;
    const char *url_str = NULL;
    char *result = NULL;

    parsed_json = json_tokener_parse(json_str);
    if (!parsed_json) {
        fprintf(stderr, "failed to parse JSON\n");
        return NULL;
    } 

    if (!json_object_object_get_ex(parsed_json, "response", &response)) {
        json_object_put(parsed_json);
        fprintf(stderr, "no response field\n");
        return NULL;
    }

    if (!json_object_object_get_ex(response, "data", &data_array)) {
        json_object_put(parsed_json);
        fprintf(stderr, "no data array\n");
        return NULL;
    }

    if (index >= json_object_array_length(data_array)) {
        json_object_put(parsed_json);
        fprintf(stderr, "index %d out of range\n", index);
        return NULL;
    }

    item = json_object_array_get_idx(data_array, index);
    if (!json_object_object_get_ex(item, "cctvurl", &url_obj)) {
        json_object_put(parsed_json);
        fprintf(stderr, "no cctvurl field\n");
        return NULL;
    }

    url_str = json_object_get_string(url_obj);
    if (url_str) {
        result = strdup(url_str);
    }

    json_object_put(parsed_json);

    return result;
}


char* get_cctv_url(char* api_key, uint32_t index) {
    const char *baseUrlTemplate =
        "https://openapi.its.go.kr:9443/cctvInfo?"
        "apiKey=%s"
        "&type=ex&cctvType=1"
        "&minX=126.800000&maxX=127.890000"
        "&minY=34.90000&maxY=35.100000"
        "&getType=json";


    
    char full_url[512];
    snprintf(full_url, sizeof(full_url), baseUrlTemplate, api_key);

    CURL *curl;
    CURLcode res;

    MemoryStruct chunk;
    chunk.received_str = calloc(1, 1);
    chunk.size = 0;

    char* url;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_call_back);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "failed curl_easy_perform() %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(chunk.received_str);
            curl_global_cleanup();
            return 0;
        }

        url = extract_url(chunk.received_str, index);
        if(url == NULL) {
            fprintf(stderr, "failed to parse url form received data");
        }

        curl_easy_cleanup(curl);
        free(chunk.received_str);
        return url;
    }

    curl_global_cleanup();
    return NULL;
}