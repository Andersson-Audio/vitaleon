// SPDX-License-Identifier: GPL-3.0-or-later
// Andersson Audio, 2026-11-17. Development aid, compiled only with VITALEON_PROFILE:
// books the time of every processor call of the graph, with the time of its children
// taken out. Single-threaded use only (switch the effect worker off while profiling).
#pragma once
#ifdef VITALEON_PROFILE
#include <chrono>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace vital {
  namespace profile {
    struct Entry { const char* type = nullptr; long long total = 0, children = 0; long calls = 0; };
    // References into an unordered_map stay valid when it grows.
    inline std::unordered_map<const void*, Entry>& entries() { static std::unordered_map<const void*, Entry> map; return map; }
    inline std::vector<long long*>& stack() { static std::vector<long long*> open; return open; }
    inline bool& enabled() { static bool on = false; return on; }

    struct Scope {
      Entry* entry = nullptr;
      std::chrono::steady_clock::time_point start;
      Scope(const void* key, const char* type) {
        if (!enabled())
          return;
        entry = &entries()[key];
        entry->type = type;
        ++entry->calls;
        stack().push_back(&entry->children);
        start = std::chrono::steady_clock::now();
      }
      ~Scope() {
        if (entry == nullptr)
          return;
        long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        entry->total += ns;
        stack().pop_back();
        if (!stack().empty())
          *stack().back() += ns;
      }
    };
  } // namespace profile
} // namespace vital
#define VITALEON_PROFILE_SCOPE(processor) vital::profile::Scope vitaleon_profile_scope(processor, typeid(*(processor)).name())
#else
#define VITALEON_PROFILE_SCOPE(processor)
#endif
