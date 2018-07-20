#include <string>
#include <iostream>
#include <cmath>
#include <cstdio>  
#include <codecvt>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ImfOutputFile.h>
#include <ImfInputFile.h>
#include <ImfRgbaFile.h>
#include <ImfChannelList.h>
#include <ImfStringAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfArray.h>
#include <ImfNamespace.h>
#include <ImfHeader.h>
#include <ImfCRgbaFile.h>
#include <png.h>

#define EXPORTBUILD
#include "LayaEXR.h"

using namespace std;
using namespace Imf;
using namespace Imath;

struct RGBA
{
    float r;
    float g;
    float b;
    float a;
};

struct RGBE
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char e;
};

struct ExrWindow
{
    int max_x;
    int max_y;
    int min_x;
    int min_y;
};

void  EXR2RGBE(RGBA* pixelsIn, int columns, int rows, std::vector<RGBE> &pixelsOut)
{
    int size = columns * rows;
    pixelsOut.resize(size);
    for (int i = 0; i < size; ++i)
    {
        RGBA &pixel = pixelsIn[i];
        float red = pixel.r;
        float green = pixel.g;
        float blue = pixel.b;

        double maxValue = red;

        if (green > maxValue)
            maxValue = green;

        if (blue > maxValue)
            maxValue = blue;

        if (maxValue < std::numeric_limits<float>::epsilon())
        {
            pixelsOut[i].r = pixelsOut[i].g = pixelsOut[i].b = pixelsOut[i].e = 0;
        }
        else
        {
            int exponent = 0;
            double mantissa = frexp(maxValue, &exponent);
            if (mantissa < 0)
            {
                pixelsOut[i].r = pixelsOut[i].g = pixelsOut[i].b = pixelsOut[i].e = 0;
            }
            else
            {
                // 当某个值为v的时候，其尾数就是e[0]。 这里*256了，所以反向的时候有个/256即-(128+8)里的8
                // e[0]永远不会为1所以结果<256
                double scaleRatio = (double)(mantissa * 256.0f / maxValue);
                pixelsOut[i].r = (unsigned char)(red * scaleRatio);
                pixelsOut[i].g = (unsigned char)(green * scaleRatio);
                pixelsOut[i].b = (unsigned char)(blue * scaleRatio);
                pixelsOut[i].e = (unsigned char)(exponent + 128);
            }
        }
    }
}


void write_png(const char *file_name, const char* data, int width, int height)
{
    std::vector<unsigned char> buffer;
    buffer.reserve(width * height * 4);

    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    fp = fopen(file_name, "wb");
    if (fp == NULL)
        return;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        png_voidp(NULL), NULL, NULL);

    if (png_ptr == NULL)
    {
        fclose(fp);
        return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        fclose(fp);
        png_destroy_write_struct(&png_ptr, NULL);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, 
        info_ptr,
        width,
        height,
        8,
        PNG_COLOR_TYPE_RGB_ALPHA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);


    png_write_info(png_ptr, info_ptr);
    for (int i = 0; i < height; i++)
        png_write_row(png_ptr, (unsigned char*)data + i * width * 4);

    png_write_end(png_ptr, info_ptr);

    png_free(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);

    return;
}


void _DLLExport EXR2PNG(const char* exrPath, const char* pngPath)
{
    ExrWindow data_window, display_window;
    ImfInputFile *file = ImfOpenInputFile(exrPath);
    assert(file != nullptr);
    const ImfHeader *hdr_info = ImfInputHeader(file);
    ImfHeaderDisplayWindow(hdr_info, 
        &display_window.min_x, 
        &display_window.min_y,
        &display_window.max_x,
        &display_window.max_y);

    ImfHeaderDataWindow(hdr_info, 
        &data_window.min_x, 
        &data_window.min_y,
        &data_window.max_x, 
        &data_window.max_y);

    assert(display_window == data_window);

    int columns = data_window.max_x - data_window.min_x + 1;
    int rows = data_window.max_x - data_window.min_x + 1;

    RGBA *pixels = new RGBA[rows * columns];
    ImfRgba *scanline = new ImfRgba[columns * sizeof(ImfRgba)];
    for (long y = 0; y < rows; y++)
    {
        int yy = data_window.min_y + y;
        memset(scanline, 0, columns * sizeof(ImfRgba));
        ImfInputSetFrameBuffer(file, scanline - data_window.min_x - columns*yy, 1, columns);
        ImfInputReadPixels(file, yy, yy);
        for (long x = 0; x < columns; x++)
        {
            int xx = display_window.min_x + ((int)x - data_window.min_x);
            int index = y * columns + x;
            RGBA &pixel = pixels[index];
            pixel.r = ((float)((double)65535.0f * ImfHalfToFloat(scanline[xx].r))) / 65535.0f;
            pixel.g = ((float)((double)65535.0f * ImfHalfToFloat(scanline[xx].g))) / 65535.0f;
            pixel.b = ((float)((double)65535.0f * ImfHalfToFloat(scanline[xx].b))) / 65535.0f;
            pixel.a = ((float)((double)65535.0f * ImfHalfToFloat(scanline[xx].a))) / 65535.0f;
        }
    }
    delete[] scanline;

    std::vector<RGBE> pixelsOut;
    EXR2RGBE(pixels, columns, rows, pixelsOut);
    write_png(pngPath, (const char*)&pixelsOut[0], columns, rows);
}

int main(int argc, char **argv)
{
    if (argc > 2)
    {
        try
        {
            cout << "exr --> " << argv[1] << endl;
            cout << "png --> "<< argv[2] << endl;
            EXR2PNG(argv[1], argv[2]);
        }
        catch (const std::exception &exc)
        {
            std::cerr << exc.what() << std::endl;
            return 1;
        }
    }
    return 0;
}
