// SPDX-License-Identifier: GPL-3.0-only

#include <windows.h>
#include <iostream>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <balltze/memory.hpp>
#include <balltze/command.hpp>
#include <balltze/engine/core.hpp>
#include "../logger.hpp"
#include "codefinder.hpp"

namespace Balltze::Memory {
    static std::vector<Signature> signatures;

    void write_code(void *pointer, const std::uint16_t *data, std::size_t length) noexcept {
        // Instantiate our new_protection and old_protection variables.
        DWORD new_protection = PAGE_EXECUTE_READWRITE, old_protection;

        // Apply read/write/execute protection
        VirtualProtect(pointer, length, new_protection, &old_protection);

        // Copy
        for(std::size_t i = 0; i < length; i++) {
            if(data[i] != -1) {
                *(reinterpret_cast<std::uint8_t *>(pointer) + i) = static_cast<std::uint8_t>(data[i]);
            }
        }

        // Restore the older protection unless it's the same
        if(new_protection != old_protection) {
            VirtualProtect(pointer, length, old_protection, &new_protection);
        }
    }

    void fill_with_nops(void *address, std::size_t length) noexcept {
        auto *bytes = reinterpret_cast<std::byte *>(address);
        for(std::size_t i = 0; i < length; i++) {
            overwrite(bytes + i, static_cast<std::byte>(0x90));
        }
    }

    std::int32_t calculate_32bit_offset(const void *origin, const void *destination) noexcept {
        std::int32_t offset = reinterpret_cast<std::uint32_t>(destination) - reinterpret_cast<std::uint32_t>(origin);
        return offset;
    }

    std::byte *follow_32bit_offset(std::uint32_t *offset) noexcept {
        auto offset_end = reinterpret_cast<std::uint32_t>(offset + 1);
        auto *destination = reinterpret_cast<std::byte *>(*offset + offset_end);
        return destination;
    }

    std::string Signature::name() const noexcept {
        return m_name;
    }

    std::byte *Signature::data() const noexcept {
        return m_data;
    }

    void Signature::restore() noexcept {
        overwrite(m_data, m_original_data.data(), m_original_data.size());
    }

    Signature::Signature(std::string name, const short *signature, std::size_t lenght, std::uint16_t offset, std::size_t match_num) {
        auto *address = reinterpret_cast<std::byte *>(FindCode(GetModuleHandle(0), signature, lenght, match_num));
        if(address) {
            m_name = name;
            m_data = address + offset;
            m_original_data.insert(m_original_data.begin(), address + offset, address + lenght);
        }
        else {
            throw std::runtime_error("Could not find signature " + name);
        }
    }

    Signature const *get_signature(std::string name) noexcept {
        for(auto &signature : signatures) {
            if(signature.name() == name) {
                return &signature;
            }
        }
        logger.warning("Could not find signature \"{}\"", name);
        return nullptr;
    }

    Signature find_signature(const char *name, const short *signature, std::size_t lenght, std::uint16_t offset, std::size_t match_num) {
        try {
            return Signature(name, signature, lenght, offset, match_num);
        }
        catch(std::runtime_error &e) {
            throw;
        }
    }

    Signature find_signature(const char *name, std::string signature, std::uint16_t offset, std::size_t match_num) {
        std::vector<short> data;
        std::string byte;
        if(signature.size() % 2 != 0) {
            throw std::runtime_error("Invalid signature " + signature);
        }
        for(std::size_t i = 0; i < signature.size(); i += 2) {
            byte = signature.substr(i, 2);
            if(byte == "??") {
                data.push_back(-1);
            }
            else {
                data.push_back(std::stoi(byte, nullptr, 16));
            }
        }
        return Signature(name, data.data(), data.size(), offset, match_num);
    }

