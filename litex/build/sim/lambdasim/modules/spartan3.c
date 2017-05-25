#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "modules.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define FRAMERATE 25

struct session_s {
    char *sw;
    char *segment;
    char *sys_clk;

    int msecs;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* img_board;
    SDL_Texture* img_digits;
    SDL_Texture* img_sw0;
    SDL_Texture* img_sw1;
    SDL_Event event;
    SDL_Rect* rect_sw;
    SDL_Rect* rect_segment;
};

static SDL_Rect rect_sw[8] = {
    {326, 242, 20, 37},
    {305, 242, 20, 37},
    {284, 242, 20, 37},
    {263, 242, 20, 37},
    {242, 242, 20, 37},
    {221, 242, 20, 37},
    {200, 242, 20, 37},
    {180, 242, 20, 37}
};

static SDL_Rect rect_segment[8] = {
    {216, 193, 32, 42},
    {248, 193, 32, 42},
    {280, 193, 32, 42},
    {312, 193, 32, 42}
};

static int lambdasim_module_pads_get( struct pad_s *pads, char *name, void **signal)
{
    int ret;
    void *sig=NULL;
    int i;

    if(!pads || !name || !signal)
    {
        ret=RC_INVARG;
        goto out;
    }
    i = 0;
    while(pads[i].name)
    {
        if(!strcmp(pads[i].name, name))
        {
            sig=(void*)pads[i].signal;
            break;
        }
        i++;
    }

out:
    *signal=sig;
    return ret;
}

static int spartan3_start(void *base)
{
    int ret = RC_OK;

    printf("Loaded Spartan!\n");

    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1"); /* No sig handlers */
    if(!SDL_Init(SDL_INIT_EVERYTHING))
    {
        ret = RC_ERROR;
        goto out;
    }

out:
    return ret;
}

