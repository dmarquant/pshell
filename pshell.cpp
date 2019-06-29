#include <SDL2/SDL.h>


int main(int argc, char** argv)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* Window = SDL_CreateWindow("PSHELL", 0, 0, 800, 600, 0);

    SDL_Renderer* Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* BackBufferTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 800, 600);

    Uint8* Pixels = (Uint8*)malloc(800 * 600 * 4);

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
                    printf("TI: %s\n", E.text.text);
                }
                break;
            }
        }

        memset(Pixels, 100, 800*600*4);

        // Copy back buffer to screen
        SDL_UpdateTexture(BackBufferTexture, NULL, Pixels, 800 * sizeof(Uint32));
        SDL_RenderClear(Renderer);
        SDL_RenderCopy(Renderer, BackBufferTexture, NULL, NULL);
        SDL_RenderPresent(Renderer);

        SDL_Delay(100);
    }
}
