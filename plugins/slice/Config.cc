/** @file
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

#include "Config.h"

#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <getopt.h>
#include <string_view>

namespace
{
constexpr std::string_view DefaultSliceSkipHeader = {"X-Slicer-Info"};
constexpr std::string_view DefaultCrrImsHeader    = {"X-Crr-Ims"};
constexpr std::string_view DefaultCrrIdentHeader  = {"X-Crr-Ident"};
} // namespace

Config::~Config()
{
  if (nullptr != m_regex_extra) {
#ifndef PCRE_STUDY_JIT_COMPILE
    pcre_free(m_regex_extra);
#else
    pcre_free_study(m_regex_extra);
#endif
  }
  if (nullptr != m_regex) {
    pcre_free(m_regex);
  }
}

int64_t
Config::bytesFrom(char const *const valstr)
{
  char             *endptr     = nullptr;
  int64_t           blockbytes = strtoll(valstr, &endptr, 10);
  constexpr int64_t kib        = 1024;

  if (nullptr != endptr && valstr < endptr) {
    size_t const dist = endptr - valstr;
    if (dist < strlen(valstr) && 0 <= blockbytes) {
      switch (tolower(*endptr)) {
      case 'g':
        blockbytes *= (kib * kib * kib);
        break;
      case 'm':
        blockbytes *= (kib * kib);
        break;
      case 'k':
        blockbytes *= kib;
        break;
      default:
        break;
      }
    }
  }

  if (blockbytes < 0) {
    blockbytes = 0;
  }

  return blockbytes;
}

bool
Config::fromArgs(int const argc, char const *const argv[])
{
  DEBUG_LOG("Number of arguments: %d", argc);
  for (int index = 0; index < argc; ++index) {
    DEBUG_LOG("args[%d] = %s", index, argv[index]);
  }

  // look for lowest priority deprecated blockbytes
  int64_t blockbytes = 0;

  // backwards compat: look for blockbytes
  for (int index = 0; index < argc; ++index) {
    std::string_view const argstr = argv[index];

    std::size_t const spos = argstr.find_first_of(':');
    if (spos != std::string_view::npos) {
      std::string_view const key = argstr.substr(0, spos);
      std::string_view const val = argstr.substr(spos + 1);

      if (!key.empty() && !val.empty()) {
        char const *const valstr    = val.data(); // inherits argv's null
        int64_t const     bytesread = bytesFrom(valstr);

        if (blockbytesmin <= bytesread && bytesread <= blockbytesmax) {
          DEBUG_LOG("Found deprecated blockbytes %" PRId64, bytesread);
          blockbytes = bytesread;
        }
      }
    }
  }

  // standard parsing
  constexpr struct option longopts[] = {
    {const_cast<char *>("blockbytes"),           required_argument, nullptr, 'b'},
    {const_cast<char *>("crr-ims-header"),       required_argument, nullptr, 'c'},
    {const_cast<char *>("disable-errorlog"),     no_argument,       nullptr, 'd'},
    {const_cast<char *>("exclude-regex"),        required_argument, nullptr, 'e'},
    {const_cast<char *>("crr-ident-header"),     required_argument, nullptr, 'g'},
    {const_cast<char *>("include-regex"),        required_argument, nullptr, 'i'},
    {const_cast<char *>("ref-relative"),         no_argument,       nullptr, 'l'},
    {const_cast<char *>("pace-errorlog"),        required_argument, nullptr, 'p'},
    {const_cast<char *>("remap-host"),           required_argument, nullptr, 'r'},
    {const_cast<char *>("skip-header"),          required_argument, nullptr, 's'},
    {const_cast<char *>("blockbytes-test"),      required_argument, nullptr, 't'},
    {const_cast<char *>("prefetch-count"),       required_argument, nullptr, 'f'},
    {const_cast<char *>("strip-range-for-head"), no_argument,       nullptr, 'h'},
    {const_cast<char *>("minimum-size"),         required_argument, nullptr, 'm'},
    {const_cast<char *>("metadata-cache-size"),  required_argument, nullptr, 'z'},
    {const_cast<char *>("stats-prefix"),         required_argument, nullptr, 'x'},
    {nullptr,                                    0,                 nullptr, 0  },
  };

  // getopt assumes args start at '1' so this hack is needed
  char *const *argvp = (const_cast<char *const *>(argv) - 1);
  for (;;) {
    int const opt = getopt_long(argc + 1, argvp, "b:dc:e:g:i:lm:p:r:s:t:x:z:", longopts, nullptr);
    if (-1 == opt) {
      break;
    }

    DEBUG_LOG("processing '%c' %s", (char)opt, argvp[optind - 1]);

    switch (opt) {
    case 'b': {
      int64_t const bytesread = bytesFrom(optarg);
      if (blockbytesmin <= bytesread && bytesread <= blockbytesmax) {
        DEBUG_LOG("Using blockbytes %" PRId64, bytesread);
        blockbytes = bytesread;
      } else {
        ERROR_LOG("Invalid blockbytes: %s", optarg);
      }
    } break;
    case 'c': {
      m_crr_ims_header.assign(optarg);
      DEBUG_LOG("Using override crr ims header %s", optarg);
    } break;
    case 'd': {
      m_paceerrsecs = -1;
    } break;
    case 'e': {
      if (None != m_regex_type) {
        ERROR_LOG("Regex already specified!");
        break;
      }

      const char *errptr;
      int         erroffset;
      m_regexstr = optarg;
      m_regex    = pcre_compile(m_regexstr.c_str(), 0, &errptr, &erroffset, nullptr);
      if (nullptr == m_regex) {
        ERROR_LOG("Invalid regex: '%s'", m_regexstr.c_str());
      } else {
        m_regex_type  = Exclude;
        m_regex_extra = pcre_study(m_regex, 0, &errptr);
        DEBUG_LOG("Using regex for url exclude: '%s'", m_regexstr.c_str());
      }
    } break;
    case 'g': {
      m_crr_ident_header.assign(optarg);
      DEBUG_LOG("Using override crr ident header %s", optarg);
    } break;
    case 'i': {
      if (None != m_regex_type) {
        ERROR_LOG("Regex already specified!");
        break;
      }

      const char *errptr;
      int         erroffset;
      m_regexstr = optarg;
      m_regex    = pcre_compile(m_regexstr.c_str(), 0, &errptr, &erroffset, nullptr);
      if (nullptr == m_regex) {
        ERROR_LOG("Invalid regex: '%s'", m_regexstr.c_str());
      } else {
        m_regex_type  = Include;
        m_regex_extra = pcre_study(m_regex, 0, &errptr);
        DEBUG_LOG("Using regex for url include: '%s'", m_regexstr.c_str());
      }
    } break;
    case 'l': {
      m_reftype = RefType::Relative;
      DEBUG_LOG("Reference slice relative to request (not slice block 0)");
    } break;
    case 'p': {
      int const secsread = atoi(optarg);
      if (0 < secsread) {
        m_paceerrsecs = std::min(secsread, 60);
      } else {
        ERROR_LOG("Ignoring pace-errlog argument");
      }
    } break;
    case 'r': {
      m_remaphost = optarg;
      DEBUG_LOG("Using loopback remap host override: %s", m_remaphost.c_str());
    } break;
    case 's': {
      m_skip_header.assign(optarg);
      DEBUG_LOG("Using slice skip header %s", optarg);
    } break;
    case 't': {
      if (0 == blockbytes) {
        int64_t const bytesread = bytesFrom(optarg);
        if (0 < bytesread) {
          DEBUG_LOG("Using blockbytes-test %" PRId64, bytesread);
          blockbytes = bytesread;
        } else {
          ERROR_LOG("Invalid blockbytes-test: %s", optarg);
        }
      } else {
        DEBUG_LOG("Skipping blockbytes-test in favor of blockbytes");
      }
    } break;
    case 'f': {
      m_prefetchcount = atoi(optarg);
    } break;
    case 'h': {
      m_head_strip_range = true;
    } break;
    case 'm': {
      int64_t const bytesread = bytesFrom(optarg);
      if (bytesread < 0) {
        DEBUG_LOG("Invalid minimum-size: %s", optarg);
      }
      m_min_size_to_slice = bytesread;
      DEBUG_LOG("Only slicing objects %" PRIu64 " bytes or larger", m_min_size_to_slice);
    } break;
    case 'z': {
      try {
        size_t size = std::stoul(optarg);
        setCacheSize(size);
        DEBUG_LOG("Metadata cache size: %zu entries", size);
      } catch (const std::invalid_argument &e) {
        ERROR_LOG("Invalid metadata cache size argument: %s", optarg);
      } catch (const std::out_of_range &e) {
        ERROR_LOG("Metadata cache size out of range: %s", optarg);
      }
    } break;
    case 'x': {
      stat_prefix = optarg;
      DEBUG_LOG("Stat prefix: %s", stat_prefix.c_str());
    } break;
    default:
      break;
    }
  }

  if (0 < blockbytes) {
    DEBUG_LOG("Using configured blockbytes %" PRId64, blockbytes);
    m_blockbytes = blockbytes;
  } else {
    DEBUG_LOG("Using default blockbytes %" PRId64, m_blockbytes);
  }

  if (m_paceerrsecs < 0) {
    DEBUG_LOG("Block stitching error logs disabled");
  } else if (0 == m_paceerrsecs) {
    DEBUG_LOG("Block stitching error logs enabled");
  } else {
    DEBUG_LOG("Block stitching error logs at most every %d sec(s)", m_paceerrsecs);
  }
  if (m_crr_ims_header.empty()) {
    m_crr_ims_header = DefaultCrrImsHeader;
    DEBUG_LOG("Using default crr ims header %s", m_crr_ims_header.c_str());
  }
  if (m_crr_ident_header.empty()) {
    m_crr_ident_header = DefaultCrrIdentHeader;
    DEBUG_LOG("Using default crr ident header %s", m_crr_ident_header.c_str());
  }
  if (m_skip_header.empty()) {
    m_skip_header = DefaultSliceSkipHeader;
    DEBUG_LOG("Using default slice skip header %s", m_skip_header.c_str());
  }

  if (m_min_size_to_slice > 0) {
    if (m_oscache.has_value()) {
      DEBUG_LOG("Metadata cache size: %zu", m_oscache->cache_capacity());
    } else {
      ERROR_LOG("--metadata-cache-size is required when --minimum-size is specified!  Using a default size of 16384.");
      setCacheSize(16384);
    }
  }

  return true;
}

bool
Config::canLogError()
{
  if (m_paceerrsecs < 0) {
    return false;
  } else if (0 == m_paceerrsecs) {
    return true;
  }

#if !defined(UNITTEST)
  TSHRTime const timenow = TShrtime();
#endif

  std::lock_guard<std::mutex> const guard(m_mutex);

#if !defined(UNITTEST)
  if (timenow < m_nextlogtime) {
    return false;
  }

  m_nextlogtime = timenow + TS_HRTIME_SECONDS(m_paceerrsecs);
#else
  m_nextlogtime = 0; // needed by clang
#endif

  return true;
}

bool
Config::matchesRegex(char const *const url, int const urllen) const
{
  bool matches = true;

  switch (m_regex_type) {
  case Exclude: {
    if (0 <= pcre_exec(m_regex, m_regex_extra, url, urllen, 0, 0, nullptr, 0)) {
      matches = false;
    }
  } break;
  case Include: {
    if (pcre_exec(m_regex, m_regex_extra, url, urllen, 0, 0, nullptr, 0) < 0) {
      matches = false;
    }
  } break;
  default:
    break;
  }

  return matches;
}

void
Config::setCacheSize(size_t entries)
{
  if (entries == 0) {
    m_oscache.reset();
  } else {
    m_oscache.emplace(entries);
  }
}

bool
Config::isKnownLargeObj(std::string_view url)
{
  if (m_min_size_to_slice <= 0) {
    // If conditional slicing is not set, all objects are large enough to slice
    return true;
  }

  assert(m_oscache.has_value()); // object size cache is always present when conditionally slicing
  std::optional<uint64_t> size = m_oscache->get(url);
  if (size.has_value()) {
    DEBUG_LOG("Found url in cache: %.*s -> %" PRIu64, static_cast<int>(url.size()), url.data(), size.value());
    if (size.value() >= m_min_size_to_slice) {
      return true;
    }
  }

  return false;
}

void
Config::sizeCacheAdd(std::string_view url, uint64_t size)
{
  if (m_oscache) {
    DEBUG_LOG("Adding url to cache: %.*s -> %" PRIu64, static_cast<int>(url.size()), url.data(), size);
    m_oscache->set(url, size);
  }
}

void
Config::sizeCacheRemove(std::string_view url)
{
  if (m_oscache) {
    DEBUG_LOG("Removing url from cache: %.*s", static_cast<int>(url.size()), url.data());
    m_oscache->remove(url);
  }
}
