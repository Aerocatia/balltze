// SPDX-License-Identifier: GPL-3.0-only

#include <lua.hpp>
#include <balltze/engine.hpp>
#include "../../../logger.hpp"
#include "../../plugin.hpp"
#include "../../loader.hpp"
#include "../helpers.hpp"

namespace Balltze::Plugins {
    static int lua_engine_get_tag_data_header(lua_State *state) noexcept {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            int args = lua_gettop(state);
            if(args == 0) {
                auto tag_data_header = Engine::get_tag_data_header();
                
                lua_newtable(state);

                lua_pushinteger(state, reinterpret_cast<std::uint32_t>(tag_data_header.tag_array));
                lua_setfield(state, -2, "tagArrayAddress");

                lua_pushinteger(state, tag_data_header.scenario_tag.handle);
                lua_setfield(state, -2, "scenarioTagHandle");

                lua_pushinteger(state, tag_data_header.tag_count);
                lua_setfield(state, -2, "tagCount");

                lua_pushinteger(state, tag_data_header.model_part_count);
                lua_setfield(state, -2, "modelPartCount");

                lua_pushinteger(state, tag_data_header.model_data_file_offset);
                lua_setfield(state, -2, "modelDataFileOffset");

                lua_pushinteger(state, tag_data_header.vertex_size);
                lua_setfield(state, -2, "vertexSize");

                lua_pushinteger(state, tag_data_header.model_data_size);
                lua_setfield(state, -2, "modelDataSize");

                return 1;
            }
            else {
                return luaL_error(state, "Invalid number of arguments in function engine.get_tag_data_header.");
            }
        }
        else {
            logger.warning("Could not get plugin for lua state.");
            return luaL_error(state, "Unknown plugin.");
        }
    }

    static int lua_engine_get_tag(lua_State *state) noexcept {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            int args = lua_gettop(state);
            Engine::Tag *tag_entry = nullptr;
            if(args == 1) {
                auto tag_number_thing = lua_tointeger(state, 1);
                if(tag_number_thing < 0xFFFF) {
                    tag_entry = Engine::get_tag(tag_number_thing);
                }
                else {
                    Engine::TagHandle tag_handle;
                    tag_handle.handle = tag_number_thing;
                    tag_entry = Engine::get_tag(tag_handle);
                }
            }
            else if(args == 2) {
                const char *tag_path = luaL_checkstring(state, 1);
                const char *tag_class_str = luaL_checkstring(state, 2);
                auto tag_class = Engine::tag_class_from_string(tag_class_str);
                if(tag_class != Engine::TagClassInt::TAG_CLASS_NULL) {
                    tag_entry = Engine::get_tag(tag_path, tag_class);
                }
                else {
                    return luaL_error(state, "Invalid tag class.");
                }
            }
            else {
                return luaL_error(state, "Invalid number of arguments in function engine.get_tag_entry.");
            }

            if(tag_entry) {
                lua_push_engine_tag(state, tag_entry);
            }
            else {
                lua_pushnil(state);
            }

            return 1;
            
        }
        else {
            logger.warning("Could not get plugin for lua state.");
            return luaL_error(state, "Unknown plugin.");
        }
    }
    
    void lua_attach_tag_data_metatable(lua_State *state) noexcept;
    
    static int lua_engine_get_tag_data(lua_State *state) noexcept {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            int args = lua_gettop(state);
            if(args == 1 || args == 2) {
                Engine::Tag *tag_entry = nullptr;
                if(args == 2 && lua_isstring(state, 1)) {
                    const char *tag_path = lua_tostring(state, 1);
                    const char *tag_class_str = lua_tostring(state, 2);
                    auto tag_class = Engine::tag_class_from_string(tag_class_str);
                    if(tag_class != Engine::TagClassInt::TAG_CLASS_NULL) {
                        tag_entry = Engine::get_tag(tag_path, tag_class);
                    }
                    else {
                        return luaL_error(state, "Invalid tag class.");
                    }
                }
                else {
                    std::uint32_t tag_handle_or_index;
                    if(lua_istable(state, 1)) {
                        lua_getfield(state, 1, "handle");
                        tag_handle_or_index = lua_tointeger(state, -1);
                        lua_pop(state, 1);
                    }
                    else {
                        tag_handle_or_index = lua_tointeger(state, 1);
                    }

                    if(tag_handle_or_index < 0xFFFF) {
                        tag_entry = Engine::get_tag(tag_handle_or_index);
                    }
                    else {
                        Engine::TagHandle tag_handle;
                        tag_handle.handle = tag_handle_or_index;
                        tag_entry = Engine::get_tag(tag_handle);
                    }
                }

                if(!tag_entry) {
                    lua_pushnil(state);
                    return 1;
                }

                if(args == 2) {
                    const char *tag_class_str = lua_tostring(state, 2);
                    auto tag_class = Engine::tag_class_from_string(tag_class_str);
                    if(tag_class != Engine::TagClassInt::TAG_CLASS_NULL) {
                        if(tag_entry->primary_class != tag_class) {
                            return luaL_error(state, "Tag class does not match.");
                        }
                    }
                    else {
                        return luaL_error(state, "Invalid tag class.");
                    }
                }

                lua_newtable(state);
                lua_pushlightuserdata(state, tag_entry->data);
                lua_setfield(state, -2, "_tag_data");
                lua_pushinteger(state, tag_entry->primary_class);
                lua_setfield(state, -2, "_tag_class");
                lua_pushinteger(state, tag_entry->handle.handle);
                lua_setfield(state, -2, "_tag_handle");

                lua_attach_tag_data_metatable(state);

                return 1;
            }
            else {
                return luaL_error(state, "Invalid number of arguments in function engine.get_tag_data.");
            }
        }
        else {
            logger.warning("Could not get plugin for lua state.");
            return luaL_error(state, "Unknown plugin.");
        }
    }

    static const luaL_Reg engine_tag_functions[] = {
        {"getTagDataHeader", lua_engine_get_tag_data_header},
        {"getTag", lua_engine_get_tag},
        {"getTagData", lua_engine_get_tag_data},
        {nullptr, nullptr}
    };

    void set_engine_tag_functions(lua_State *state) noexcept {
        luaL_newlibtable(state, engine_tag_functions);
        luaL_setfuncs(state, engine_tag_functions, 0);
        lua_setglobal(state, "tag");
    }
}