    #define FIND_SIGNATURE(name, offset, match_num, ...) { \
        const std::int16_t data[] = __VA_ARGS__; \
        signatures.emplace_back(name, data, sizeof(data) / sizeof(data[0]), offset, match_num); \
    }

    static void find_core_signatures() {
        FIND_SIGNATURE("engine_type", 0x4, 0, {0x8D, 0x75, 0xD0, 0xB8, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x83});
        FIND_SIGNATURE("window_globals", 0x4, 0, {0x8B, 0x45, 0x08, 0xA3, -1, -1, -1, -1, 0x8B, 0x4D, 0x14});
        FIND_SIGNATURE("console_out", 0x0, 0, { 0x83, 0xEC, 0x10, 0x57, 0x8B, 0xF8, 0xA0, -1, -1, -1, -1, 0x84, 0xC0, 0xC7, 0x44, 0x24, 0x04, 0x00, 0x00, 0x80, 0x3F });
    }

    static void find_engine_signatures() {
        auto engine_type = Engine::ENGINE_TYPE_CUSTOM_EDITION;

        FIND_SIGNATURE("halo_path", 0x1, 0, {0xBF, -1, -1, -1, -1, 0xF3, 0xAB, 0xAA, 0xE8});
        FIND_SIGNATURE("resolution", 0x4, 0, { 0x75, 0x0A, 0x66, 0xA1, -1, -1, -1, -1, 0x66, 0x89, 0x42, 0x04, 0x83, 0xC4, 0x10, 0xC3 });
        FIND_SIGNATURE("tick_counter", 0x1, 0, {0xA1, -1, -1, -1, -1, 0x8B, 0x50, 0x14, 0x8B, 0x48, 0x0C, 0x83, 0xC4, 0x04, 0x42, 0x41, 0x4E, 0x4F});
        FIND_SIGNATURE("server_type", 0x0, 0, {0x0F, 0xBF, 0x2D, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x39, 0x1D, -1, -1, -1, -1, 0x75, 0x05});
        FIND_SIGNATURE("current_gametype", 0x0, 0, {0x83, 0x3D, -1, -1, -1, -1, 0x04, 0x8B, 0x4F, 0x6C, 0x89, 0x4C, 0x24, 0x34, 0x75});
        FIND_SIGNATURE("map_index", 0xA, 0, { 0x3B, 0x05, -1, -1, -1, -1, 0x7D, -1, 0x8B, 0x0D, -1, -1, -1, -1 });
        // FIND_SIGNATURE("map_index_demo", 0x2, 0, { 0x89, 0x35, -1, -1, -1, -1, 0xEB, 0x06, 0x8B, 0x35, -1, -1, -1, -1, 0x8B, 0x44, 0x24, 0x18 });

        /** Events */
        FIND_SIGNATURE("on_tick", 0x0, 0, {-1, -1, -1, -1, -1, 0xA1, -1, -1, -1, -1, 0x8B, 0x50, 0x14, 0x8B, 0x48, 0x0C});
        FIND_SIGNATURE("on_map_load", 0x0, 0, {0xE8, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0xA1, -1, -1, -1, -1, 0x33, 0xD2, 0x8B, 0xC8, 0x89, 0x11});
        FIND_SIGNATURE("on_frame", 0x0, 0, { /*0xE8*/ -1, -1, -1, -1, -1, 0x83, 0xC4, 0x08, 0x89, 0x3D });
        FIND_SIGNATURE("d3d9_call_end_scene", 0x0, 0, {0xFF, 0x92, 0xA8, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x7C, 0x0C});
        FIND_SIGNATURE("d3d9_call_reset", 0x0, 0, {0xFF, 0x52, 0x40, 0x85, 0xC0, 0x0F, 0x8C});

        /** Map loading */
        FIND_SIGNATURE("map_header", 0x2, 0, { 0x81, 0x3D, -1, -1, -1, -1, -1, -1, -1, -1, 0x8B, 0x3D });
        FIND_SIGNATURE("map_load_path", 0x0, 0, { /*0xE8*/ -1, -1, -1, -1, -1, 0xA1, -1, -1, -1, -1, 0x83, 0xC4, -1, 0x85, 0xC0, 0xBF, 0x80, 0x00, 0x00, 0x48 });
        FIND_SIGNATURE("read_map_file_data", 0x0, 0, { /*0x57*/ -1, /*0x56*/ -1, /*0x53*/ -1, /*0x55*/ -1, /*0x50*/ -1, 0xFF, 0x54, 0x24, -1, 0x85, 0xC0, 0x75, 0x29 });
        FIND_SIGNATURE("model_data_buffer_alloc", 0x0, 0, {0xFF, 0x15, -1, -1, -1, -1, 0x8B, 0x4B, 0x20, 0x8B, 0x53, 0x14, 0x57, 0x8B, 0xE8});
        
        FIND_SIGNATURE("hold_for_weapon_hud_button_name_draw", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x53, 0x68, -1, -1, -1, -1, 0x8D, 0x44, 0x24, 0x2C, 0x8D, 0x4C, 0x24, 0x38});
        FIND_SIGNATURE("hud_icon_messages_tag_handle", 0x4, 0, {0x83, 0xEC, 0x10, 0xA1, 0xA4, 0x44, 0x6B, 0x00, 0x8B, 0x88, 0xB0, 0x00, 0x00, 0x00, 0x8A, 0x46, 0x0C, 0x53, 0x55, 0x57});
        FIND_SIGNATURE("draw_hud_bitmap_function", 0x0, 0, {0x83, 0xEC, 0x28, 0x84, 0xC9, 0x56, 0x57, 0x8B, 0xF8, 0x8B, 0xF2, 0xC7, 0x44, 0x24, 0x10, 0x00, 0x00, 0x00, 0x00});
        FIND_SIGNATURE("hold_for_action_message_left_quote_print", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x8D, 0x94, 0x24, 0x88, 0x00, 0x00, 0x00, 0x53, 0x52, 0x8D, 0x44, 0x24, 0x24, 0x8D, 0x4C, 0x24, 0x30};)
        FIND_SIGNATURE("hold_for_action_message_right_quote_print", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x18, 0xE9, 0xBF, 0x01, 0x00, 0x00, 0x8B, 0x15, 0xA8, 0x44, 0x6B, 0x00, 0x8A, 0x4A, 0x01};)

        FIND_SIGNATURE("keyboard_input", 0x0, 0, {0x81, 0xFE, 0xFF, 0x7F, 0x00, 0x00, 0x74, 0x32, 0x66, 0x3B, 0xF3, 0x7C, 0x27, 0x66, 0x83, 0xFE, 0x1D});
        FIND_SIGNATURE("mouse_input", 0x0, 0, {0x81, 0xFD, 0xFF, 0x7F, 0x00, 0x00, 0x74, 0x32, 0x66, 0x3B, 0xEF, 0x7C, 0x27, 0x66, 0x83, 0xFD, 0x1D});
        FIND_SIGNATURE("gamepad_input", 0x0, 0, {0x81, 0xFD, 0xFF, 0x7F, 0x00, 0x00, 0x74, 0x3D, 0x66, 0x85, 0xED, 0x7C, 0x2E, 0x66, 0x83, 0xFD, 0x1D});
        FIND_SIGNATURE("get_button_name_function", 0x0, 0, {0x53, 0x8B, 0xD9, 0x0F, 0xBF, 0x08, 0x49, 0x0F, 0x84, 0x8F, 0x00, 0x00, 0x00, 0x49});
        FIND_SIGNATURE("multiplayer_pause_menu_tag_path", 0x1, 0, {0xB8, -1, -1, -1, -1, 0x6A, 0xFF, 0x50, 0xE9, 0xA7, 0x00, 0x00, 0x00, 0x6A, 0xFF});
        FIND_SIGNATURE("singleplayer_pause_menu_tag_path", 0x1, 0, {0x68, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x1C, 0xC6, 0x44, 0x24, 0x12, 0x01, 0x5F, 0x5E});

        /** Menu widget stuff */
        FIND_SIGNATURE("widget_globals", 0x8, 0, {0x33, 0xC0, 0xB9, 0x0D, 0x00, 0x00, 0x00, 0xBF, -1, -1, -1, -1, 0xF3, 0xAB, 0x39, 0x1D});
        FIND_SIGNATURE("widget_event_globals", 0x8, 0, {0x33, 0xC0, 0xB9, 0x43, 0x00, 0x00, 0x00, 0xBF, -1, -1, -1, -1, 0xF3, 0xAB, 0x8D, 0x44, 0x24, 0x04});
        FIND_SIGNATURE("widget_cursor_globals", 0x4, 0, {0x8B, 0xC6, 0xC6, 0x05, -1, -1, -1, -1, 0x01, 0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x04, 0xC6, 0x05, -1, -1, -1, -1, 0x00});
        FIND_SIGNATURE("widget_create_function", 0x0, 0, {0x83, 0xEC, 0x0C, 0x53, 0x8B, 0x5C, 0x24, 0x20, 0x55, 0x33, 0xC0, 0x33, 0xED, 0x66, 0x83, 0xFB, 0xFF});
        FIND_SIGNATURE("widget_open_function", 0x0, 0, {0x8B, 0x0D, -1, -1, -1, -1, 0x8B, 0x54, 0x24, 0x04, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x10, 0x8B, 0xC5, 0x25, 0xFF, 0xFF, 0x00, 0x00});
        FIND_SIGNATURE("widget_close_function", 0x0, 0, {0x83, 0xEC, 0x10, 0x53, 0x8B, 0xD8, 0x33, 0xC0, 0x66, 0x8B, 0x43, 0x08, 0x33, 0xC9, 0x66, 0x3D, 0xFF, 0xFF});
        FIND_SIGNATURE("widget_find_function", 0x0, 0, {0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x11, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x33, 0xC0, 0x3B, 0xD7, 0x75, 0x04});
        FIND_SIGNATURE("widget_focus_function", 0x0, 0, {0x55, 0x56, 0x8B, 0xF1, 0x8B, 0x48, 0x30, 0x85, 0xC9, 0x74, 0x0E, 0xEB, 0x03, 0x8D, 0x49, 0x00});
        FIND_SIGNATURE("widget_list_item_index_function", 0x0, 0, {0x8B, 0x4E, 0x30, 0x83, 0xC8, 0xFF, 0x85, 0xC9, 0x74, 0x18, 0x8B, 0x49, 0x34, 0x33, 0xD2});
        FIND_SIGNATURE("widget_memory_release_function", 0x0, 0, {0x51, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8A, 0x47, 0x14, 0x84, 0xC0, 0x0F, 0x85, -1, -1, -1, -1, 0x66, 0x8B, 0x47, 0x08};)

        FIND_SIGNATURE("get_tag_handle", 0x0, 0, {0xA0, -1, -1, -1, -1, 0x53, 0x83, 0xCB, 0xFF, 0x84, 0xC0, 0x55, 0x8B, 0x6C, 0x24, 0x0C, 0x74, 0x5B, 0xA1, -1, -1, -1, -1, 0x8B, 0x48, 0x0C});
        FIND_SIGNATURE("play_sound_function", 0x0, 0, {0x83, 0xEC, 0x08, 0x8B, 0x0D, -1, -1, -1, -1, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x14, 0x8B, 0xC5, 0x25, 0xFF, 0xFF, 0x00, 0x00, 0xC1, 0xE0, 0x05});
        FIND_SIGNATURE("get_next_sound_permutation_function", 0x0, 0, {0x53, 0x55, 0x8B, 0x6C, 0x24, 0x0C, 0x8B, 0x95, 0x9C, 0x00, 0x00, 0x00, 0x0F, 0xBF, 0xC0, 0x8D, 0x04, 0xC0});
        FIND_SIGNATURE("get_next_sound_permutation_function_play_sound_call", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x8B, 0x55, 0x08, 0x33, 0xC9, 0x89, 0x8D, 0xA8, 0x00, 0x00, 0x00, 0x89, 0x8D, 0xA4, 0x00, 0x00, 0x00});

        FIND_SIGNATURE("draw_8_bit_text", 0x0, 0, {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00, 0x53, 0x8B, 0xD8, 0xA0, -1, -1, -1, -1, 0x84, 0xC0, 0x56, 0x57, 0x0F, 0x84, 0xDA, 0x01, 0x00, 0x00});
        FIND_SIGNATURE("draw_16_bit_text", 0x0, 0, {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x81, 0xEC, 0xA4, 0x00, 0x00, 0x00, 0x53, 0x8B, 0xD8, 0xA0, -1, -1, -1, -1, 0x84, 0xC0, 0x56, 0x57, 0x8B, 0xF9, 0x0F, 0x84, 0xD8, 0x01, 0x00, 0x00});
        FIND_SIGNATURE("text_hook", 0x0, 0, {0x83, 0xEC, 0x48, 0xA0, -1, -1, -1, -1, 0x53, 0x33, 0xDB, 0x3C, 0x01});
        FIND_SIGNATURE("text_font_data", 13, 0, {0xC7, 0x44, 0x24, 0x0C, 0xEB, 0xEA, 0xEA, 0x3E, 0x8B, 0x4C, 0x24, 0x0C, 0xA3, -1, -1, -1, -1, 0x8B, 0xC2});
        FIND_SIGNATURE("read_map_file_data_call_1", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x0C, 0x8D, 0x74, 0x24, 0x13, 0xE8, -1, -1, -1, -1, 0x8A, 0x44, 0x24, 0x13});
        FIND_SIGNATURE("read_map_file_data_call_2", 0x9, 0, {0xBF, 0xD0, 0x43, 0x44, 0x00, 0xC6, 0x46, 0x1E, 0x01, 0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x0C, 0xE9, -1, -1, -1, -1});
        FIND_SIGNATURE("enqueue_sound_function", 0x0, 0, {0x0F, 0xBF, 0xC1, 0x56, 0x8D, 0x34, 0x40, 0x8B, 0x04, 0xF5, -1, -1, -1, -1, 0x85, 0xC0});
        FIND_SIGNATURE("execute_console_command_function", 0x0, 0, {0x8A, 0x07, 0x81, 0xEC, 0x00, 0x05, 0x00, 0x00, 0x3C, 0x3B, 0x74, 0x0E});
        FIND_SIGNATURE("console_unknown_command_message_print_call", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x0C, 0x5E, 0x8A, 0xC3, 0x5B, 0x81, 0xC4, 0x00, 0x05, 0x00, 0x00});
        FIND_SIGNATURE("camera_coord", 0x2, 0, {0xD9, 0x05, -1, -1, -1, -1, 0x83, 0xEC, 0x18, 0xDD, 0x5C, 0x24, 0x10});
        FIND_SIGNATURE("camera_type", 0x2, 0, {0x81, 0xC1, -1, -1, -1, -1, 0x8B, 0x41, 0x08, 0x3D, -1, -1, -1, -1, 0x75, 0x1D, 0xD9, 0x05});
        FIND_SIGNATURE("chat_out", 0x0, 0, {0x83, 0xEC, 0x10, 0x8A, 0x4C, 0x24, 0x14, 0x55, 0x6A, 0x00, 0x6A, 0x01, 0x6A, 0x00, 0x88, 0x4C, 0x24, 0x18});
        FIND_SIGNATURE("antenna_table_address", 0x2, 0, {0x8B, 0x15, -1, -1, -1, -1, 0x8B, 0xC7, 0x25, 0xFF, 0xFF, 0x00, 0x00, 0xC1, 0xE0, 0x05, 0x55, 0x8B, 0x6C, 0x08, 0x14, 0x89, 0x6C, 0x24, 0x28});
        FIND_SIGNATURE("object_table_address", 0x2, 0, {0x8B, 0x0D, -1, -1, -1, -1, 0x8B, 0x51, 0x34, 0x25, 0xFF, 0xFF, 0x00, 0x00, 0x8D});
        FIND_SIGNATURE("delete_object_function", 0x0, 0, {0x8B, 0xF8, 0x25, 0xFF, 0xFF, 0x00, 0x00, 0x8D, 0x04, 0x40, 0x8B, 0x44, 0x82, 0x08, 0x8B, 0x40, 0x04});
        FIND_SIGNATURE("create_object_function", 0x0, 0, {0x56, 0x83, 0xCE, 0xFF, 0x85, 0xC9, 0x57});
        FIND_SIGNATURE("create_object_query_function", 0x0, 0, {0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x56, 0x8B, 0xF0, 0x33, 0xC0});
        FIND_SIGNATURE("apply_damage_function", 0x0, 0, {0x81, 0xEC, 0x94, 0x00, 0x00, 0x00, 0x8B, 0x84, 0x24, 0x9C, 0x00, 0x00, 0x00, 0x25, 0xFF, 0xFF, 0x00, 0x00});
        FIND_SIGNATURE("decal_table_address", 0x1, 0, {0xA1, -1, -1, -1, -1, 0x8A, 0x48, 0x24, 0x83, 0xEC, 0x10, 0x84, 0xC9, 0x74, 0x48, 0x89, 0x04, 0x24, 0x57, 0x35, 0x72, 0x65, 0x74, 0x69});
        FIND_SIGNATURE("effect_table_address", 0x1, 0, {0xA1, -1, -1, -1, -1, 0x8B, 0x15, -1, -1, -1, -1, 0x53, 0x8B, 0x5C, 0x24, 0x24, 0x81, 0xE3, 0xFF, 0xFF, 0x00, 0x00});
        FIND_SIGNATURE("flag_table_address", 0x2, 0, { 0x8B, 0x3D, -1, -1, -1, -1, 0x83, 0xC4, 0x0C, 0x8D, 0x4E, 0x01, 0x83, 0xCB, 0xFF, 0x66, 0x85, 0xC9, 0x7C, 0x31 });
        FIND_SIGNATURE("controls_struct_address", 0xB, 0, {0x0F, 0xBF, 0xCE, 0x8A, 0x14, 0x01, 0x0F, 0xB6, 0xC2, 0x88, 0x85, -1, -1, -1, -1});
        FIND_SIGNATURE("keyboard_keys_struct_address", 0x1, 0, {0xB8, -1, -1, -1, -1, 0xBA, 0x6D, 0x00, 0x00, 0x00, 0x8D, 0x49, 0x00, 0x80, -1, 0x6D, 0x01, 0x75, 0x05});
        FIND_SIGNATURE("light_table_address", 0x2, 0, {0x8B, 0x0D, -1, -1, -1, -1, 0x8B, 0x51, 0x34, 0x56, 0x8B, 0xF0, 0x81, 0xE6, 0xFF, 0xFF, 0x00, 0x00, 0x6B, 0xF6, 0x7C});
        FIND_SIGNATURE("particle_table_address", 0x2, 0, {0x8B, 0x2D, -1, -1, -1, -1, 0x83, 0xCA, 0xFF, 0x8B, 0xFD, 0xE8, -1, -1, -1, -1, 0x8B, 0xF8, 0x83, 0xFF, 0xFF, 0x0F, 0x84, 0x10, 0x06, 0x00, 0x00});
        FIND_SIGNATURE("game_paused_flag_address", 0x2, 0, {0x8B, 0x15, -1, -1, -1, -1, 0x8A, 0x42, 0x02, 0x84, 0xC0, 0x75, 0x22, 0x8B, 0x0D});
        FIND_SIGNATURE("player_handle_address", 0x2, 0, {0x8B, 0x0D, -1, -1, -1, -1, 0xC1, 0xF8, 0x05, 0x23, 0x54, 0x81, 0x18});
        FIND_SIGNATURE("player_table_address", 0x1, 0, {0xA1, -1, -1, -1, -1, 0x89, 0x44, 0x24, 0x48, 0x35});
        FIND_SIGNATURE("server_info_player_list_offset", 0x4, 0, {0x66, 0x0F, 0xBE, 0x8A, -1, -1, -1, -1, 0x66, 0x39, 0x8A});
        FIND_SIGNATURE("server_info_host", 0x1, 0, {0xBF, -1, -1, -1, -1, 0xF3, 0xAB, 0xA1, -1, -1, -1, -1, 0xBA, -1, -1, -1, -1, 0xC7, 0x40, 0x08, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x66, 0x8B, 0x0D, -1, -1, -1, -1, 0x66, 0x89, 0x0D, -1, -1, -1, -1, 0xB9, 0xFF, 0xFF, 0xFF, 0xFF});
        FIND_SIGNATURE("server_info_client", 0x1, 0, {0xBA, -1, -1, -1, -1, 0xE8, -1, -1, -1, -1, 0x66, 0xA1, -1, -1, -1, -1, 0x66, 0x25, 0xF9, 0xFF});
        FIND_SIGNATURE("camera_data_read", 0x0, 0, {/*0xE8*/ -1, -1, -1, -1, -1, 0x8B, 0x45, 0xEC, 0x8B, 0x4D, 0xF0, 0x40, 0x81, 0xC6});
        FIND_SIGNATURE("server_connect_function_call", 0x0, 0, {0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x14, 0x84, 0xC0, 0x74, 0x12, 0xB8, 0x01, 0x00, 0x00, 0x00});
        FIND_SIGNATURE("apply_damage_function", 0x0, 0, {0x81, 0xEC, 0x94, 0x00, 0x00, 0x00, 0x8B, 0x84, 0x24, 0x9C, 0x00, 0x00, 0x00, 0x25, 0xFF, 0xFF, 0x00, 0x00})
        FIND_SIGNATURE("rcon_message_function_call", 0x0, 0, {/*0xE8*/ -1, -1, -1, -1, -1, 0x83, 0xC4, 0x08, 0x83, 0xC4, 0x58, 0xC3, 0x8B, 0xC2, 0xE8, -1, -1, -1, -1, 0x83, 0xC4, 0x58, 0xC3});
        FIND_SIGNATURE("console_tab_completion_function_call", 0x0, 0, {/*0xE8*/ -1, -1, -1, -1, -1, 0x83, 0xC4, 0x08, 0x8B, 0xE8, 0x66, 0x85, 0xED});
        // FIND_SIGNATURE("command_list_address_demo", 0x1, 0, {0xBD, -1, -1, -1, -1, 0xC7, 0x44, 0x24, 0x10, -1, -1, -1, -1, 0x8B, 0x75, 0x00, 0x8A, 0x5E, 0x18});
        // FIND_SIGNATURE("command_list_address_retail", 0x1, 0, {0xBD, -1, -1, -1, -1, 0xC7, 0x44, 0x24, 0x10, -1, -1, -1, -1, 0x8B, 0x75, 0x00, 0x8A, 0x5E, 0x18});
        FIND_SIGNATURE("command_list_address_custom_edition", 0x1, 0, {0xBB, -1, -1, -1, -1, 0xBD, -1, -1, -1, -1, 0x8B, 0xFF, 0x8B, 0x33, 0x8A, -1, 0x18});

        // Object functions
        FIND_SIGNATURE("unit_enter_vehicle_function", 0x0, 0, {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x81, 0xec, 0xdc, 0x00, 0x00, 0x00, 0x53, 0x56, 0x8b, 0x75, 0x08, 0x57, 0x83, 0xcf, 0xff, 0x3b, 0xf7, 0x0f, 0x84, 0x20, 0x05, 0x00, 0x00});

        FIND_SIGNATURE("play_bik_video_function", 0x0, 0, {0x83, 0xEC, 0x68, 0xA1, -1, -1, -1, -1, 0x53, 0x33, 0xDB, 0x3B, 0xC3, 0x89, 0x5C, 0x24, 0x0C});
        FIND_SIGNATURE("play_bik_video_resolution_set", 0x0, 0, {0xFF, 0x91, 0x90, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x0F, 0x85, -1, -1, -1, -1, 0xA1, -1, -1, -1, -1, 0x8B, 0x08});

        register_command("signature", "debug", "Get address for signature", "<signature name>", +[](int arg_count, const char **args) -> bool {
            if(arg_count == 1) {
                auto sig = get_signature(args[0]);
                if(sig) {
                    char buffer[1024];
                    sprintf_s(buffer, "0x%08X", sig->data());
                    logger.debug("Signature {}: {}", args[0], buffer);
                }
                else {
                    logger.debug("Signature %s not found", args[0]);
                }
            }
            else {
                logger.debug("Usage: signature <signature name>");
            }
            return true;
        }, false, 0, 1);
    }

    void find_signatures() {
        find_core_signatures();
        find_engine_signatures();
    }

    extern "C" std::byte *get_address_for_signature(const char *name) noexcept {
        return get_signature(name)->data();
    }

    #undef FIND_SIGNATURE
}
