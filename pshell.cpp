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

int
FontLineAdvance(font* Font)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);

    int Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&Font->TTF, &Ascent, &Descent, &LineGap);
    
    return Scale * (Ascent - Descent + LineGap);
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

void
PixelBufferDrawRect(pixel_buffer* Buffer, int RX, int RY, int RW, int RH, uint32_t RGB)
{
    for (int Y = RY; Y < RY+RH; Y++)
    {
        for (int X = RX; X < RX+RW; X++)
        {
            if (0 <= X && X < Buffer->Width && 0 <= Y && Y < Buffer->Height)
            {
                uint32_t* Out = (uint32_t*)&Buffer->Data[Y * Buffer->Stride + X * 4];
                *Out = RGB;
            }
        }
    }
}

struct line
{
    int Length;
    char Data[1024];
};

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

    const char* Prompt = "dmarquant@my-desktop:/home/dmarquant$ ";
    int PromptLen = strlen(Prompt);


    // Load font
    font Font = {};
    LoadFont(&Font, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    Font.Size = 16;

    int NumLines = 1;
    line LineBuffer[1000];
    strcpy(LineBuffer[0].Data, Prompt);



    SDL_StartTextInput();

    bool Running = true;
    while (Running)
    {
        SDL_Event E;
        while (SDL_PollEvent(&E))
        {
            switch (E.type)
            {
                // TODO: Handle window resize

                case SDL_KEYUP:
                {
                    if (E.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    {
                        Running = false;
                    }
                }
                break;

                case SDL_KEYDOWN:
                {
                    if (E.key.keysym.scancode == SDL_SCANCODE_RETURN)
                    {
                        // TODO: Execute command

                        NumLines++;
                        strcpy(LineBuffer[NumLines-1].Data, "Output of: ");
                        strcat(LineBuffer[NumLines-1].Data, LineBuffer[NumLines-2].Data + PromptLen);

                        NumLines++;
                        strcpy(LineBuffer[NumLines-1].Data, Prompt);
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
                    {
                        int Len = strlen(LineBuffer[NumLines-1].Data);                        
                        if (Len > PromptLen)
                        {
                            LineBuffer[NumLines-1].Data[Len-1] = 0;
                        }
                    }
                }
                break;

                case SDL_TEXTINPUT:
                {
                    strcat(LineBuffer[NumLines-1].Data, E.text.text);
                }
                break;
            }
        }

        memset(BackBuffer.Data, 0, 800*600*4);

        // TODO: Line wrapping
        int FontDescent = FontGetDescent(&Font);
        int LineAdvance = FontLineAdvance(&Font);

        int ContentHeight = NumLines * LineAdvance;

        int YPos = BackBuffer.Height + FontDescent;
        for (int i = NumLines-1; i >= 0; i--) {
            PixelBufferDrawText(&BackBuffer, &Font, LineBuffer[i].Data, 1, YPos, 0xffffff);
            YPos -= LineAdvance;
        }

        // Draw scroll bar
        int ScrollBarWidth = 14;

        float VisibleRatio = (float)BackBuffer.Height / ContentHeight;
        if (VisibleRatio > 1.0f)
            VisibleRatio = 1.0f;


        int ScrollBarHeight = VisibleRatio * BackBuffer.Height - 4; // TODO: Introduce Pad variables

        PixelBufferDrawRect(&BackBuffer, BackBuffer.Width - ScrollBarWidth, 0, ScrollBarWidth, BackBuffer.Height, 0x5e5e5e);
        PixelBufferDrawRect(&BackBuffer, BackBuffer.Width - ScrollBarWidth + 2, 2, ScrollBarWidth-4, ScrollBarHeight, 0xaeaeae);


        // Copy back buffer to screen
        SDL_UpdateTexture(BackBufferTexture, NULL, BackBuffer.Data, BackBuffer.Stride);
        SDL_RenderClear(Renderer);
        SDL_RenderCopy(Renderer, BackBufferTexture, NULL, NULL);
        SDL_RenderPresent(Renderer);

        SDL_Delay(100);
    }
}
