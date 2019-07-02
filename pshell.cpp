#include <cassert>

#include <SDL2/SDL.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

uint32_t
Blend(uint32_t C1, uint32_t C2, uint8_t A)
{
    uint32_t RB = C1 & 0xff00ff;
    uint32_t G  = C1 & 0x00ff00;

    RB += ((C2 & 0xff00ff) - RB) * A >> 8;
    G  += ((C2 & 0x00ff00) - G ) * A >> 8;

    return (RB & 0xff00ff) | (G & 0x00ff00);
}

struct font
{
    stbtt_fontinfo TTF;

    int DataSize;
    unsigned char* Data;

    int Size;
};

void
LoadFont(font* Font, const char* Path)
{
    FILE* File = fopen(Path, "rb");
    assert(File);

    fseek(File, 0, SEEK_END);
    Font->DataSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    Font->Data = (unsigned char*)malloc(Font->DataSize);
    fread(Font->Data, 1, Font->DataSize, File);

    fclose(File);

    stbtt_InitFont(&Font->TTF, Font->Data, stbtt_GetFontOffsetForIndex(Font->Data, 0));
}

int
FontGetDescent(font* Font)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);

    int Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&Font->TTF, &Ascent, &Descent, &LineGap);
    
    return Scale * Descent;
}


struct pixel_buffer
{
    int Width;
    int Height;

    // Pixel data in XRGB8
    unsigned char* Data;

    int Stride;
};


void 
PixelBufferDrawText(pixel_buffer* Buffer, font* Font, const char* Text, int DstX, int DstY, Uint32 RGB)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);
    //float Scale = stbtt_ScaleForMappingEmToPixels(&Font->TTF, Size);

    int PosX = DstX;

    while (*Text)
    {
        int GlyphW, GlyphH;
        int Xoff, Yoff;

        // TODO: Use subpixel APIs
        // TODO: Allocate single buffer to hold the whole string instead
        unsigned char* GlyphBitmap = stbtt_GetCodepointBitmap(&Font->TTF, 0, Scale, *Text,
                                                              &GlyphW, &GlyphH, &Xoff, &Yoff);

        int Advance, Bearing;
        stbtt_GetCodepointHMetrics(&Font->TTF, *Text, &Advance, &Bearing);

        printf("Char(%c): off (%d, %d), Size(%d, %d), Bearing(%f)\n", *Text, Xoff, Yoff, GlyphW, GlyphH, Bearing*Scale);

        for (int Y = 0; Y < GlyphH; Y++)
        {
            for (int X = 0; X < GlyphW; X++)
            {
                int CoordX = PosX + Xoff + X;
                int CoordY = DstY + Yoff + Y;

                if (0 <= CoordX && CoordX < Buffer->Width && 0 <= CoordY && CoordY < Buffer->Height) {
                    uint32_t* Out = (uint32_t*)&Buffer->Data[CoordY * Buffer->Stride + CoordX * 4];
                    uint8_t Coverage = GlyphBitmap[Y*GlyphW + X];

                    *Out = Blend(*Out, RGB, Coverage);
                }
            }
        }

        PosX += Scale*Advance;

        Text++;

        free(GlyphBitmap);
    }
}


int main(int argc, char** argv)
{

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* Window = SDL_CreateWindow("PSHELL", 0, 0, 800, 600, 0);

    SDL_Renderer* Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* BackBufferTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 800, 600);

    pixel_buffer BackBuffer = {};
    BackBuffer.Width  = 800;
    BackBuffer.Height = 600;
    BackBuffer.Data   = (unsigned char*)malloc(800 * 600 * 4);
    BackBuffer.Stride = BackBuffer.Width * 4;


    char CurrentLine[1024];
    strcpy(CurrentLine, "dmarquant@my-desktop:/home/dmarquant$ ");

    // Load font
    font Font = {};
    LoadFont(&Font, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    Font.Size = 16;


    SDL_StartTextInput();

    bool Running = true;
    while (Running)
    {
        SDL_Event E;
        while (SDL_PollEvent(&E))
        {
            switch (E.type)
            {
                case SDL_KEYUP:
                {
                    if (E.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    {
                        Running = false;
                    }
                }
                break;

                case SDL_TEXTINPUT:
                {
                    strcat(CurrentLine, E.text.text);
                }
                break;
            }
        }

        memset(BackBuffer.Data, 0, 800*600*4);

        // TODO: Line wrapping
        int FontDescent = FontGetDescent(&Font);
        PixelBufferDrawText(&BackBuffer, &Font, CurrentLine, 0, BackBuffer.Height + FontDescent, 0xffffff);

        // Copy back buffer to screen
        SDL_UpdateTexture(BackBufferTexture, NULL, BackBuffer.Data, BackBuffer.Stride);
        SDL_RenderClear(Renderer);
        SDL_RenderCopy(Renderer, BackBufferTexture, NULL, NULL);
        SDL_RenderPresent(Renderer);

        SDL_Delay(100);
    }
}
