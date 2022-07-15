// this took 6 hours to get to properly display the splash screen. insanity.
// right now, it doesn't display overlays so you don't get the text saying
// what's currently loading but honestly i value my mental health more than
// some little "loading renderer!!" text
//
// for the sake of my sanity, i'm staying far away from the legacy infested
// spiderweb that is XCB for the forseeable future.
#ifdef __linux__
#include "SplashScreenX11.hpp"
#include "Core/Log.hpp"
#include "SDL_filesystem.h"
#include "stb_image.h"
#include <thread>
#include <xcb/render.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_image.h>

namespace worlds
{
    xcb_intern_atom_reply_t *getAtom(xcb_connection_t *connection, const char *atomName)
    {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(atomName), atomName);
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);

        return reply;
    }

    void testCookie(xcb_void_cookie_t cookie, xcb_connection_t *connection, char *errMessage)
    {
        xcb_generic_error_t *error = xcb_request_check(connection, cookie);
        if (error)
        {
            logErr("ERROR: %s : %" PRIu8 "\n", errMessage, error->error_code);
        }
    }

    // taken from http://vincentsanders.blogspot.com/2010/04/xcb-programming-is-hard.html
    xcb_format_t *findFormat(xcb_connection_t *c, uint8_t depth, uint8_t bpp)
    {
        const xcb_setup_t *setup = xcb_get_setup(c);
        xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
        xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);
        for (; fmt != fmtend; ++fmt)
        {
            if ((fmt->depth == depth) && (fmt->bits_per_pixel == bpp))
            {
                return fmt;
            }
        }
        return 0;
    }

    // this is not a place of honour. nothing valued is here. what is here is dangerous and repulsive to us.
    xcb_pixmap_t loadDataFileToPixmap(xcb_connection_t *connection, uint8_t depth, xcb_window_t window,
                                      const char *fileName)
    {
        const char *basePath = SDL_GetBasePath();

        char *buf = (char *)alloca(strlen(fileName) + strlen(basePath) + 2);
        buf[0] = 0;
        strcat(buf, basePath);
        free((void *)basePath);
        strcat(buf, "EngineData/");
        strcat(buf, fileName);
        logMsg("Loading splash screen from %s", buf);

        int width, height, channels;
        unsigned char *imgData = stbi_load(buf, &width, &height, &channels, 4);

        for (int i = 0; i < width * height; i++)
        {
            uint8_t r = imgData[(i * 4) + 0];
            uint8_t b = imgData[(i * 4) + 2];
            imgData[(i * 4) + 0] = b;
            imgData[(i * 4) + 2] = r;
        }

        const xcb_setup_t *setup = xcb_get_setup(connection);
        const xcb_format_t *format = findFormat(connection, 24, 32);

        xcb_image_t *img =
            xcb_image_create(width, height, XCB_IMAGE_FORMAT_Z_PIXMAP, format->scanline_pad, format->depth,
                             format->bits_per_pixel, 0, (xcb_image_order_t)setup->image_byte_order,
                             XCB_IMAGE_ORDER_LSB_FIRST, imgData, width * height * 4, imgData);
        xcb_image_t *nativeImg = xcb_image_native(connection, img, 1);

        if (nativeImg != img)
        {
            xcb_image_destroy(img);
        }

        xcb_pixmap_t pixmap = xcb_generate_id(connection);
        xcb_create_pixmap(connection, depth, pixmap, window, width, height);

        xcb_gcontext_t pixmapGc = xcb_generate_id(connection);
        xcb_create_gc(connection, pixmapGc, pixmap, 0, nullptr);
        xcb_flush(connection);

        xcb_image_put(connection, pixmap, pixmapGc, nativeImg, 0, 0, 0);
        xcb_flush(connection);

        xcb_free_gc(connection, pixmapGc);
        xcb_image_destroy(nativeImg);

        return pixmap;
    }

    SplashScreenImplX11::SplashScreenImplX11(bool small) : small{small}
    {
        // Connect to the X server
        connection = xcb_connect(nullptr, nullptr);

        // Find the first screen
        const xcb_setup_t *setup = xcb_get_setup(connection);
        xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(setup);
        xcb_screen_t *screen = iterator.data;

        window = xcb_generate_id(connection);

        // We want to receive the exposure event
        uint32_t windowMask = XCB_CW_EVENT_MASK;
        uint32_t windowValues[] = {XCB_EVENT_MASK_EXPOSURE};

        xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, small ? 460 : 800,
                          small ? 215 : 600, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, windowMask,
                          windowValues);

        background =
            loadDataFileToPixmap(connection, screen->root_depth, window, small ? "splash_game.png" : "splash.png");

        // Motif hint to remove decoration
        struct MotifWMHints
        {
            uint32_t flags;
            uint32_t functions;
            uint32_t decorations;
            int input_mode;
            uint32_t status;
        };

        MotifWMHints hints = {0};
        hints.flags = 2;

        xcb_intern_atom_reply_t *motifAtomReply = getAtom(connection, "_MOTIF_WM_HINTS");
        if (motifAtomReply)
        {
            xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, motifAtomReply->atom, motifAtomReply->atom,
                                32, 5, &hints);
            free(motifAtomReply);
        }

        // EWMH hint to set the window type to a splash screen
        xcb_ewmh_connection_t ewmhConnection;
        xcb_ewmh_init_atoms_replies(&ewmhConnection, xcb_ewmh_init_atoms(connection, &ewmhConnection), nullptr);

        xcb_ewmh_set_wm_window_type(&ewmhConnection, window, 1, &ewmhConnection._NET_WM_WINDOW_TYPE_SPLASH);
        xcb_flush(connection);

        xcb_map_window(connection, window);

        graphicsContext = xcb_generate_id(connection);

        uint32_t mask = XCB_GC_BACKGROUND;
        uint32_t value[] = {screen->black_pixel};

        xcb_create_gc(connection, graphicsContext, window, mask, value);
        xcb_flush(connection);

        thread = new std::thread{&SplashScreenImplX11::eventLoop, this};
    }

    void SplashScreenImplX11::handleEvent(xcb_generic_event_t *event)
    {
        switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE: {
            xcb_copy_area(connection, background, window, graphicsContext, 0, 0, 0, 0, small ? 460 : 800,
                          small ? 215 : 600);
            xcb_flush(connection);
            break;
        }
        case 0: {
            xcb_generic_error_t *error = (xcb_generic_error_t *)event;
            logErr("XCB Error: %i (%i:%i, seq %i)", error->error_code, error->minor_code, error->major_code,
                   error->sequence);
            if (error->error_code == XCB_VALUE)
            {
                xcb_value_error_t *valError = (xcb_value_error_t *)event;
                logErr("value error: %u", valError->bad_value);
            }
            break;
        }
        default:
            break;
        }
    }

    void SplashScreenImplX11::eventLoop()
    {
        while (running)
        {
            xcb_generic_event_t *event;
            while ((event = xcb_poll_for_event(connection)))
            {
                handleEvent(event);
                free(event);
            }
        }
        logMsg("splash event loop exiting");
    }

    void SplashScreenImplX11::changeOverlay(const char *overlay)
    {
    }

    SplashScreenImplX11::~SplashScreenImplX11()
    {
        running = false;
        thread->join();
        xcb_disconnect(connection);
    }
}

#endif
