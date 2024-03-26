// SPDX-License-Identifier: GPL-3.0-only

#ifndef BALLTZE__FEATURES__FEATURES_HPP
#define BALLTZE__FEATURES__FEATURES_HPP

#include <balltze/api.hpp>
#include <balltze/features.hpp>

namespace Balltze::Features {
    void set_up_hud_button_prompts();
    void set_up_gamepad_pause_button();
    void set_up_tag_data_importing();
    void set_up_sound_subtitles() noexcept;
    void set_up_loading_screen() noexcept;
    void set_up_map_data_read_warden() noexcept;
    void set_up_map_textures_preloading() noexcept;
    void set_up_extended_limits();
    void set_up_echo_message_command();
    void set_up_menu_widescreen_override() noexcept;
    void set_up_settings_integration();

    inline void set_up_features() {
        try {
            set_up_echo_message_command();

            switch(get_balltze_side()) {
                case BALLTZE_SIDE_CLIENT: {
                    set_up_gamepad_pause_button();
                    set_up_hud_button_prompts();
                    set_up_map_data_read_warden();
                    set_up_extended_limits();
                    set_up_menu_widescreen_override();
                    set_up_settings_integration();
#ifdef BALLTZE_ENABLE_EXPERIMENTAL
                    set_up_sound_subtitles();
                    set_up_tag_data_importing();
                    set_up_loading_screen();
                    set_up_map_textures_preloading();                    
#endif
                    break;
                }
            }
        }
        catch(...) {
            throw;
        }
    }
}

#endif
