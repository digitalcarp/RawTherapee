/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2026 Daniel Gao <daniel.gao.work@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "dynlib.h"

#ifdef _WIN32
#include "leanwindows.h"
#else
#include <dlfcn.h>
#endif

#include <fmt/format.h>

using namespace rt;

DynamicLib::DynamicLib(const std::string& path)
{
#ifdef _WIN32
    const int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len == 0) {
        throw std::runtime_error(
            fmt::format("MultiByteToWideChar failed: {}", getLastError()));
    }

    std::wstring wstr(len, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wstr.data(), len) == 0) {
        throw std::runtime_error(
            fmt::format("MultiByteToWideChar failed: {}", getLastError()));
    }

    m_handle = reinterpret_cast<void*>(LoadLibraryW(wstr.c_str()));
    if (!m_handle) {
        throw std::runtime_error(fmt::format("LoadLibrary failed: {}", getLastError()));
    }
#else
    m_handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m_handle) {
        throw std::runtime_error(fmt::format("dlopen failed: {}", dlerror()));
    }
#endif
}

void* DynamicLib::getSymbol(const std::string& name) const
{
#ifdef _WIN32
    auto h = reinterpret_cast<HMODULE>(m_handle);
    void* sym = reinterpret_cast<void*>(GetProcAddress(h, name.c_str()));
    if (!sym) {
        throw std::runtime_error(
            fmt::format("GetProcAddress failed for '{}': {}", name, getLastError()));
    }
#else
    dlerror();  // Clear previous errors
    void* sym = dlsym(m_handle, name.c_str());
    const char* err = dlerror();
    if (err) {
        throw std::runtime_error(fmt::format("dlsym failed for '{}': {}", name, err));
    }
#endif
    return sym;
}

void DynamicLib::close() noexcept
{
    if (m_handle) {
#ifdef _WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
        dlclose(m_handle);
#endif
        m_handle = nullptr;
    }
}

#ifdef _WIN32
std::string DynamicLib::getLastError() const
{
    DWORD err = GetLastError();
    if (err == 0) return "";

    char* buf = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 0, nullptr);

    if (size == 0) return fmt::format("Windows error {}", err);

    std::string msg(buf, size);
    LocalFree(buf);

    return msg;
}
#endif
