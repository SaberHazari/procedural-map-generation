#include "utils.h"
#include <cstdio>
#include <cmath>

struct map_info
{
    global_variable constexpr int width = 64;
    global_variable constexpr int height = 32;
    global_variable constexpr int cell_count = width * height;
    global_variable constexpr int octaves = 4;
    global_variable constexpr real32 persistence = 0.5f;
    global_variable constexpr real32 lacunarity = 2.0f;
    global_variable constexpr real32 base_frequency = 0.08f;
};

enum biome : uint8
{
    biome_deepwater = 0,
    biome_water,
    biome_beach,
    biome_plains,
    biome_forest,
    biome_mountain,
    biome_snow,
    biome_count
};

struct biome_info
{
    const char *biome_names[biome_count] = {
        "Deep Water",
        "Water",
        "Beach",
        "Plains",
        "Forest",
        "Mountain",
        "Snow"
    };
    char biome_glyphs[biome_count] = {
        '~',
        '-',
        '.',
        ',',
        'T',
        '^',
        '*'
    };
};

struct terrain_map
{
    real32 height_map[map_info::cell_count];
    real32 moisture_map[map_info::cell_count];
    uint8 biome_map[map_info::cell_count];
    int width_cells;
    int height_cells;
};

internal real32 LatticeNoise(int x, int y, const perm_table *table)
{
    int index = table->perm[(table->perm[x & 255] + y) & 255];
    return table->gradients[index];
}

internal real32 ValueNoise(real32 fx, real32 fy, const perm_table *table)
{
    int ix = (int)std::floorf(fx);
    int iy = (int)std::floorf(fy);
    real32 tx = fx - (real32)ix;
    real32 ty = fy - (real32)iy;

    real32 ux = tx * tx * (3.0f - 2.0f * tx);
    real32 uy = ty * ty * (3.0f - 2.0f * ty);

    real32 v00 = LatticeNoise(ix, iy, table);
    real32 v10 = LatticeNoise(ix + 1, iy, table);
    real32 v01 = LatticeNoise(ix, iy + 1, table);
    real32 v11 = LatticeNoise(ix + 1, iy + 1, table);

    real32 lerp_value = v00 + (v10 - v00) * ux + 
                        (v01 - v00) * uy + 
                        (v11 - v10 - v01 + v00) * ux * uy;
    return lerp_value;
}

internal real32 FractionalBrownianMotion(real32 fx, real32 fy, 
    uint32 seed)
{
    real32 value = 0.0f;
    real32 amplitude = 1.0f;
    real32 frequency = map_info::base_frequency;
    real32 max_value = 0.0f;

    for(int oct = 0; oct < map_info::octaves; ++oct)
    {
        perm_table table;
        PermutationTable(&table, seed + (uint32)oct * 997);
        value += ValueNoise(fx * frequency, fy * frequency, 
            &table) * amplitude;
        max_value += amplitude;
        amplitude *= map_info::persistence;
        frequency *= map_info::lacunarity;
    }
    return value / max_value;
}

internal void GenerateHeightMap(terrain_map *map, uint32 seed)
{
    for(int i = 0; i < map_info::cell_count; ++i)
    {
        int x = i % map->width_cells;
        int y = i / map->width_cells;
        real32 h = FractionalBrownianMotion((real32)x, 
            (real32)y, seed);
        real32 norm_x = (real32)x / (real32)map->width_cells - 0.5f;
        real32 norm_y = (real32)y / (real32)map->height_cells - 0.5f;
        real32 dist = 1.0f - (1.0f - norm_x * norm_x * 4.0f) * 
            (1.0f - norm_y * norm_y * 4.0f);
        dist = dist < 0.0f ? 0.0f : (dist > 1.0f ? 1.0f : dist);
        h = (h - dist * 0.8f + 1.0f) * 0.5f;
        h = h < 0.0f ? 0.0f : (h > 1.0f ? 1.0f : h);
        map->height_map[i] = h;
    }
}

internal void GenerateMoistureMap(terrain_map *map, uint32 seed)
{
    uint32 moisture_seed = seed ^ 0xDEADBEEF;

    for(int i = 0; i < map_info::cell_count; ++i)
    {
        int x = i % map->width_cells;
        int y = i / map->width_cells;
        real32 m = FractionalBrownianMotion((real32)x, 
            (real32)y, moisture_seed);
        map->moisture_map[i] = (m + 1.0f) * 0.5f;
    }
}

internal void ClassifyBiomes(terrain_map *map)
{
    real32 *h = map->height_map;
    real32 *m = map->moisture_map;
    uint8 *b = map->biome_map;

    for(int i = 0; i < map_info::cell_count; ++i)
    {
        if(h[i] < 0.3f)
        {
            b[i] = (m[i] < 0.5f) ? biome_deepwater : biome_water;
        }
        else if(h[i] < 0.35f)
        {
            b[i] = biome_beach;
        }
        else if(h[i] < 0.6f)
        {
            b[i] = (m[i] < 0.5f) ? biome_plains : biome_forest;
        }
        else if(h[i] < 0.88f)
        {
            b[i] =biome_mountain;
        }
        else
        {
            b[i] = biome_snow;
        }
    }
}

internal void TallyBiomes(terrain_map *map, int counts[biome_count])
{
    for(int j = 0; j < biome_count; ++j)
        { counts[j] = 0; }
    
    for(int i = 0; i < map_info::cell_count; ++i)
        { counts[map->biome_map[i]]++; }
}

internal void RenderMapToFile(terrain_map *map, const char *filename)
{
    biome_info info;
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }
    for(int y = 0; y < map->height_cells; ++y)
    {
        for(int x = 0; x < map->width_cells; ++x)
        {
            uint8 biome_id = map->biome_map[y * map->width_cells + x];
            char glyph = info.biome_glyphs[biome_id];
            fputc(glyph, fp);
        }
        fputc('\n', fp);
    }

    int biome_counts[biome_count];
    TallyBiomes(map, biome_counts);

    fprintf(fp, "\nBiome Coverage:\n");
    for(int i = 0; i < biome_count; ++i)
    {
        fprintf(fp, " %-12s: %c %4d cells (%5.2f%%)\n", 
            info.biome_names[i], 
            info.biome_glyphs[i], 
            biome_counts[i], 
            (real32)biome_counts[i] / (real32)map_info::cell_count * 100.0f);
    }
    fclose(fp);
}

int main()
{
    memory_snapshot mem_before, mem_after;
    GetMemoryUsage(&mem_before);
    uint32 seed;
    printf("Enter a seed value: ");
    scanf("%u", &seed);

    LARGE_INTEGER frequency, start_time, end_time;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start_time);

    local_persist terrain_map map;
    map.width_cells = map_info::width;
    map.height_cells = map_info::height;

    GenerateHeightMap(&map, seed);
    GenerateMoistureMap(&map, seed);
    ClassifyBiomes(&map);

    printf("\nGenerated Map(Seed: %u):\n", seed);
    RenderMapToFile(&map, "generated_map.txt");

    QueryPerformanceCounter(&end_time);
    real32 total_elapsed = (real32)(end_time.QuadPart - start_time.QuadPart) / 
        (real32)frequency.QuadPart * 1000.0f;
    
    printf("\nTotal Generation Time: %.02f ms\n", total_elapsed);

    GetMemoryUsage(&mem_after);
    real32 memory_delta_kb = (real32)(mem_after.working_set_size - mem_before.working_set_size) / 1024.0f;
    printf("\nProcess Memory Usage: %.02f KB\n", memory_delta_kb);

    return 0;
}