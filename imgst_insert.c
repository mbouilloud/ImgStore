/**
 * @file imgst_insert.c
 * @brief imgStore library: do_insert implementation.
 */
#include "imgStore.h"
#include "dedup.h"
#include "image_content.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>

// See imgStore.h
int do_insert(const char* buffer, size_t size, const char* img_id, struct imgst_file* imgst_file)
{
    M_REQUIRE_NON_NULL(buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgst_file);

    if (imgst_file->header.num_files >= imgst_file->header.max_files) return ERR_FULL_IMGSTORE;

    for (size_t i = 0; i < imgst_file->header.max_files; ++i) {
        // Finds (if possible) an empty entry in the metadata for the image
        if (!imgst_file->metadata[i].is_valid) {

            // Sets the SHA, img_id and size fields of the image metadata
            SHA256((const unsigned char*) buffer, size, imgst_file->metadata[i].SHA);
            strncpy(imgst_file->metadata[i].img_id, img_id, MAX_IMG_ID);
            imgst_file->metadata[i].size[RES_ORIG] = (uint32_t) size;

            // De-duplicates the image
            M_EXIT_IF_ERR(do_name_and_content_dedup(imgst_file, (uint32_t) i));

            // If there is no duplicate of the image, writes the image on the disk
            if (imgst_file->metadata[i].offset[RES_ORIG] == 0) {
                imgst_file->metadata[i].offset[RES_SMALL] = 0;
                imgst_file->metadata[i].offset[RES_THUMB] = 0;
                imgst_file->metadata[i].size[RES_SMALL] = 0;
                imgst_file->metadata[i].size[RES_THUMB] = 0;
                M_EXIT_IF_ERR(write_image_end_of_imgst(i, RES_ORIG, buffer, size, imgst_file));
            }
            imgst_file->metadata[i].is_valid = NON_EMPTY;

            // Sets the width and the height of the image
            M_EXIT_IF_ERR(get_resolution(&(imgst_file->metadata[i].res_orig[1]),
                                         &(imgst_file->metadata[i].res_orig[0]),
                                         buffer,
                                         size));

            // Updates the database header on the disk
            ++imgst_file->header.imgst_version;
            ++imgst_file->header.num_files;
            M_EXIT_IF_ERR(update_header(imgst_file));

            // Updates the database metadata on the disk
            M_EXIT_IF_ERR(update_metadata(imgst_file, i));

            return ERR_NONE;
        }
    }
    return ERR_FULL_IMGSTORE;
}