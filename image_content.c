/**
 * @file imgst_content.c
 * @brief imgStore library: lazily_resize implementation.
 */
#include "imgStore.h"
#include "image_content.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <vips/vips.h>

double shrink_value(const VipsImage *image,
                    int max_thumbnail_width,
                    int max_thumbnail_height);

int load_orig_from_disk(struct imgst_file * imgst_file,
                        const size_t index,
                        VipsImage** original,
                        void** buffer);

int store_resized_on_disk(const int res,
                          struct imgst_file * imgst_file,
                          const size_t index,
                          VipsImage** resized,
                          void** buffer);

void resize_image(VipsImage* original,
                  VipsImage** resized,
                  const struct imgst_file * imgst_file,
                  const int res);

// ======================================================================
// See image_content.h
int lazily_resize (const int res,
                   struct imgst_file * imgst_file,
                   const size_t index)
{
    M_REQUIRE_NON_NULL(imgst_file);
    if (res < RES_THUMB || res > RES_ORIG || index < 0 || index >= imgst_file->header.max_files) {
        return ERR_INVALID_ARGUMENT;
    }
    if (res == RES_ORIG || imgst_file->metadata[index].offset[res]) return ERR_NONE;

    VipsObject* parent = VIPS_OBJECT(vips_image_new());
    VipsObject** tab = vips_object_local_array(parent, 2); // contains references of original and resized image

    // Loads the original image from memory
    void* buffer_orig = NULL; // allocated by 'load_orig_from_disk'
    VipsImage* original = VIPS_IMAGE(tab[0]);
    M_EXIT_IF_ERR(load_orig_from_disk(imgst_file, index, &original, &buffer_orig));

    // Resizes the original image in a new resolution
    VipsImage* resized = VIPS_IMAGE(tab[1]);
    resize_image(original, &resized, imgst_file, res);

    // Stores the new image in memory
    void* buffer_resized = NULL; // allocated by 'vips_jpegsave_buffer'
    M_EXIT_IF_ERR(store_resized_on_disk(res, imgst_file, index, &resized, &buffer_resized));

    // Frees the array of image
    g_object_unref(parent);

    // Frees the buffers
    FREE_POINTER(buffer_orig);
    FREE_POINTER(buffer_resized);

    return ERR_NONE;
}

// ======================================================================
/**
 * @brief Loads the original image from the disk, in 'original'.
 *
 * @param imgst_file The main in-memory data structure
 * @param index The position of the image to be resized in memory.
 * @param original Pointer to the original image.
 * @param buffer Pointer to the memory buffer to be filled (to be freed in 'lazily_resize').
 */
int load_orig_from_disk (struct imgst_file * imgst_file,
                         const size_t index,
                         VipsImage** original,
                         void** buffer)
{
    // Allocates the buffer on the heap
    size_t buffer_size_orig = imgst_file->metadata[index].size[RES_ORIG]; // Size (in bytes) of the original image
    *buffer = calloc(1, buffer_size_orig);
    M_EXIT_IF_NULL(*buffer, buffer_size_orig);

    // Loads the image at position 'index' and original resolution in the allocated buffer
    int error_load = load_image_from_imgst(index, RES_ORIG, *buffer, buffer_size_orig, imgst_file);
    if (error_load != ERR_NONE) {
        FREE_POINTER(*buffer);
        return error_load;
    }

    // Loads the content of the buffer in the 'original' VipsImage
    if (vips_jpegload_buffer(*buffer, buffer_size_orig, original, NULL)) return ERR_IMGLIB;

    return ERR_NONE;
}

// ======================================================================
/**
 * @brief Stores the resized image ('resized') on the disk.
 *
 * @param res The code of the new image resolution.
 * @param imgst_file The main in-memory data structure.
 * @param index The position of the resized image in memory.
 * @param resized Pointer to the resized image.
 * @param buffer Pointer to the buffer (to be freed in 'lazily_resize').
 */
int store_resized_on_disk (const int res,
                           struct imgst_file * imgst_file,
                           const size_t index,
                           VipsImage** resized,
                           void** buffer)
{
    size_t buffer_size = 0; // computed by 'vips_jpegsave_buffer'

    // Allocates the buffer and saves the 'resized' VipsImage in it
    if (vips_jpegsave_buffer(*resized, buffer, &buffer_size, NULL)) return ERR_IMGLIB;

    // Stores the content of the buffer at the end of the imgStore
    M_EXIT_IF_ERR(write_image_end_of_imgst(index, res, *buffer, buffer_size, imgst_file));

    // Updates the metadata
    imgst_file->metadata[index].size[res] = buffer_size;
    return update_metadata(imgst_file, index);
}

// ======================================================================
/**
 * @brief Stores the resized version of 'original' image in 'resized'.
 *
 * @param original The original image.
 * @param resized Pointer to the resized image.
 * @param imgst_file The main in-memory data structure.
 * @param res The code of the new image resolution to the resized image.
 */
void resize_image(VipsImage* original,
                  VipsImage** resized,
                  const struct imgst_file * imgst_file,
                  const int res)
{
    const double ratio = shrink_value(original, imgst_file->header.res_resized[2*res], imgst_file->header.res_resized[2*res+1]);
    vips_resize(original, resized, ratio, NULL); // Creates the resized image in 'resized'
}

// ======================================================================
/**
 * @brief Computes the shrinking factor (keeping aspect ratio).
 *
 * @param image The image to be resized.
 * @param max_thumbnail_width The maximum width allowed for thumbnail creation.
 * @param max_thumbnail_height The maximum height allowed for thumbnail creation.
 */
double
shrink_value (const VipsImage *image,
              int max_thumbnail_width,
              int max_thumbnail_height)
{
    const double h_shrink = (double) max_thumbnail_width  / (double) image->Xsize ;
    const double v_shrink = (double) max_thumbnail_height / (double) image->Ysize ;
    return h_shrink > v_shrink ? v_shrink : h_shrink ;
}

// ======================================================================
// See image_content.h
int get_resolution(uint32_t* height, uint32_t* width, const char* image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* image = NULL;
    if (vips_jpegload_buffer((void*) image_buffer, image_size, &image, NULL)) return ERR_IMGLIB;
    *width = image->Xsize;
    *height = image->Ysize;

    return ERR_NONE;
}