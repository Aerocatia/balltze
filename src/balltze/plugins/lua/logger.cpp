// SPDX-License-Identifier: GPL-3.0-only

#include <lua.hpp>
#include "../../logger.hpp"
#include "../plugin.hpp"
#include "../loader.hpp"
#include "helpers.hpp"

extern "C" {
    #include "lfmt.h"
}

namespace Balltze::Plugins {
    #define LUA_LOGGER_FUNCTION(name) \
        static int lua_logger_##name(lua_State *state) { \
            auto *plugin = get_lua_plugin(state); \
            if(plugin) { \
                int args = lua_gettop(state); \
                if(args >= 2) { \
                    /* Get the logger name */ \
                    lua_getfield(state, 1, "_name"); \
                    std::string logger_name = luaL_checkstring(state, -1); \
                    lua_pop(state, 1); \
                    \
                    /* Format the message using lfmt */ \
                    std::string message; \
                    if(args > 2) { \
                        lua_remove(state, 1); \
                        Lformat(state); \
                        message = luaL_checkstring(state, -1); \
                        lua_pop(state, 1); \
                        lua_insert(state, 1); \
                    } \
                    else { \
                        message = luaL_checkstring(state, 2); \
                    } \
                    Logger *llogger = plugin->get_logger(logger_name); \
                    if(llogger) { \
                        llogger->name(message); \
                    } \
                    else { \
                        logger.warning("Could not get logger {} for plugin {}", logger_name, plugin->filename()); \
                        return luaL_error(state, "Unknown logger."); \
                    } \
                } \
                else { \
                    logger.warning("Invalid number of arguments for logger." #name "."); \
                    return luaL_error(state, "Invalid number of arguments for logger print function."); \
                } \
            } \
            else { \
                logger.warning("Could not get plugin for lua state."); \
                return luaL_error(state, "Unknown plugin."); \
            } \
            return 0; \
        }

    LUA_LOGGER_FUNCTION(debug)
    LUA_LOGGER_FUNCTION(info)
    LUA_LOGGER_FUNCTION(warning)
    LUA_LOGGER_FUNCTION(error)
    LUA_LOGGER_FUNCTION(fatal)

    static int lua_logger_set_file(lua_State *state) noexcept {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            int args = lua_gettop(state);
            if(args == 2 || args == 3) {
                /* Get the logger name */
                lua_getfield(state, 1, "_name");
                std::string logger_name = luaL_checkstring(state, -1);
                lua_pop(state, 1);

                /* Get the file */
                std::string file = luaL_checkstring(state, 2);

                /* Get the mode */
                bool append = args == 3 ? lua_toboolean(state, 3) : true;

                Logger *llogger = plugin->get_logger(logger_name);
                if(llogger) {
                    auto plugin_dir = plugin->directory();
                    auto file_path = plugin_dir / file;
                    if(plugin->path_is_valid(file_path)) {
                        llogger->set_file(file_path, append);
                    }
                    else {
                        logger.warning("Could not set logger file to {} for plugin {} because it is not in the plugin directory.", file, plugin->filename());
                        return luaL_error(state, "Invalid file path.");
                    }
                }
                else {
                    logger.warning("Could not get logger {} for plugin {}", logger_name, plugin->filename());
                    return luaL_error(state, "Unknown logger.");
                }
            }
            else {
                logger.warning("Invalid number of arguments for logger.set_file.");
                return luaL_error(state, "Invalid number of arguments for logger.set_file.");
            }
        }
        else {
            logger.warning("Could not get plugin for lua state.");
            return luaL_error(state, "Unknown plugin.");
        }
        return 0;
    }

    static int lua_logger__gc(lua_State *state) noexcept {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            /* Get the logger name */
            lua_getfield(state, 1, "_name");
            std::string logger_name = luaL_checkstring(state, -1);
            lua_pop(state, 1);

            plugin->remove_logger(logger_name);
        }
        else {
            logger.warning("Could not get plugin for lua state.");
        }
        return 0;
    }

    static int lua_create_logger(lua_State *state) {
        auto *plugin = get_lua_plugin(state);
        if(plugin) {
            auto *logger_name = luaL_checkstring(state, 1);

            if(std::strlen(logger_name) == 0) {
                return luaL_error(state, "Invalid logger name: name is empty.");
            }

            try {
                plugin->add_logger(logger_name);

                lua_newtable(state);

                lua_pushstring(state, logger_name);
                lua_setfield(state, -2, "_name");

                lua_pushcfunction(state, lua_logger_debug);
                lua_setfield(state, -2, "debug");

                lua_pushcfunction(state, lua_logger_info);
                lua_setfield(state, -2, "info");

                lua_pushcfunction(state, lua_logger_warning);
                lua_setfield(state, -2, "warning");

                lua_pushcfunction(state, lua_logger_error);
                lua_setfield(state, -2, "error");

                lua_pushcfunction(state, lua_logger_fatal);
                lua_setfield(state, -2, "fatal");

                lua_pushcfunction(state, lua_logger_set_file);
                lua_setfield(state, -2, "set_file");

                // Set the garbage collector metatable
                lua_createtable(state, 0, 1);
                lua_pushcfunction(state, lua_logger__gc);
                lua_setfield(state, -2, "__gc");
                lua_setmetatable(state, -2);

                return 1;
            }
            catch(const std::runtime_error &e) {
                return luaL_error(state, "Could not create logger in function create_logger: logger already exists.");
            }
        }
        else {
            logger.warning("Could not get plugin for lua state.");
            return luaL_error(state, "Unknown plugin.");
        }
    }

    static const luaL_Reg logger_functions[] = {
        {"create_logger", lua_create_logger},
        {nullptr, nullptr}
    };

    void lua_set_logger_table(lua_State *state) noexcept {
        lua_create_functions_table(state, "logger", logger_functions);
    }
}
