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

#pragma once

#include "ts_lua_common.h"

// Forward declaration
class RemapPluginInst;

// Structure to track loaded plugins
struct ts_lua_loaded_plugin {
  char                        *plugin_name;
  RemapPluginInst             *plugin_inst;
  struct ts_lua_loaded_plugin *next;
};

// Function to inject run_plugin API into Lua
void ts_lua_inject_run_plugin_api(lua_State *L);
