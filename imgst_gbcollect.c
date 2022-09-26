/**
 * @file imgst_gbcollect.c
 * @brief imgStore library: do_gbcollect implementation.
 */

#include "imgStore.h"
#include "image_content.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

// See imgStore.h
int do_gbcollect (const char *imgst_path, const char *imgst_tmp_bkp_path)
{
    M_REQUIRE_NON_NULL(imgst_path);
    M_REQUIRE_NON_NULL(imgst_tmp_bkp_path);
    
    // Original imgStore file
    struct imgst_file original_file;
    M_EXIT_IF_ERR(do_open(imgst_path, "rb", &original_file));
    
    // Temporary imgStore file
    struct imgst_header imgst_header = {
        .max_files = original_file.header.max_files,
        .res_resized = {
            original_file.header.res_resized[0],
            original_file.header.res_resized[1],
            original_file.header.res_resized[2],
            original_file.header.res_resized[3]

        }
    };
    struct imgst_file temp_file = {
        .header = imgst_header
    };

    do_create(imgst_tmp_bkp_path, &temp_file);
    
    // Garbage collection of the original file
    int ret = ERR_NONE;
    for (size_t i = 0; ret == ERR_NONE && i < original_file.header.max_files; ++i) {
        if (original_file.metadata[i].is_valid == NON_EMPTY) {
            // Buffer to contain the image in original resolution
            size_t size = original_file.metadata[i].size[RES_ORIG];
            char* buffer = calloc(size, 1);
            uint32_t size1 = (uint32_t) size;

            // Reads the image in original resolution
            if ((ret = do_read(original_file.metadata[i].img_id, RES_ORIG, &buffer, &size1, &original_file)) != ERR_NONE) {
                FREE_POINTER(buffer);
                return ret;
            }
            
            // Re-inserts the image in the imgStore (and possibly deduplicates it)
            ret = do_insert(buffer, (size_t) size1, original_file.metadata[i].img_id, &temp_file);
            FREE_POINTER(buffer);
            if (ret != ERR_NONE) return ret;

            // Inserts the image in other resolutions, if any
            if (original_file.metadata[i].offset[RES_SMALL] != 0) {
                M_EXIT_IF_ERR(lazily_resize(RES_SMALL, &temp_file, temp_file.header.num_files-1));
            }

            if (original_file.metadata[i].offset[RES_THUMB] != 0) {
                M_EXIT_IF_ERR(lazily_resize(RES_THUMB, &temp_file, temp_file.header.num_files-1));
            }
        }
    }

    // Renames the temporary file with the original name
    remove(imgst_path);
    rename(imgst_tmp_bkp_path, imgst_path);

    // Cleanup
    do_close(&original_file);
    do_close(&temp_file);

    return ret;
}