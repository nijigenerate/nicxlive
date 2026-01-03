// stb_image - public domain image loader - https://github.com/nothings/stb
// abbreviated version for PNG/JPEG/BMP/TGA loading.
#ifndef STB_IMAGE_H
#define STB_IMAGE_H

#define STBI_VERSION 1

// Define STBI_NO_STDIO in texture.cpp before including to use memory loading only.
#ifndef STBI_NO_STDIO
#include <stdio.h>
#endif
#include <stdlib.h>
#include <stdint.h>

// Declarations (full implementation is compiled in texture.cpp)
extern "C" {
    typedef unsigned char stbi_uc;
    typedef struct {
        int      (*read)  (void *user, char *data, int size);
        void     (*skip)  (void *user, int n);
        int      (*eof)   (void *user);
    } stbi_io_callbacks;

    stbi_uc *stbi_load_from_memory(const stbi_uc *buffer, int len, int *x, int *y, int *comp, int req_comp);
    stbi_uc *stbi_load(const char *filename, int *x, int *y, int *comp, int req_comp);
    void     stbi_image_free(void *retval_from_stbi_load);
}

#endif // STB_IMAGE_H
