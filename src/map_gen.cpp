#include "utils.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <queue>

struct map_info
{
    global_variable constexpr int width = 128;
    global_variable constexpr int height = 64;
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
    biome_desert,
    biome_savanna,
    biome_plains,
    biome_forest,
    biome_rainforest,
    biome_taiga,
    biome_tundra,
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
        "Desert",
        "Savanna",
        "Plains",
        "Forest",
        "Rainforest",
        "Taiga",
        "Tundra",
        "Mountain",
        "Snow"
    };
    global_variable constexpr const uint8 biome_colors[biome_count][3] = {
        { 20, 60, 120 },    // Deep Water
        { 40, 100, 180 },   // Water
        { 230, 210, 160 },  // Beach
        { 210, 185, 90 },   // Desert
        { 180, 160, 60 },   // Savanna
        { 120, 180, 80 },   // Plains
        { 50, 120, 50 },    // Forest
        { 20, 80, 30 },     // Rainforest
        { 100, 140, 100 },  // Taiga
        { 180, 190, 185 },  // Tundra
        { 120, 100, 80 },   // Mountain
        { 240, 245, 250 }   // Snow
    };
    char biome_glyphs[biome_count] = {
        '~',
        '-',
        '.',
        '+',
        '`',
        ',',
        'T',
        '@',
        't',
        '!',
        '^',
        '*'
    };
};

struct terrain_map
{
    real32 height_map[map_info::cell_count];
    real32 moisture_map[map_info::cell_count];
    real32 temperature_map[map_info::cell_count];
    uint8 biome_map[map_info::cell_count];
    uint8 river_map[map_info::cell_count];
    int width_cells;
    int height_cells;
};

internal_func real32 LatticeNoise(int x, int y, const perm_table *table)
{
    int index = table->perm[(table->perm[x & 255] + y) & 255];
    return table->gradients[index];
}

internal_func real32 ValueNoise(real32 fx, real32 fy, const perm_table *table)
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

internal_func real32 FractionalBrownianMotion(real32 fx, real32 fy, 
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

internal_func void GenerateHeightMap(terrain_map *map, uint32 seed)
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
        h = (h - dist * 1.5f + 1.0f) * 0.5f;
        h = h < 0.0f ? 0.0f : (h > 1.0f ? 1.0f : h);
        map->height_map[i] = h;
    }
}

internal_func void GenerateMoistureMap(terrain_map *map, uint32 seed)
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

internal_func void GenerateTemperatureMap(terrain_map *map, uint32 seed)
{
    uint32 temp_seed = seed ^ 0xBEEFDEAD;

    for(int i = 0; i < map_info::cell_count; ++i)
    {
        int x = i % map->width_cells;
        int y = i / map->width_cells;
        real32 t = FractionalBrownianMotion((real32)x, 
            (real32)y, temp_seed);
        map->temperature_map[i] = (t + 1.0f) * 0.5f;
    }
}

internal_func void ClassifyBiomes(terrain_map *map)
{
    real32 *h = map->height_map;
    real32 *m = map->moisture_map;
    real32 *t = map->temperature_map;
    uint8 *b = map->biome_map;

    for(int i = 0; i < map_info::cell_count; ++i)
    {
        if(h[i] < 0.15f) { b[i] = biome_deepwater; continue; }
        if(h[i] < 0.30f) { b[i] = biome_water; continue; }
        if(h[i] < 0.35f) { b[i] = biome_beach; continue; }
        if(h[i] > 0.75f) { b[i] = (t[i] < 0.30f) ? biome_snow : biome_mountain; continue; }

        if(t[i] < 0.30f)
        {
            b[i] = (m[i] < 0.5f) ? biome_tundra : biome_taiga;
        }
        else if(t[i] < 0.60f)
        {
            if(m[i] < 0.33f) { b[i] = biome_plains; }
            else if(m[i] < 0.66f) { b[i] = biome_forest; }
            else { b[i] = biome_rainforest; }
        }
        else
        {
            if(m[i] < 0.33f) { b[i] = biome_desert; }
            else if(m[i] < 0.66f) { b[i] = biome_savanna; }
            else { b[i] = biome_rainforest; }
        }
    }
}

