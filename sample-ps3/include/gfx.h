#ifndef GFX_H
#define GFX_H

#include <tiny3d.h>
#include <libfont.h>

#include <jpgdec/jpgdec.h>
#include <pngdec/pngdec.h>

extern jpgData jpg1;
extern u32 jpg1_offset;

extern u32 * texture_pointer; // use to asign texture space without changes texture_mem
extern u32 * texture_pointer2; // use to asign texture for PNG

extern pngData Png_datas[5];
extern u32 Png_offset[5];


void LoadTexture();
int LoadTexturePNG(char * filename, int index);

void DrawBox(float x, float y, float z, float w, float h, u32 rgba);
void DrawTextBox(float x, float y, float z, float w, float h, u32 rgba);

void init_twat();
void update_twat();
void draw_twat(float x, float y, float angle);

int show_dialog(int tdialog, const char * format, ...);
int osk_dialog_get_text(const char* title, char* text, uint32_t size);

#endif