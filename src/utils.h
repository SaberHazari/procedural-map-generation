#pragma once

#include <cstdint>
#include <windows.h>
#include <psapi.h>

#define global_variable static
#define local_persist static
#define internal_func static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef float real32;
typedef double real64;
typedef int32_t bool32;

namespace hex_colors
{
    inline constexpr const char *dark_blue = "#1a1a6b";
    inline constexpr const char *bright_blue = "#4d94ff";
    inline constexpr const char *bright_yellow = "#ffff99";
    inline constexpr const char *dark_yellow = "#ccaa00";
    inline constexpr const char *green = "#00aa00";
    inline constexpr const char *bright_green = "#00ff00";
    inline constexpr const char *bold_green = "#00aa00";
    inline constexpr const char *bold_cyan = "#00ffff";
    inline constexpr const char *cyan = "#00ffff";
    inline constexpr const char *gray_white = "#cccccc";
    inline constexpr const char *dark_gray = "#404040";
    inline constexpr const char *bright_white = "#ffffff";
    inline constexpr const char *river_blue = "#0066ff";
}

struct memory_snapshot
{
    size_t working_set_size;
};

struct perm_table
{
    global_variable constexpr int table_size = 256;
    int perm[table_size * 2];
    real32 gradients[table_size];
};

internal_func uint32 LinearCongruentialGenerator(uint32 state)
{
    const uint32 a = 1664525;
    const uint32 c = 1013904223;
    return a * state + c;
}

internal_func void GetMemoryUsage(memory_snapshot *snapshot)
{
    HANDLE process = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS pmc;
    if(GetProcessMemoryInfo(process, &pmc, sizeof(pmc)))
    {
        snapshot->working_set_size = pmc.WorkingSetSize;
    }
    else
    {
        snapshot->working_set_size = 0;
    }
}

internal_func void PermutationTable(perm_table *table, uint32 seed)
{
    for(int i = 0; i < perm_table::table_size; ++i)
    {
        table->perm[i] = i;
    }

    uint32 state = seed;
    for(int i = perm_table::table_size - 1; i > 0; --i)
    {
        state = LinearCongruentialGenerator(state);
        int j = state % (i + 1);
        int temp = table->perm[i];
        table->perm[i] = table->perm[j];
        table->perm[j] = temp;
    }

    for(int i = 0; i < perm_table::table_size; ++i)
    {
        table->perm[perm_table::table_size + i] = table->perm[i];
    }

    for(int i = 0; i < perm_table::table_size; ++i)
    {
        state = LinearCongruentialGenerator(state);
        table->gradients[i] = ((real32)(state & 0xFFFF) * (1.0f / 32767.5f)) - 1.0f;
    }
}