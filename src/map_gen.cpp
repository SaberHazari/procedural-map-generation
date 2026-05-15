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
    const char *biome_colors[biome_count] = {
        hex_colors::dark_blue,
        hex_colors::bright_blue,
        hex_colors::bright_yellow,
        hex_colors::dark_yellow,
        hex_colors::green,
        hex_colors::bright_green,
        hex_colors::bold_green,
        hex_colors::bold_cyan,
        hex_colors::cyan,
        hex_colors::gray_white,
        hex_colors::dark_gray,
        hex_colors::bright_white
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

internal_func void RenderMapToHTML(terrain_map *map, const char *filename, uint32 seed)
{
    biome_info info;
    
    local_persist char file_buffer[map_info::cell_count * 50 + 16 * 1024];
    int pos = 0;

    pos += sprintf(file_buffer + pos, 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Procedural Map (Seed: %u)</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }\n"
        "        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"
        "        h1 { color: #333; }\n"
        "        .map-container { margin: 20px 0; font-family: monospace; line-height: 1.2; }\n"
        "        .map-cell { display: inline-block; width: 16px; height: 16px; text-align: center; line-height: 16px; font-weight: bold; font-size: 12px; }\n"
        "        .stats { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-top: 20px; }\n"
        "        .stat-section { border: 1px solid #ddd; padding: 15px; border-radius: 4px; }\n"
        "        .stat-section h3 { margin-top: 0; color: #333; border-bottom: 2px solid #0066cc; padding-bottom: 10px; }\n"
        "        .stat-row { display: flex; align-items: center; margin: 8px 0; }\n"
        "        .stat-color { display: inline-block; width: 20px; height: 20px; margin-right: 10px; border: 1px solid #999; }\n"
        "        .stat-name { flex: 1; }\n"
        "        .stat-value { font-weight: bold; color: #0066cc; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"container\">\n"
        "        <h1>Procedural Map Generator</h1>\n"
        "        <p><strong>Seed:</strong> %u</p>\n"
        "        <div class=\"map-container\">\n", seed, seed);

    for(int y = 0; y < map->height_cells; ++y)
    {
        for(int x = 0; x < map->width_cells; ++x)
        {
            int i = y * map->width_cells + x;
            char glyph;
            const char *color;
            
            if(map->river_map[i])
            {
                glyph = '|';
                color = hex_colors::river_blue;
            }
            else
            {
                uint8 b = map->biome_map[i];
                glyph = info.biome_glyphs[b];
                color = info.biome_colors[b];
            }
            
            pos += sprintf(file_buffer + pos, 
                "<div class=\"map-cell\" style=\"background-color: %s; color: white;\">%c</div>", 
                color, glyph);
        }
        pos += sprintf(file_buffer + pos, "<br>\n");
    }

    pos += sprintf(file_buffer + pos, "        </div>\n        <div class=\"stats\">\n");

    int biome_counts[biome_count];
    int river_cell_count, river_count;
    TallyBiomes(map, biome_counts, river_cell_count, river_count);

    pos += sprintf(file_buffer + pos, "            <div class=\"stat-section\">\n");
    pos += sprintf(file_buffer + pos, "                <h3>Biome Coverage</h3>\n");
    for(int i = 0; i < biome_count; ++i)
    {
        real32 percentage = (real32)biome_counts[i] / (real32)map_info::cell_count * 100.0f;
        pos += sprintf(file_buffer + pos, 
            "                <div class=\"stat-row\">\n"
            "                    <div class=\"stat-color\" style=\"background-color: %s;\"></div>\n"
            "                    <span class=\"stat-name\">%s:</span>\n"
            "                    <span class=\"stat-value\">%d cells (%.2f%%)</span>\n"
            "                </div>\n",
            info.biome_colors[i], info.biome_names[i], biome_counts[i], percentage);
    }
    pos += sprintf(file_buffer + pos, "            </div>\n");

    pos += sprintf(file_buffer + pos, "            <div class=\"stat-section\">\n");
    pos += sprintf(file_buffer + pos, "                <h3>River Information</h3>\n");
    real32 river_percentage = (real32)river_cell_count / (real32)map_info::cell_count * 100.0f;
    pos += sprintf(file_buffer + pos, 
        "                <div class=\"stat-row\">\n"
        "                    <div class=\"stat-color\" style=\"background-color: %s;\"></div>\n"
        "                    <span class=\"stat-name\">River Cells:</span>\n"
        "                    <span class=\"stat-value\">%d cells (%.2f%%)</span>\n"
        "                </div>\n"
        "                <div class=\"stat-row\">\n"
        "                    <span class=\"stat-name\">Number of Rivers:</span>\n"
        "                    <span class=\"stat-value\">%d</span>\n"
        "                </div>\n"
        "            </div>\n"
        "        </div>\n"
        "    </div>\n"
        "</body>\n"
        "</html>\n",
        hex_colors::river_blue, river_cell_count, river_percentage, river_count);

    FILE *fp = fopen(filename, "wb");
    if(!fp) { printf("Error: could not open %s\n", filename); return; }
    fwrite(file_buffer, 1, pos, fp);
    fclose(fp);
    printf("Map saved to %s\n", filename);
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

    RenderMapToHTML(&map, "generated_map.html", seed);

    QueryPerformanceCounter(&end_time);
    real32 total_elapsed = (real32)(end_time.QuadPart - start_time.QuadPart) / 
        (real32)frequency.QuadPart * 1000.0f;
    
    printf("\nTotal Generation Time: %.02f ms\n", total_elapsed);

    GetMemoryUsage(&mem_after);
    real32 memory_delta_kb = (real32)(mem_after.working_set_size - mem_before.working_set_size) / 1024.0f;
    printf("\nProcess Memory Usage: %.02f KB\n", memory_delta_kb);

    return 0;
}