internal_func void GenerateRivers(terrain_map* map, uint32 seed)
{
    for(int i = 0; i < map_info::cell_count; ++i)
    { map->river_map[i] = 0; }

    local_persist int candidates[map_info::cell_count];
    int candidate_count = 0;

    for(int i = 0; i < map_info::cell_count; ++i)
    {
        uint8 b = map->biome_map[i];
        bool32  is_land = (b != biome_deepwater && b != biome_water && b != biome_beach);
        bool32 is_high = (map->height_map[i] > 0.68f);
        if(is_land && is_high)
        { candidates[candidate_count++] = i; }
    }
    if(candidate_count == 0) { return; }

    constexpr int river_count = 15;
    uint32 state = seed ^ 0xF00DCAFE;

    constexpr int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    constexpr int dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    for(int r = 0; r < river_count; ++r)
    {
        state = LinearCongruentialGenerator(state);
        int current = candidates[state % candidate_count];

        for(int step = 0; step < map_info::cell_count; ++step)
        {
            uint8 b = map->biome_map[current];
            if(b == biome_deepwater || b == biome_water) { break; }

            map->river_map[current] = 1;

            int cx = current % map->width_cells;
            int cy = current / map->width_cells;

            int best_idx = -1;
            real32 best_height = map->height_map[current];

            for(int d = 0; d < 8; ++d)
            {
                int nx = cx + dx[d];
                int ny = cy + dy[d];
                if(nx < 0 || nx >= map->width_cells || 
                   ny < 0 || ny >= map->height_cells)
                { continue; }

                int ni = ny * map->width_cells + nx;
                if(map->height_map[ni] < best_height)
                {
                    best_height = map->height_map[ni];
                    best_idx = ni;
                }
            }
            if(best_idx == -1) { break; }
            current = best_idx;
        }
    }
}

internal_func void FloodFillRiver(terrain_map* map, int start, 
    std::vector<bool32> &visited)
{
    constexpr int dx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    constexpr int dy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    int width = map->width_cells;
    int height = map->height_cells;

    std::queue<int> q;
    q.push(start);
    visited[start] = true;

    while(!q.empty())
    {
        int current = q.front();
        q.pop();

        int cx = current % width;
        int cy = current / width;

        for(int d = 0; d < 8; ++d)
        {
            int nx = cx + dx[d];
            int ny = cy + dy[d];

            if(nx < 0 || nx >= width || ny < 0 || ny >= height)
                continue;

            int ni = ny * width + nx;

            if(map->river_map[ni] == 1 && !visited[ni])
            {
                visited[ni] = true;
                q.push(ni);
            }
        }
    }
}

internal_func void TallyBiomes(terrain_map *map, int biome_counts[biome_count], 
    int &river_cell_count, int &river_count)
{
    for(int j = 0; j < biome_count; ++j) { biome_counts[j] = 0; }
    river_cell_count = 0;
    river_count = 0;

    std::vector<bool32> visited(map_info::cell_count, false);
    for(int i = 0; i < map_info::cell_count; ++i)
    {
        if(map->river_map[i] != 0)
        {
            river_cell_count++;
            if(!visited[i])
            {
                river_count++;
                FloodFillRiver(map, i, visited);
            }
        }
        else
        {
            biome_counts[map->biome_map[i]]++;
        }
    }
}

internal_func void RenderMapToFile(terrain_map *map, const char *filename, uint32 &seed)
{
    biome_info info;
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }
    fprintf(fp, "Generated Map(Seed: %u):\n\n", seed);
    for(int y = 0; y < map->height_cells; ++y)
    {
        for(int x = 0; x < map->width_cells; ++x)
        {
            int i = y * map->width_cells + x;
            char glyph;
            if(map->river_map[i]) { glyph = '|'; }
            else
            {
                uint8 biome_id = map->biome_map[i];
                glyph = info.biome_glyphs[biome_id];
            }
            fputc(glyph, fp);
        }
        fputc('\n', fp);
    }

    int biome_counts[biome_count];
    int river_cell_count, river_count;
    TallyBiomes(map, biome_counts, river_cell_count, river_count);

    fprintf(fp, "\nBiome Coverage:\n");
    for(int i = 0; i < biome_count; ++i)
    {
        fprintf(fp, " %-12s: %c %4d cells (%5.2f%%)\n", 
            info.biome_names[i], 
            info.biome_glyphs[i], 
            biome_counts[i], 
            (real32)biome_counts[i] / (real32)map_info::cell_count * 100.0f);
    }
    fprintf(fp, "\nRiver Information:\n");
    fprintf(fp, " %-12s: %c %4d cells (%5.2f%%)\n", "River", '|', river_cell_count, 
        (real32)river_cell_count / (real32)map_info::cell_count * 100.0f);
    fprintf(fp, " Number of Rivers: %d\n", river_count);
    fclose(fp);
}

