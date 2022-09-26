/**
 * @file imgst_read.c
 * @brief imgStore library: do_read implementation.
 */

#include "imgStore.h"
#include "image_content.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// See imgStore.h
int do_read(const char * img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgst_file * imgst_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgst_file);
    if (resolution < RES_THUMB || resolution > RES_ORIG) return ERR_INVALID_ARGUMENT;
    if (imgst_file->header.num_files == 0) return ERR_FILE_NOT_FOUND;

    for (size_t i = 0; i < imgst_file->header.max_files; ++i) {
        // Finds (if possible) the entry in the metadata corresponding to the given ID
        if (!strncmp(imgst_file->metadata[i].img_id, img_id, MAX_IMG_ID)
            && imgst_file->metadata[i].is_valid == NON_EMPTY) { // for all other valid images

            // If the found image does not exist in the given resolution, creates it
            if (imgst_file->metadata[i].offset[resolution] == 0) {
                M_EXIT_IF_ERR(lazily_resize(resolution, imgst_file, i));
            }

            // Reads the image content in the image buffer
            *image_size = imgst_file->metadata[i].size[resolution];
            *image_buffer = calloc(1, *image_size);
            M_EXIT_IF_NULL(*image_buffer, *image_size);

            int error_load = load_image_from_imgst(i, resolution, *image_buffer, *image_size, imgst_file);
            if (error_load != ERR_NONE) {
                FREE_POINTER(*image_buffer);
            }

            return error_load;
        }
    }
    return ERR_FILE_NOT_FOUND;
}