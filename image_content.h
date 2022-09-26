#pragma once

/**
 * @file image_content.h
 * @brief Methods offered by 'image_content.c'.
 */

#include "imgStore.h"

/**
 * @brief Creates and stores in memory a derivative image of resolution 'res'.
 *
 * @param res The code of the new image resolution.
 * @param imgst_file The main in-memory data structure.
 * @param index The index of the image to be resized in memory.
 */
int lazily_resize(const int res_code, struct imgst_file * imgst_file, const size_t index);

/**
 * @brief Gets the resolution of a JPEG image.
 *
 * @param height Reference to the height of the image.
 * @param width Reference to the width of the image.
 * @param image_buffer Buffer containing the image.
 * @param image_size Size of the image.
 */
int get_resolution(uint32_t* height, uint32_t* width, const char* image_buffer, size_t image_size);