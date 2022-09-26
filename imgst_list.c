/**
 * @file imgst_list.c
 * @brief imgStore library: do_list implementation.
 */

#include "imgStore.h"

#include <stdio.h>
#include <json-c/json.h>
#include <string.h>

// See imgStore.h
char* do_list (const struct imgst_file * imgst_file, do_list_mode mode)
{
    if (imgst_file == NULL) return NULL;

    if (mode == STDOUT) {
        print_header(&(imgst_file->header));
        if (imgst_file->header.num_files == 0) {
            printf("<< empty imgStore >>\n");
        } else {
            for (int i = 0; i < imgst_file->header.max_files; ++i) {
                if (imgst_file->metadata[i].is_valid == NON_EMPTY) {
                    print_metadata(&(imgst_file->metadata[i]));
                }
            }
        }
        return NULL;
    }

    char* ret; // Return string
    if (mode == JSON) {
        struct json_object* obj = json_object_new_object();
        if (obj == NULL) return NULL;

        struct json_object* array = json_object_new_array();
        if (array == NULL) {
            json_object_put(obj);
            return NULL;
        }

        for (size_t i = 0; i < imgst_file->header.max_files; ++i) {
            if (imgst_file->metadata[i].is_valid == NON_EMPTY) {
                if (json_object_array_add(array, json_object_new_string(imgst_file->metadata[i].img_id)) != 0) {
                    json_object_put(obj);
                    json_object_put(array);
                    return NULL;
                }
            }
        }

        if (json_object_object_add(obj, "Images", array) != 0) {
            json_object_put(obj);
            json_object_put(array);
            return NULL;
        }

        const char* json = json_object_to_json_string(obj);
        ret = malloc(strlen(json)+1);
        if (ret == NULL) {
            json_object_put(obj);
            return NULL;
        }

        strcpy(ret, json);
        json_object_put(obj); // Frees the JSON object

        return ret;
    }

    // The mode is unknown
    const char* error_msg = "unimplemented do_list output mode";
    ret = malloc(strlen(error_msg)+1);
    if (ret == NULL) return ret;
    
    strcpy(ret, error_msg);

    return ret;
}
