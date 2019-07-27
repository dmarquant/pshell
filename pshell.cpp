#include <cassert>

#include <SDL2/SDL.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "linebuffer.cpp"
#include "process.cpp"

int
MinI(int A, int B)
{
    if (A < B)
        return A;
    else
        return B;
}

int
MaxI(int A, int B)
{
    if (A > B)
        return A;
    else
        return B;
}

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
FontGetWidth(font* Font)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);

    int Advance, Bearing;
    stbtt_GetCodepointHMetrics(&Font->TTF, 'M', &Advance, &Bearing);

    return Scale * Advance;
}

int
FontGetAscent(font* Font)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);

    int Ascent, Descent, LineGap;
    stbtt_GetFontVMetrics(&Font->TTF, &Ascent, &Descent, &LineGap);
    
    return Scale * Ascent;
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
PixelBufferDrawText(pixel_buffer* Buffer, font* Font, const char* Text, int Size, int DstX, int DstY, Uint32 RGB)
{
    float Scale = stbtt_ScaleForPixelHeight(&Font->TTF, Font->Size);
    //float Scale = stbtt_ScaleForMappingEmToPixels(&Font->TTF, Size);

    int PosX = DstX;

    for (int I = 0; I < Size; I++) 
    {
        int GlyphW, GlyphH;
        int Xoff, Yoff;

        // TODO: Use subpixel APIs
        // TODO: Allocate single buffer to hold the whole string instead
        unsigned char* GlyphBitmap = stbtt_GetCodepointBitmap(&Font->TTF, 0, Scale, Text[I],
                                                              &GlyphW, &GlyphH, &Xoff, &Yoff);

        int Advance, Bearing;
        stbtt_GetCodepointHMetrics(&Font->TTF, Text[I], &Advance, &Bearing);

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

int
ComputeContentHeight(line_buffer* LineBuffer, int TextAreaWidth, font* Font) {
    int NumRows = 0;

    int CharacterWidth = FontGetWidth(Font);

    int CharactersPerRow = TextAreaWidth / CharacterWidth;

    for (int I = 0; I < LineBuffer->NumLines; I++) {
        line* Line = &LineBuffer->Lines[I];
        NumRows += Line->Size / CharactersPerRow + ((Line->Size % CharactersPerRow) ? 1 : 0);
    }
    return NumRows * FontLineAdvance(Font);
}

int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    int WindowWidth = 800;
    int WindowHeight = 600;

    SDL_Window* Window = SDL_CreateWindow("PSHELL", 0, 0, WindowWidth, WindowHeight, 0);

    SDL_Renderer* Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* BackBufferTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WindowWidth, WindowHeight);

    pixel_buffer BackBuffer = {};
    BackBuffer.Width  = WindowWidth;
    BackBuffer.Height = WindowHeight;
    BackBuffer.Data   = (unsigned char*)malloc(WindowWidth * WindowHeight * 4);
    BackBuffer.Stride = BackBuffer.Width * 4;

    const char* Prompt = "dmarquant@my-desktop:/home/dmarquant$ ";
    int PromptLen = strlen(Prompt);

    line_buffer LineBuffer = {};
    LineBufferInit(&LineBuffer);


    // Load font
    font Font = {};
    LoadFont(&Font, "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    Font.Size = 16;

    LineBufferAddLine(&LineBuffer, Prompt);

    int ScrollBarWidth = 14;
    int ScrollPosition = 0;

    int CharacterWidth = FontGetWidth(&Font);

    int LineAdvance = FontLineAdvance(&Font);
    int ContentHeight = ComputeContentHeight(&LineBuffer, BackBuffer.Width - ScrollBarWidth, &Font);

    process Process = {};

    SDL_StartTextInput();
    bool Running = true;
    while (Running)
    {

        bool AutoScroll = false;

        SDL_Event E;
        while (SDL_PollEvent(&E))
        {
            switch (E.type)
            {
                // TODO: Handle window resize

                case SDL_KEYUP:
                {
                    switch (E.key.keysym.scancode)
                    {
                        case SDL_SCANCODE_ESCAPE:
                        {
                            Running = false;
                        }
                        break;

                    }
                }
                break;

                case SDL_KEYDOWN:
                {
                    if (E.key.keysym.scancode == SDL_SCANCODE_RETURN)
                    {
                        AutoScroll = true;

                        if (Process.PID) 
                        {
                            write(Process.stdinFd, "\n", 1);
                        }
                        else
                        {
                            char Program[1024];
                            line* CurrentLine = &LineBuffer.Lines[LineBuffer.NumLines-1];
                            memcpy(Program, CurrentLine->String + PromptLen, CurrentLine->Size - PromptLen);
                            Program[CurrentLine->Size - PromptLen] = 0;

                            ProcessCreate(&Process, Program);
                        }
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_BACKSPACE)
                    {
                        line* CurrentLine = &LineBuffer.Lines[LineBuffer.NumLines-1];

                        if (CurrentLine->Size > PromptLen)
                        {
                            CurrentLine->Size--;
                            CurrentLine->String[CurrentLine->Size] = 0;
                        }
                        AutoScroll = true;
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_UP)
                    {
                        ScrollPosition = MaxI(ScrollPosition - LineAdvance, 0);
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_DOWN)
                    {
                        ScrollPosition = MinI(ScrollPosition + LineAdvance, ContentHeight - BackBuffer.Height);
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_PAGEUP)
                    {
                        ScrollPosition = MaxI(ScrollPosition - BackBuffer.Height, 0);
                    }
                    else if (E.key.keysym.scancode == SDL_SCANCODE_PAGEDOWN)
                    {
                        ScrollPosition = MinI(ScrollPosition + BackBuffer.Height, ContentHeight - BackBuffer.Height);
                    }
                }
                break;

                case SDL_TEXTINPUT:
                {
                    LineBufferAppendToCurrentLine(&LineBuffer, E.text.text, strlen(E.text.text));
                    AutoScroll = true;

                    if (Process.PID)
                        write(Process.stdinFd, E.text.text, strlen(E.text.text));

                    // TODO: If process is running, buffer input to send to the process
                }
                break;
            }
        }

        // TODO: Don't block, just read whats there

        // Read output from process
        if (Process.PID)
        {
            printf("Reading from proc\n");

            char Buffer[4096];

            int NRead = 0;
            do
            {
                NRead = ProcessReadFromStdout(&Process, Buffer, sizeof(Buffer));
                if (NRead > 0)
                    printf("Read %d bytes\n", NRead);
                
                int Pos = 0;
                while (Pos < NRead)
                {
                    char* LineStart = &Buffer[Pos];

                    while (Buffer[Pos] != '\n' && Pos < NRead)
                        Pos++;

                    Buffer[Pos] = '\0';
                    Pos++;

                    AutoScroll = true;
                    LineBufferAddLine(&LineBuffer, LineStart);

                    // TODO: Add incomplete lines, they also need to be continued on the start of the loop
                }
            } while (NRead == sizeof(Buffer));


            // Check if process is still running
            if (ProcessExited(&Process))
            {
                // TODO: Handle abnormal failures
                
                AutoScroll = true;
                LineBufferAddLine(&LineBuffer, Prompt);

                memset(&Process, 0, sizeof(Process));

                printf("Process exited\n");
            }
        }

        ContentHeight = ComputeContentHeight(&LineBuffer, BackBuffer.Width - ScrollBarWidth, &Font);

        if (AutoScroll)
        {
            // Scroll down to current line
            ScrollPosition = MaxI(0, ContentHeight - BackBuffer.Height);
        }


        memset(BackBuffer.Data, 0, BackBuffer.Width*BackBuffer.Height*4);

        int FontAscent = FontGetAscent(&Font);

        int CharactersPerRow = (BackBuffer.Width-ScrollBarWidth)  / CharacterWidth;

        int YPos = FontAscent;
        for (int i = 0; i < LineBuffer.NumLines; i++)
        {
            line* Line = &LineBuffer.Lines[i];

            for (int Pos = 0; Pos < Line->Size; Pos += CharactersPerRow)
            {
                PixelBufferDrawText(&BackBuffer, &Font, Line->String + Pos, 
                                    MinI(Line->Size - Pos, CharactersPerRow),
                                    1, YPos - ScrollPosition, 0xffffff);

                YPos += LineAdvance;
            }
        }

        // Draw scroll bar

        float VisibleRatio = (float)BackBuffer.Height / ContentHeight;
        if (VisibleRatio > 1.0f)
            VisibleRatio = 1.0f;

        int ScrollBarHeight = VisibleRatio * (BackBuffer.Height - 4); // TODO: Introduce Pad variables

        int ScrollBarPosition = ((float)ScrollPosition / ContentHeight) * (BackBuffer.Height - 4);


        PixelBufferDrawRect(&BackBuffer, BackBuffer.Width - ScrollBarWidth, 0, ScrollBarWidth, BackBuffer.Height, 0x5e5e5e);
        PixelBufferDrawRect(&BackBuffer, BackBuffer.Width - ScrollBarWidth + 2, 2 + ScrollBarPosition, ScrollBarWidth-4, ScrollBarHeight, 0xaeaeae);


        // Copy back buffer to screen
        SDL_UpdateTexture(BackBufferTexture, NULL, BackBuffer.Data, BackBuffer.Stride);
        SDL_RenderClear(Renderer);
        SDL_RenderCopy(Renderer, BackBufferTexture, NULL, NULL);
        SDL_RenderPresent(Renderer);
    }
}
