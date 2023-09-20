// SPDX-License-Identifier: GPL-3.0-only

#ifndef BALLTZE_API__ENGINE__RENDERER_HPP
#define BALLTZE_API__ENGINE__RENDERER_HPP

#include <d3d9.h>
#include "tag_definitions/bitmap.hpp"
#include "../api.hpp"

namespace Balltze::Engine {
    /**
     * This struct represents one of those render targets.
     * 
     * The game holds an array of 9 render targets. The first 
     * one is the backbuffer, the rest are used for various 
     * things depending on the game state.
     */
    struct RenderTarget {
        std::uint32_t width;
        std::uint32_t height;
        D3DFORMAT format;
        IDirect3DSurface9 *surface;
        IUnknown *unknown; // texture?
    };

    /**
     * Load a bitmap into the texture cache
     * @param bitmap_data       the bitmap to load
     * @param immediate         whether to load the bitmap immediately
     * @param force_pixels_read whether to force the pixels to be read
     */
    BALLTZE_API IDirect3DTexture9 *load_bitmap(TagDefinitions::BitmapData *bitmap_data, bool immediate, bool force_pixels_read);

    /**
     * Set the texture of a bitmap to a stage of the device
     * @param device        the device to set the texture on
     * @param stage         the stage to set the texture on
     * @param bitmap_data   the bitmap to set the texture to
     */
    BALLTZE_API void set_bitmap_texture(IDirect3DDevice9 *device, std::size_t stage, TagDefinitions::BitmapData *bitmap_data);

    /**
     * Get the render target at the specified index
     * @param index     the index of the render target (0-8)
     * @return          the render target
     * @throws          std::runtime_error if the index is out of bounds
     */
    RenderTarget *get_render_target(std::size_t index = 0);

    /**
     * Render the UI
     * @param param_1   unknown parameter; you can take it from the UI render event
     */
    BALLTZE_API void render_ui(std::uint32_t param_1);

    /**
     * Render the HUD
     */
    BALLTZE_API void render_hud();

    /**
     * Render the post carnage report
     */
    BALLTZE_API void render_post_carnage_report();
}

#endif