static int spartan3_new(void **sess, char *args)
{
    int ret = RC_OK;
    struct session_s *s = NULL;

    if(!sess)
    {
        ret = RC_INVARG;
        goto out;
    }

    s = (struct session_s*)malloc(sizeof(struct session_s));
    if(!s)
    {
        ret = RC_NOENMEM;
        goto out;
    }
    memset(s, 0, sizeof(struct session_s));

    s->window = SDL_CreateWindow("Spartan3",
                                 SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED,
                                 600, 426, 0);
    if(!s->window)
    {
        ret = RC_ERROR;
        eprintf("Can't create window\n");
        goto out;
    }

    s->renderer = SDL_CreateRenderer(s->window, -1, SDL_RENDERER_ACCELERATED);
    if(!s->renderer)
    {
        ret = RC_ERROR;
        eprintf("Can't create renderer\n");
        goto out;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(s->renderer, 400, 284);

    s->img_board  = IMG_LoadTexture(s->renderer, "spartan3/spartan3_board.png");
    if(!s->img_board)
    {
        ret = RC_ERROR;
        eprintf("Can't load image: spartan3/spartan3_board.png\n");
        goto out;
    }

    s->img_digits = IMG_LoadTexture(s->renderer, "spartan3/spartan3_digits.png");
    if(!s->img_digits)
    {
        ret = RC_ERROR;
        eprintf("Can't load image: spartan3/spartan3_digits.png\n");
        goto out;
    }

    s->img_sw0    = IMG_LoadTexture(s->renderer, "spartan3/spartan3_sw0.png");
    if(!s->img_sw0)
    {
        ret = RC_ERROR;
        eprintf("Can't load image: spartan3/spartan3_sw0.png\n");
        goto out;
    }

    s->img_sw1    = IMG_LoadTexture(s->renderer, "spartan3/spartan3_sw1.png");
    if(!s->img_sw1)
    {
        ret = RC_ERROR;
        eprintf("Can't load image: spartan3/spartan3_sw1.png\n");
        goto out;
    }

    s->rect_sw = rect_sw;
    s->rect_segment = rect_segment;

out:
    *sess = (void*)s;
    return ret;
}

static int spartan3_add_pads(void *sess, struct pad_list_s *plist)
{
    int ret=RC_OK;
    struct session_s *s=(struct session_s*)sess;
    struct pad_s *pads;

    if(!sess || !plist)
    {
        ret = RC_INVARG;
        goto out;
    }
    pads = plist->pads;
    if(!strcmp(plist->name, "sw"))
    {
        lambdasim_module_pads_get(pads, "sw", (void**)&s->sw);
    }
    if(!strcmp(plist->name, "segment"))
    {
        lambdasim_module_pads_get(pads, "segment", (void**)&s->segment);
    }
    if(!strcmp(plist->name, "sys_clk"))
    {
        lambdasim_module_pads_get(pads, "sys_clk", (void**)&s->sys_clk);
    }

out:
    return ret;
}

static int _spartan3_point_in_box(int x, int y, SDL_Rect *box)
{
    return (x > box->x)
        && (x < box->x + box->w)
        && (y > box->y)
        && (y < box->y + box->h);
}

static int _spartan3_event_sw(struct session_s* s)
{
    int i;

    if(s->event.type == SDL_MOUSEBUTTONDOWN)
    {
        if(s->event.button.button == SDL_BUTTON_LEFT)
        {
            for(i=0; i<8; i++)
            {
                if(_spartan3_point_in_box(s->event.button.x,
                                          s->event.button.y, &s->rect_sw[i]))
                    *s->sw ^= (1 << i);
            }
        }
    }

    return RC_OK;
}

static int _spartan3_render_sw(struct session_s *s)
{
    int i;
    SDL_Texture *img;

    for(i=0; i<8; i++)
    {
        if((*s->sw >> i) & 0x1)
            img = s->img_sw1;
        else
            img = s->img_sw0;
        SDL_RenderCopy(s->renderer, img, NULL, &s->rect_sw[i]);
    }

    return RC_OK;
}

unsigned char _spartan_swap_8bits(unsigned char a) {
    int i;
    int r = 0;

    for(i=0; i<8; i++)
        r = (r << 1) | ((a >> i) & 0x1);

    return r;
}

static int _spartan3_render_segment(struct session_s *s)
{
    char signal;
    SDL_Rect rect;

    signal = _spartan_swap_8bits(~(*s->segment));

    rect.x = s->rect_segment[0].w * ((signal >> 1) & 0xf);
    rect.y = s->rect_segment[0].h * ((signal >> 5) & 0xf);
    rect.w = s->rect_segment[0].w;
    rect.h = s->rect_segment[0].h;

    SDL_RenderCopy(s->renderer, s->img_digits, &rect, &s->rect_segment[3]); // Only one segment handled

    return RC_OK;
}

static int spartan3_tick(void *sess)
{
    int msecs_now;
    struct session_s *s = (struct session_s*)sess;

    if(*s->sys_clk == 0)
    {
        return RC_OK;
    }

    msecs_now = SDL_GetTicks();
    if((msecs_now - s->msecs) > (1000/FRAMERATE))
    {
        s->msecs = msecs_now;
        SDL_RenderClear(s->renderer);

        /* Handle events */
        while(SDL_PollEvent(&s->event))
        {
            if(s->event.type == SDL_QUIT)
            {
                printf("QUIT\n");
            }
            _spartan3_event_sw(s);
        }

        /* Show board */
        SDL_RenderCopy(s->renderer, s->img_board, NULL, NULL);
        _spartan3_render_sw(s);
        _spartan3_render_segment(s);
        SDL_RenderPresent(s->renderer);
    }

    return RC_OK;
}

static struct ext_module_s ext_mod = {
    "spartan3",
    spartan3_start,
    spartan3_new,
    spartan3_add_pads,
    NULL,
    spartan3_tick
};

int lambdasim_ext_module_init(int (*register_module)(struct ext_module_s *))
{
    int ret=RC_OK;
    ret = register_module(&ext_mod);
    return ret;
}