internal_func real32 GetNormalizedHeight(uint8 biome_id, real32 h)
{
    real32 min_h = 0.0f;
    real32 max_h = 1.0f;

    switch(biome_id)
    {
        case biome_deepwater: { min_h = 0.0f; max_h = 0.15f; break; }
        case biome_water: { min_h = 0.15f; max_h = 0.30f; break; }
        case biome_beach: { min_h = 0.30f; max_h = 0.35f; break; }
        case biome_mountain:
        case biome_snow: { min_h = 0.75f; max_h = 1.0f; break; }
        default: { min_h = 0.35f; max_h = 0.75f; break; }
    }

    real32 norm_h = (h - min_h) / (max_h - min_h);
    norm_h = norm_h < 0.0f ? 0.0f : (norm_h > 1.0f ? 1.0f : norm_h);
    return norm_h;
}

internal_func real32 GetBiomeShading(uint8 biome_id, real32 norm_h)
{
    real32 min_shade, max_shade;

    if(biome_id == biome_deepwater || biome_id == biome_water)
    {
        min_shade = 0.5f;
        max_shade = 1.0f;
    }
    else if(biome_id == biome_mountain || biome_id == biome_snow)
    {
        min_shade = 0.8f;
        max_shade = 1.3f;
    }
    else
    {
        min_shade = 0.7f;
        max_shade = 1.2f;
    }

    real32 shading = min_shade + norm_h * (max_shade - min_shade);
    return shading;
}

internal_func void RenderMapToPPM(terrain_map *map, const char *filename)
{
    local_persist const uint8 river_color[3] = { 60, 140, 220 };

    const int scale = 8;
    int pw = map->width_cells * scale;
    int ph = map->height_cells * scale;

    FILE *fp = fopen(filename, "wb");
    if(!fp) return;

    fprintf(fp, "P6\n%d %d\n255\n", pw, ph);

    for(int y = 0; y < map->height_cells; ++y)
    {
        for(int py = 0; py < scale; ++py)
        {
            for(int x = 0; x < map->width_cells; ++x)
            {
                int i = y * map->width_cells + x;
                uint8 final_color[3];
                if(map->river_map[i])
                {
                    for(int k = 0; k < 3; ++k)
                    { final_color[k] = river_color[k]; }
                }
                else
                {
                    uint8 biome_id = map->biome_map[i];
                    real32 cell_height = map->height_map[i];
                    const uint8 *base_color = biome_info::biome_colors[biome_id];

                    real32 norm_h = GetNormalizedHeight(biome_id, cell_height);
                    real32 shading = GetBiomeShading(biome_id, norm_h);

                    for(int j = 0; j < 3; ++j)
                    {
                        real32 shaded_channel = (real32)base_color[j] * shading;
                        shaded_channel = shaded_channel < 0.0f ? 0.0f : 
                            (shaded_channel > 255.0f ? 255.0f : shaded_channel);
                        final_color[j] = (uint8)shaded_channel;
                    }
                }

                for(int px = 0; px < scale; ++px)
                { fwrite(final_color, 1, 3, fp); }
            }
        }
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
    GenerateTemperatureMap(&map, seed);
    ClassifyBiomes(&map);
    GenerateRivers(&map, seed);

    RenderMapToFile(&map, "generated_map.txt", seed);
    RenderMapToPPM(&map, "generated_map.ppm");

    QueryPerformanceCounter(&end_time);
    real32 total_elapsed = (real32)(end_time.QuadPart - start_time.QuadPart) / 
        (real32)frequency.QuadPart * 1000.0f;
    
    printf("\nTotal Generation Time: %.02f ms\n", total_elapsed);

    GetMemoryUsage(&mem_after);
    real32 memory_delta_kb = (real32)(mem_after.working_set_size - mem_before.working_set_size) / 1024.0f;
    printf("\nProcess Memory Usage: %.02f KB\n", memory_delta_kb);

    return 0;
}