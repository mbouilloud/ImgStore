/**
 * @file imgst_delete.c
 * @brief imgStore library: do_delete implementation.
 */

#include "imgStore.h"

#include <stdio.h>
#include <string.h> // for strncmp

// See imgStore.h
int do_delete(const char * img_id, struct imgst_file * imgst_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgst_file);

    if (imgst_file->header.num_files == 0) return ERR_FILE_NOT_FOUND;

    // ====================================== METADATA =====================================================
    // Finds the reference of the image in the metadata to be deleted and invalidates it
    size_t index = -1;
    for (size_t i = 0; index == -1 && i < imgst_file->header.max_files; ++i) {
        if (!strncmp(imgst_file->metadata[i].img_id, img_id, MAX_IMGST_NAME) // the image has the same ID
            && imgst_file->metadata[i].is_valid == NON_EMPTY) { // this image is not already erased
            index = i;
            imgst_file->metadata[i].is_valid = EMPTY; // invalidates the reference
        }
    }
    if (index == -1) return ERR_FILE_NOT_FOUND; // The reference does not exist

    M_EXIT_IF_ERR(update_metadata(imgst_file, index));

    // ========================================= HEADER =====================================================
    --imgst_file->header.num_files;
    ++imgst_file->header.imgst_version;

    // Writes the updated header on the disk
    M_EXIT_IF_ERR(update_header(imgst_file));

    return ERR_NONE;
}