/*
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "ts_lua_run_plugin.h"
#include "ts_lua_util.h"

#include "swoc/swoc_file.h"
#include "records/RecCore.h"
#include "proxy/http/remap/PluginFactory.h"
#include "tscore/ElevateAccess.h"

#include <sstream>
#include <iomanip>
#include <vector>

// External reference to the plugin factory (will be defined in ts_lua.cc)
extern PluginFactory *ts_lua_plugin_factory;
extern uint32_t ts_lua_elevate_access;

// Forward declarations
static int              ts_lua_run_plugin(lua_State *L);
static RemapPluginInst *ts_lua_find_loaded_plugin(ts_lua_instance_conf *conf, const char *plugin_name, const char *plugin_args);
static RemapPluginInst *ts_lua_load_plugin(const char *plugin_name, const char *plugin_args, const char *from_url,
                                           const char *to_url);

// Register the run_plugin API with Lua
void
ts_lua_inject_run_plugin_api(lua_State *L)
{
  lua_pushcfunction(L, ts_lua_run_plugin);
  lua_setfield(L, -2, "run_plugin");
}

// Main Lua C function for ts.run_plugin()
static int
ts_lua_run_plugin(lua_State *L)
{
  const char           *plugin_name = nullptr;
  const char           *plugin_args = "";
  size_t                name_len    = 0;
  size_t                args_len    = 0;
  ts_lua_http_ctx      *http_ctx    = nullptr;
  ts_lua_instance_conf *conf        = nullptr;
  RemapPluginInst      *plugin      = nullptr;

  // Get the HTTP context
  GET_HTTP_CONTEXT(http_ctx, L);
  if (http_ctx == nullptr) {
    TSError("[ts_lua][run_plugin] Failed to get HTTP context");
    lua_pushboolean(L, 0);
    return 1;
  }

  conf = http_ctx->instance_conf;
  if (conf == nullptr) {
    TSError("[ts_lua][run_plugin] Failed to get instance configuration");
    lua_pushboolean(L, 0);
    return 1;
  }

  // Check if we're in remap context
  if (http_ctx->rri == nullptr) {
    TSError("[ts_lua][run_plugin] run_plugin can only be called in remap context");
    lua_pushboolean(L, 0);
    return 1;
  }

  // Get plugin name (required)
  plugin_name = luaL_checklstring(L, 1, &name_len);
  if (plugin_name == nullptr || name_len == 0) {
    TSError("[ts_lua][run_plugin] Plugin name is required");
    lua_pushboolean(L, 0);
    return 1;
  }

  // Get plugin arguments (optional)
  if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
    plugin_args = luaL_checklstring(L, 2, &args_len);
  }

  // Try to find already loaded plugin (including args in cache key)
  plugin = ts_lua_find_loaded_plugin(conf, plugin_name, plugin_args);

  // If not found, load the plugin
  if (plugin == nullptr) {
    char *from_url = nullptr;
    char *to_url   = nullptr;

    // Get remap URLs from the remap request info
    if (http_ctx->rri && http_ctx->rri->requestBufp) {
      if (http_ctx->rri->mapFromUrl != TS_NULL_MLOC) {
        int len = 0;
        from_url = TSUrlStringGet(http_ctx->rri->requestBufp, http_ctx->rri->mapFromUrl, &len);
      }
      if (http_ctx->rri->mapToUrl != TS_NULL_MLOC) {
        int len = 0;
        to_url = TSUrlStringGet(http_ctx->rri->requestBufp, http_ctx->rri->mapToUrl, &len);
      }
    }

    plugin = ts_lua_load_plugin(plugin_name, plugin_args, from_url ? from_url : "", to_url ? to_url : "");

    // Free the URL strings
    if (from_url) {
      TSfree(from_url);
    }
    if (to_url) {
      TSfree(to_url);
    }

    if (plugin == nullptr) {
      TSError("[ts_lua][run_plugin] Failed to load plugin: %s", plugin_name);
      lua_pushboolean(L, 0);
      return 1;
    }

    // Add to loaded plugins list (including args in cache)
    ts_lua_loaded_plugin *loaded_plugin = static_cast<ts_lua_loaded_plugin *>(TSmalloc(sizeof(ts_lua_loaded_plugin)));
    loaded_plugin->plugin_name          = TSstrdup(plugin_name);
    loaded_plugin->plugin_args          = TSstrdup(plugin_args);
    loaded_plugin->plugin_inst          = plugin;
    loaded_plugin->next                 = conf->loaded_plugins;
    conf->loaded_plugins                = loaded_plugin;

    Dbg(dbg_ctl, "[run_plugin] Loaded and cached plugin: %s", plugin_name);
  } else {
    Dbg(dbg_ctl, "[run_plugin] Using cached plugin: %s", plugin_name);
  }

  // Execute the plugin
  TSRemapStatus status = plugin->doRemap(http_ctx->txnp, http_ctx->rri);

  // Return true if plugin executed successfully
  lua_pushboolean(L, (status == TSREMAP_DID_REMAP || status == TSREMAP_NO_REMAP || status == TSREMAP_DID_REMAP_STOP ||
                      status == TSREMAP_NO_REMAP_STOP));
  return 1;
}

// Find an already loaded plugin in the instance configuration (match name and args)
static RemapPluginInst *
ts_lua_find_loaded_plugin(ts_lua_instance_conf *conf, const char *plugin_name, const char *plugin_args)
{
  ts_lua_loaded_plugin *current = conf->loaded_plugins;

  while (current != nullptr) {
    if (strcmp(current->plugin_name, plugin_name) == 0 && strcmp(current->plugin_args, plugin_args) == 0) {
      return current->plugin_inst;
    }
    current = current->next;
  }

  return nullptr;
}

// Load a plugin using the plugin factory
static RemapPluginInst *
ts_lua_load_plugin(const char *plugin_name, const char *plugin_args, const char *from_url, const char *to_url)
{
  if (ts_lua_plugin_factory == nullptr) {
    TSError("[ts_lua][run_plugin] Plugin factory not initialized");
    return nullptr;
  }

  // Parse plugin arguments into tokens
  std::vector<std::string> tokens;
  std::istringstream       iss(plugin_args);
  std::string              token;

  while (iss >> std::quoted(token)) {
    tokens.push_back(token);
  }

  // Create argc and argv
  // argv[0] = from_url, argv[1] = to_url, argv[2+] = plugin arguments
  int    argc = tokens.size() + 2;
  char **argv = new char *[argc];

  argv[0] = const_cast<char *>(from_url);
  argv[1] = const_cast<char *>(to_url);

  for (size_t i = 0; i < tokens.size(); ++i) {
    argv[i + 2] = const_cast<char *>(tokens[i].c_str());
  }

  std::string      error;
  RemapPluginInst *plugin = nullptr;

  // Escalate privileges while loading the plugin (same as header_rewrite)
  {
    ElevateAccess access(ts_lua_elevate_access ? ElevateAccess::FILE_PRIVILEGE : 0);

    plugin = ts_lua_plugin_factory->getRemapPlugin(swoc::file::path(plugin_name), argc, argv, error, false);
  } // done elevating access

  delete[] argv;

  if (!plugin) {
    TSError("[ts_lua][run_plugin] Unable to load plugin '%s': %s", plugin_name, error.c_str());
    return nullptr;
  }

  return plugin;
}
