#pragma once

/**
 * @file dedup.h
 * @brief Methods offered by 'dedup.c'.
 */

#include "imgStore.h"

/**
 * @brief De-duplicates (if necessary) the image at position 'index' in the metadata.
 *
 * @param imgst_file The main in-memory data structure, already opened.
 * @param index The position of the image in the metadata.
 */
int do_name_and_content_dedup(struct imgst_file* imgst_file, const uint32_t index);