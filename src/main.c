#include <SDL3/SDL_render.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "nkui.h"
#include "cpu.h"

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static Uint64 last_time = 0;

struct InputStates {
    bool keys[1024];
    bool shift;
    float forward;
    float right;
    float up;
    float mouseVertical;
    float mouseHorizontal;
    bool menu;
};

struct InputStates inputStates = {0};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    SDL_SetAppMetadata("ChillyGBA", "0.1.0", "io.auroraviola.chillyGBA");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("ButterCatEngine", 240, 160, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetWindowResizable(window, true);
	SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    last_time = SDL_GetTicks();

    NkUiInit(window, renderer);

    inputStates.menu = false;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) {
        if (event->key.key < 1024) {
            inputStates.keys[event->key.key] = SDL_EVENT_KEY_DOWN == event->type;
        }
        else if (event->key.key == SDLK_LSHIFT) {
            inputStates.shift = SDL_EVENT_KEY_DOWN == event->type;
        }
        inputStates.forward = 0;
        inputStates.right = 0;
        inputStates.up = 0;
        if (inputStates.keys[SDLK_W]) {
            inputStates.forward += 1;
        }
        if (inputStates.keys[SDLK_S]) {
            inputStates.forward -= 1;
        }
        if (inputStates.keys[SDLK_D]) {
            inputStates.right += 1;
        }
        if (inputStates.keys[SDLK_A]) {
            inputStates.right -= 1;
        }
        if (inputStates.keys[SDLK_SPACE]) {
            inputStates.up += 1;
        }
        if (inputStates.shift) {
            inputStates.up -= 1;
        }
        if (inputStates.keys[SDLK_ESCAPE]) {
            inputStates.menu = !inputStates.menu;
        }
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION && !inputStates.menu) {
        inputStates.mouseVertical += event->motion.yrel * 0.001;
        inputStates.mouseHorizontal += event->motion.xrel * 0.001;
    }
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    nk_sdl_handle_event(ctx, event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    NkUiDraw();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    uint64_t t = SDL_GetTicks();
    float dt = (float)t - (float)last_time;
    dt /= 1000.0f;
    if (dt > 100.0f)
        dt = 100.0f;
    last_time = t;

	nk_sdl_render(ctx, AA);
	nk_sdl_update_TextInput(ctx);

    SDL_RenderPresent(renderer);

    nk_input_begin(ctx);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
}
