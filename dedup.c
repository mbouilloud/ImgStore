/**
 * @file dedup.c
 * @brief imgStore library: dedup implementation.
 */

#include "imgStore.h"
#include "dedup.h"

// See dedup.h
int do_name_and_content_dedup(struct imgst_file* imgst_file, const uint32_t index)
{
    M_REQUIRE_NON_NULL(imgst_file);
    if (index < 0 || index >= imgst_file->header.max_files) return ERR_INVALID_ARGUMENT;

    size_t index_duplicate = -1;
    for (size_t i = 0; i < imgst_file->header.max_files; ++i) {
        if (i != index && imgst_file->metadata[i].is_valid == NON_EMPTY) { // for all other valid images

            // There cannot be two images with the same name
            if (!strncmp(imgst_file->metadata[i].img_id, imgst_file->metadata[index].img_id, MAX_IMGST_NAME)) {
                return ERR_DUPLICATE_ID;
            }

            // De-duplicates the image, i.e. references in its metadata the offsets and sizes of the image with the same SHA value
            if (index_duplicate == -1 && !compare_sha(imgst_file->metadata[i].SHA, imgst_file->metadata[index].SHA)) {
                for (size_t j = 0; j < NB_RES; ++j) {
                    imgst_file->metadata[index].offset[j] = imgst_file->metadata[i].offset[j];
                    imgst_file->metadata[index].size[j] = imgst_file->metadata[i].size[j];
                }
                index_duplicate = i;
            }
        }
    }
    if (index_duplicate == -1) { // There is no duplicate
        imgst_file->metadata[index].offset[RES_ORIG] = 0;
    }
    return ERR_NONE;
}
