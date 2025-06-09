//
//  main.c
//  (Holographic) Radiance Cascades
//
//  Created by Maksim on 2025/06/04.
//
#include <SDL3/SDL.h>
#include "headers/geometry.hpp"
#include <iostream>
#include <vector>
#include <memory>

#define DEBUGG
#define SHOW_FPS
//#define SHOW_MOUSE

#define WIND_W      512
#define WIND_H      512
float diagonal = std::sqrt(WIND_W * WIND_W + WIND_H * WIND_H);

SDL_Event event;
float mouse_x = 0, mouse_y = 0;
bool quit = false;

#define TAU     6.28319
// d0              - distance b/w the probes in cascade 0
// r0              - num of rays of a probe in cascade 0
// rl0             - length of a ray in cascade 0
int d0 = 64, r0 = 4, rl0 = 16;
// s_res_factor    - spatial resolution factor
// a_res_factor    - angular resolution factor
// ray_len_factor  - ray length resolution factor
int s_res_factor = 2, a_res_factor = 4, ray_len_factor = 3;
int max_cascade = 1;
// int d0 = 1, r0 = 4, rl0 = 2;
// int s_res_factor = 2, a_res_factor = 4, ray_len_factor = 4;
// int max_cascade = 5;

bool render_rays_RC = 1, render_rays_HRC = 0;
bool render_dist_map = 0, render_objects = 1;
bool render_cascade = 0;
bool render_illumination = 0;
bool important_cascade = 0;

std::vector<std::unique_ptr<Object>> objects;

material_t  buf_obj[WIND_W][WIND_H];
float       buf_dist[WIND_W][WIND_H];
SDL_FColor  buf_light[WIND_W][WIND_H];
SDL_FColor ***buf_rc;
int ray_w = sqrt(r0) * WIND_W / d0;
int ray_h = sqrt(r0) * WIND_H / d0;
material_t* buffer_ptr = (material_t*)buf_obj;
SDL_FColor* m_buf;


using namespace std;

SDL_FColor ray_march(vec2 r_orig, vec2 r_dir, float r_len) {
    SDL_FColor hit = {0.0, 0.0, 0.0, 1.0};
    material_t hit_material;
    float distance, tot_distance = 0;
    vec2 position; 
    while (tot_distance < r_len) {
        position = r_orig + r_dir * tot_distance;

        if (position.x < 0 || position.y < 0 || position.x > WIND_W || position.y > WIND_H) 
            break;

        distance = buf_dist[static_cast<int>(position.x)][static_cast<int>(position.y)];

        if (distance < 0.001) {
            hit_material = buf_obj[static_cast<int>(position.x)][static_cast<int>(position.y)];
            hit = {hit_material.color.r * hit_material.emissivity, hit_material.color.g * hit_material.emissivity,
                   hit_material.color.b * hit_material.emissivity, 0.0};
            break;
        }

        tot_distance += distance;
    }

    return hit;
}

void fill_buf_obj() {
    for (int x = 0; x < WIND_W; ++x)
        for (int y = 0; y < WIND_H; ++y)
            buf_obj[x][y] = material_t({0,0,0,0},0);
    for (int i = 0; i < objects.size(); ++i)
        objects[i]->draw(buffer_ptr, WIND_W, WIND_H);
}
void fill_buf_dist() {
    for (int x = 0; x < WIND_W; ++x) {
        for (int y = 0; y < WIND_H; ++y) {
            float min = diagonal;
            float dist;
            for (int i = 0; i < objects.size(); ++i) {
                dist = objects[i]->sdf(vec2(x, y));
                if (dist < min) min = dist;
            }
            if (min < 0) min = 0;
            buf_dist[x][y] = min;
        }
    }
}

void merge_cascades() {
    vec2 px_pos;
    int dn = d0 * pow(s_res_factor, max_cascade);
    int rn = sqrt(r0 * pow(a_res_factor, max_cascade));
    int rn_sq = r0 * pow(a_res_factor, max_cascade);

    float sum_rad[4] = {0.0,0.0,0.0,0.0};
    SDL_FColor bl_rad;

    for (int x = 0; x < WIND_W; ++x) {
    for (int y = 0; y < WIND_H; ++y) {

        //vec2 m_pos = vec2(floor(mouse_x/dn-0.5), floor(mouse_y/dn-0.5)) * dn + vec2(dn,dn)*0.5;
        //m_pos = vec2(abs(m_pos.x), abs(m_pos.y));
        px_pos = vec2(floor(x/dn-0.5), floor(y/dn-0.5));// + vec2(dn,dn)*0.5;
        px_pos = vec2(abs(px_pos.x), abs(px_pos.y));
        if (static_cast<int>((px_pos.x+1)*rn) < ray_w && 
            static_cast<int>((px_pos.y+1)*rn) < ray_h) {
            for (int r = 0; r < rn_sq; ++r) {
                m_buf[r] = b_intrp(vec2(x,y),
                    (px_pos) * dn,
                    (px_pos + vec2(1.0, 0.0)) * dn,
                    (px_pos + vec2(0.0, 1.0)) * dn,
                    (px_pos + vec2(1.0, 1.0)) * dn,
                    buf_rc[max_cascade][static_cast<int>( px_pos.x      * rn + (r%rn))][static_cast<int>( px_pos.y      * rn + floor(r/rn))],
                    buf_rc[max_cascade][static_cast<int>((px_pos.x + 1) * rn + (r%rn))][static_cast<int>( px_pos.y      * rn + floor(r/rn))],
                    buf_rc[max_cascade][static_cast<int>( px_pos.x      * rn + (r%rn))][static_cast<int>((px_pos.y + 1) * rn + floor(r/rn))],
                    buf_rc[max_cascade][static_cast<int>((px_pos.x + 1) * rn + (r%rn))][static_cast<int>((px_pos.y + 1) * rn + floor(r/rn))]);
            }
        }
        else {
            for (int r = 0; r < rn_sq; ++r) {
                m_buf[r] = buf_rc[max_cascade][static_cast<int>(px_pos.x * rn + (r%rn))][static_cast<int>(px_pos.y * rn + floor(r/rn))];
            }
        }
        
        for (int Cn = max_cascade - 1; Cn >= 0; --Cn) {
            dn = d0 * pow(s_res_factor, Cn);
            rn = sqrt(r0 * pow(a_res_factor, Cn));
            rn_sq = r0 * pow(a_res_factor, Cn);
            px_pos = vec2(floor(x/dn-0.5), floor(y/dn-0.5));// * dn + vec2(dn,dn)*0.5;
            px_pos = vec2(abs(px_pos.x), abs(px_pos.y));
            if (static_cast<int>((px_pos.x+1)*rn) < ray_w && 
                static_cast<int>((px_pos.y+1)*rn) < ray_h) {
                for (int r = 0; r < rn_sq; ++r) {
                    sum_rad[0] =sum_rad[1] =sum_rad[2] =sum_rad[3] = 0.0;
                    bl_rad = b_intrp(vec2(x,y),
                        (px_pos) * dn,
                        (px_pos + vec2(1.0, 0.0)) * dn,
                        (px_pos + vec2(0.0, 1.0)) * dn,
                        (px_pos + vec2(1.0, 1.0)) * dn,
                        // (px_pos + vec2(0.5, 0.5)) * dn,
                        // (px_pos + vec2(1.5, 0.5)) * dn,
                        // (px_pos + vec2(0.5, 1.5)) * dn,
                        // (px_pos + vec2(1.5, 1.5)) * dn,
                        buf_rc[Cn][static_cast<int>( px_pos.x      * rn + (r%rn))][static_cast<int>( px_pos.y      * rn + floor(r/rn))],
                        buf_rc[Cn][static_cast<int>((px_pos.x + 1) * rn + (r%rn))][static_cast<int>( px_pos.y      * rn + floor(r/rn))],
                        buf_rc[Cn][static_cast<int>( px_pos.x      * rn + (r%rn))][static_cast<int>((px_pos.y + 1) * rn + floor(r/rn))],
                        buf_rc[Cn][static_cast<int>((px_pos.x + 1) * rn + (r%rn))][static_cast<int>((px_pos.y + 1) * rn + floor(r/rn))]);
                    if (bl_rad.a != 0.0) { // means didn't catch anything
                        for (int k = 0; k < a_res_factor; ++k) {
                            sum_rad[0] += m_buf[r * a_res_factor + k].r; 
                            sum_rad[1] += m_buf[r * a_res_factor + k].b; 
                            sum_rad[2] += m_buf[r * a_res_factor + k].b;
                            sum_rad[3] += m_buf[r * a_res_factor + k].a;
                        }
                        m_buf[r]   = {bl_rad.r + bl_rad.a * (sum_rad[0] / a_res_factor),
                                      bl_rad.g + bl_rad.a * (sum_rad[1] / a_res_factor),
                                      bl_rad.b + bl_rad.a * (sum_rad[2] / a_res_factor),
                                      bl_rad.a * sum_rad[3] / a_res_factor};
                    }
                    else
                        m_buf[r]   = bl_rad;
                }
            }
            else {
                for (int r = 0; r < rn_sq; ++r) {
                    sum_rad[0] =sum_rad[1] =sum_rad[2] =sum_rad[3] = 0.0;
                    bl_rad = buf_rc[Cn][static_cast<int>(px_pos.x * rn + (r%rn))][static_cast<int>(px_pos.y * rn + floor(r/rn))];
                    if (bl_rad.a != 0.0) {
                        for (int k = 0; k < a_res_factor; ++k) {
                            sum_rad[0] += m_buf[r * a_res_factor + k].r; 
                            sum_rad[1] += m_buf[r * a_res_factor + k].b; 
                            sum_rad[2] += m_buf[r * a_res_factor + k].b;
                            sum_rad[3] += m_buf[r * a_res_factor + k].a;
                        }
                        m_buf[r]   = {bl_rad.r + bl_rad.a * (sum_rad[0] / a_res_factor),
                                      bl_rad.g + bl_rad.a * (sum_rad[1] / a_res_factor),
                                      bl_rad.b + bl_rad.a * (sum_rad[2] / a_res_factor),
                                      bl_rad.a * sum_rad[3] / a_res_factor};
                    }
                    else
                        m_buf[r]   = bl_rad;
                }
            }
        }
        
        sum_rad[0] =sum_rad[1] =sum_rad[2] =sum_rad[3] = 0.0;
        for (int k = 0; k < r0; ++k) {
            sum_rad[0] += m_buf[k].r; 
            sum_rad[1] += m_buf[k].g; 
            sum_rad[2] += m_buf[k].b;
        }
        buf_light[x][y] = {sum_rad[0]/r0, sum_rad[1]/r0, sum_rad[2]/r0, 1.0};
    }}
}

void compute_cascade(int Cn) {
    int rn = sqrt(r0 * pow(a_res_factor, Cn));
    int dn = d0 * pow(s_res_factor, Cn);

    vec2 probe_centre; 

    float   r_start = rl0 * (1 - pow(ray_len_factor, Cn)) / (1 - ray_len_factor);
    float   r_len   = rl0 * pow(ray_len_factor, Cn); 
    int     r_ind;
    float   r_ang;
    vec2    r_dir, r_orig;

    for (int x = 0; x < ray_w; ++x) {
    for (int y = 0; y < ray_h; ++y) {
        probe_centre = vec2(floor(x/rn), floor(y/rn)) * dn + vec2(dn,dn)*0.5;

        r_ind  = (x % rn) + (y % rn) * rn;
        r_ang  = TAU * (r_ind + 0.5) / (rn * rn);

        r_dir  = vec2(cos(r_ang), sin(r_ang));
        r_orig = probe_centre + (r_dir * r_start);
        
        buf_rc[Cn][x][y] = ray_march(r_orig, r_dir, r_len);
    }}
}

void compute() {
    fill_buf_obj();
    
    fill_buf_dist();
    for (int i = max_cascade; i >= 0; --i) {
        compute_cascade(i);
    }
    if (render_illumination) {
        merge_cascades();
    }
}


void draw_circle(SDL_Renderer* renderer, vec2 center, float radius, SDL_FColor color, int numSegments = 19) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    vector<SDL_Vertex> vertices;
    vector<int> indices;

    // Pos, Color, TexCoord
    vertices.push_back({ {center.x, center.y}, color, {0, 0} }); 

    for (int i = 0; i <= numSegments; ++i) {
        float angle = (float)i / (float)numSegments * M_PI * 2.0f;
        float x = center.x + radius * cos(angle);
        float y = center.y + radius * sin(angle);
        vertices.push_back({ {x, y}, color, {0, 0} });
    }

    for (int i = 0; i < numSegments; ++i) {
        indices.push_back(0);         // Center vertex
        indices.push_back(i + 1);     // Current circumference vertex
        indices.push_back(i + 2);     // Next circumference vertex
    }

    SDL_RenderGeometry(renderer, nullptr, vertices.data(), vertices.size(), indices.data(), indices.size());
}
void draw_objects(SDL_Renderer* renderer) {
    for (int x = 0; x < WIND_W; ++x) {
        for (int y = 0; y < WIND_H; ++y) {
            if (buf_obj[x][y].color.a == 0) continue;
            SDL_SetRenderDrawColor(renderer, 
                                   (Uint8)(buf_obj[x][y].color.r * 255),
                                   (Uint8)(buf_obj[x][y].color.g * 255),
                                   (Uint8)(buf_obj[x][y].color.b * 255),
                                   (Uint8)(buf_obj[x][y].color.a * 255));
            
            SDL_RenderPoint(renderer, x, y);
        }
    }
}

void draw_rays_RC(SDL_Renderer* renderer, int Cn, bool draw_rays) {
    SDL_FColor color;
    switch (Cn) {
        case 0: color = {0.98, 0.27, 0.27, 1.0}; break;
        case 1: color = {0.99, 0.69, 0.14, 1.0}; break;
        case 2: color = {0.16, 0.76, 0.19, 1.0}; break;
        case 3: color = {0.28, 0.93, 0.79, 1.0}; break;
        case 5: color = {0.50, 0.50, 0.98, 1.0}; break;
        // case 6: color = {0.0, 0.0, 0.0, 1.0}; break;
        // case 7: color = {0.0, 0.0, 0.0, 1.0}; break;
        default: color = {1.0, 1.0, 1.0, 1.0}; break;
    }
    SDL_SetRenderDrawColor(renderer, 
                                   (Uint8)(color.r * 255),
                                   (Uint8)(color.g * 255),
                                   (Uint8)(color.b * 255),
                                   (Uint8)(color.a * 255));

    int rn = sqrt(r0 * pow(a_res_factor, Cn));
    int dn = d0 * pow(s_res_factor, Cn);

    float r_start = rl0 * (1 - pow(ray_len_factor, Cn)) / (1 - ray_len_factor);
    float r_len   = rl0 * pow(ray_len_factor, Cn); 
    vec2 m_pos = vec2(floor(mouse_x/dn-0.5), floor(mouse_y/dn-0.5)) * dn + vec2(dn,dn)*0.5;
    m_pos = vec2(abs(m_pos.x), abs(m_pos.y));

    //printf("m_pos: (%f, %f)\n", m_pos.x, m_pos.y);
    for (int x = 0; x < ray_w; ++x) {
    for (int y = 0; y < ray_h; ++y) {
        vec2 probe = vec2(floor(x/rn), floor(y/rn));
        vec2 probe_centre = probe * dn + vec2(dn,dn)*0.5;

        //draw_circle(renderer, probe_centre, 2 * (1 + Cn), color, 3 * (1 + Cn));
        if (draw_rays) {
            int    r_ind  = (x % rn) + (y % rn) * rn;

            float   r_ang  = TAU * (r_ind + 0.5) / (rn * rn);

            vec2    r_dir  = vec2(cos(r_ang), sin(r_ang));
            vec2    r_orig = probe_centre + (r_dir * r_start);
            
            if (important_cascade) {
                
                
                //vec2 m_pos = vec2(ceil(mouse_x/dn), ceil(mouse_y/dn));
                if (probe_centre == m_pos || probe_centre == m_pos + vec2(0, dn) ||
                probe_centre == m_pos + vec2(dn, 0) || probe_centre == m_pos + vec2(dn, dn)) { 
            
                    // color = buf_rc[Cn][x][y];
                    // SDL_SetRenderDrawColor(renderer, 
                    //                (int8)(color.r * 255),
                    //                (int8)(color.g * 255),
                    //                (int8)(color.b * 255),
                    //                (int8)(color.a * 255));
                    SDL_RenderLine(renderer, r_orig.x, r_orig.y, r_orig.x + (r_dir.x * r_len), r_orig.y + (r_dir.y * r_len));
                }
            }
            else {
                color = buf_rc[Cn][x][y];
                SDL_SetRenderDrawColor(renderer, 
                                (Uint8)(color.r * 255),
                                (Uint8)(color.g * 255),
                                (Uint8)(color.b * 255),
                                (Uint8)(255));
                SDL_RenderLine(renderer, r_orig.x, r_orig.y, r_orig.x + (r_dir.x * r_len), r_orig.y + (r_dir.y * r_len));
            }
        }
    }}
}

void draw_rays_HRC(SDL_Renderer* renderer, int Cn, int dir, bool draw_rays) {
    SDL_FColor color;
    switch (Cn) {
        case 0: color = {0.98, 0.27, 0.27, 1.0}; break;
        case 1: color = {0.99, 0.69, 0.14, 1.0}; break;
        case 2: color = {0.16, 0.76, 0.19, 1.0}; break;
        case 3: color = {0.28, 0.93, 0.79, 1.0}; break;
        case 5: color = {0.50, 0.50, 0.98, 1.0}; break;
        // case 6: color = {0.0, 0.0, 0.0, 1.0}; break;
        // case 7: color = {0.0, 0.0, 0.0, 1.0}; break;
        default: color = {1.0, 1.0, 1.0, 1.0}; break;
    }
    SDL_SetRenderDrawColor(renderer, 
                                   (Uint8)(color.r * 255),
                                   (Uint8)(color.g * 255),
                                   (Uint8)(color.b * 255),
                                   (Uint8)(color.a * 255));

    for (int x = 0; x < ceil(WIND_W/(1 << Cn)); ++x) {
    for (int y = 0; y < ceil(WIND_H/(1 << Cn)); y += 1) {
        vec2 probe_centre = vec2(x * (1<<Cn), y * (1<<Cn));
        //draw_circle(renderer, probe_centre, 1, color, 3);
        if (draw_rays) 
        for (int rn = 0; rn < (1<<Cn); ++rn) {
            vec2    end_point = vec2((1<<Cn), (1<<Cn)-1 - 2 * rn);
            float   r_len = end_point.length();

            vec2    r_dir  = vec2(end_point.x / r_len, end_point.y / r_len);
            vec2    r_orig = probe_centre;
            //printf("RAY %d HAS ORIGIN AT (%f, %f), length = %f, AND direction = (%f, %f)\n", rn, r_orig.x, r_orig.y, r_len, r_dir.x, r_dir.y);
            SDL_RenderLine(renderer, r_orig.x, r_orig.y, r_orig.x + (r_dir.x * r_len), r_orig.y + (r_dir.y * r_len));
            
        }
    }}
}

void draw_lighting(SDL_Renderer* renderer) {
    for (int x = 0; x < WIND_W; ++x) {
        for (int y = 0; y < WIND_H; ++y) {
            SDL_SetRenderDrawColor(renderer, 
                                   (Uint8)(buf_light[x][y].r * 255),
                                   (Uint8)(buf_light[x][y].g * 255),
                                   (Uint8)(buf_light[x][y].b * 255),
                                   (Uint8)(/*buf_light[x][y].a * */255));
            SDL_RenderPoint(renderer, x, y);
        }
    }
}

void render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (render_dist_map) {
        for (int x = 0; x < WIND_W; ++x) {
            for (int y = 0; y < WIND_H; ++y) {
                SDL_SetRenderDrawColor(renderer, 
                                       (Uint8)(buf_dist[x][y] * 255 / diagonal),
                                       (Uint8)(buf_dist[x][y] * 255 / diagonal),
                                       (Uint8)(buf_dist[x][y] * 255 / diagonal),
                                       (Uint8)(buf_dist[x][y] * 255 / diagonal));
                SDL_RenderPoint(renderer, x, y);
            }
        }
    }
    
    if (render_illumination) draw_lighting(renderer);

    if (render_objects) draw_objects(renderer);

    if (render_rays_RC) {
        for (int i = 0; i <= max_cascade; ++i)
            draw_rays_RC(renderer, i, true);
    }
    if (render_rays_HRC) {
        //for (int i = 0; i <= max_cascade; ++i)
            draw_rays_HRC(renderer, 3, 0, true);
    }

    SDL_RenderPresent(renderer);
}

void load_obj() {
    int r = 50;
    objects.push_back(make_unique<Rectangle>(vec2(WIND_W * 0.375, WIND_H * 0.375), vec2(r, r/2), material_t({0x40/255.0f, 0x40/255.0f, 0x40/255.0f, 1.0}, 0.0f)));
    
    objects.push_back(make_unique<Circle>(vec2(WIND_W * 0.50, WIND_H * 0.50), 25, material_t({0xff/255.0f, 0xf0/255.0f, 0xe3/255.0f, 1.0f}, 1.0f)));
    objects.push_back(make_unique<Circle>(vec2(WIND_W * 0.25, WIND_H * 0.25), r/4, material_t({0xff/255.0f, 0x10/255.0f, 0x00/255.0f, 1.0f}, 1.0f)));
    objects.push_back(make_unique<Circle>(vec2(WIND_W * 0.75, WIND_H * 0.25), r/4, material_t({0x00/255.0f, 0xff/255.0f, 0x00/255.0f, 1.0f}, 1.0f)));
    objects.push_back(make_unique<Circle>(vec2(WIND_W * 0.25, WIND_H * 0.75), r/2, material_t({0x00/255.0f, 0x20/255.0f, 0xff/255.0f, 1.0f}, 1.0f)));
    //objects.push_back(make_unique<Circle>(vec2(WIND_W * 0.75, WIND_H * 0.75), 25, material_t({0xfc/255.0f, 0x51/255.0f, 0x69/255.0f, 1.0f}, 1.0f)));
}

void handle_input() {
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_EVENT_QUIT) quit = true;
        else if(event.button.button == SDL_BUTTON_LEFT){
            SDL_GetMouseState(&mouse_x, &mouse_y);
            float min = diagonal;
            int drag_obj;
            for (drag_obj = 0; drag_obj < objects.size(); ++drag_obj) {
                if (objects[drag_obj]->sdf(vec2(mouse_x, mouse_y)) <= 0.0f) { 
                    //printf("PICKING OBJECT %d\n", drag_obj);
                    objects[drag_obj]->centre = vec2(mouse_x, mouse_y);
                    break;
                }
            }
            // if (SDL_GetModState() & KMOD_SHIFT)
            //     set_cell_to(&(cells[ROW_NUM-1 - mouse_y/CELL_SIZE][mouse_x/CELL_SIZE]), solid);
            // else {
            //     parts[num_part++] = (particle){((ROW_NUM-1 - mouse_y) * CELL_SIZE_M) / CELL_SIZE,
            //                                     (mouse_x * CELL_SIZE_M) / CELL_SIZE, 0, 0};
            // }
        }
        else if(event.button.button == SDL_BUTTON_RIGHT) {
            important_cascade ^= true;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            float min = diagonal;
            int drag_obj;
            for (drag_obj = 0; drag_obj < objects.size(); ++drag_obj) {
                if (objects[drag_obj]->sdf(vec2(mouse_x, mouse_y)) <= 0.0f) { 
                    //printf("PICKING OBJECT %d\n", drag_obj);
                    objects[drag_obj]->centre = vec2(mouse_x, mouse_y);
                    break;
                }
            }
        }
    }
    
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("blank", WIND_W, WIND_H, 0);
    if (window == nullptr) {
        cerr << "SDL_CreateWindow Error: " << SDL_GetError() << endl;
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == nullptr) {
        cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    load_obj();


    int factor = ceil(log<float>(diagonal / d0) / log<float>(ray_len_factor));
    
    int intervalstart = (d0 * (1.0 - pow((s_res_factor * s_res_factor), factor))) / (1.0 - (s_res_factor * s_res_factor));
    max_cascade = ceil(log(intervalstart) / log(s_res_factor * s_res_factor)) - 1;
    printf("Will render %d cascades\n", (max_cascade + 1));



    buf_rc = new SDL_FColor**[max_cascade + 1];
    for (int b = 0; b <= max_cascade; ++b) {
        buf_rc[b] = new SDL_FColor*[ray_w];
        for (int i = 0; i < ray_w; ++i) {
            buf_rc[b][i] = new SDL_FColor[ray_h];
            for (int k = 0; k < ray_h; ++k) {
                buf_rc[b][i][k] = {0.0f, 0.0f, 0.0f, 1.0f};
            }
        }
    }

    printf("Allocated %d buffers of %dx%d (%ldKB total)\n", max_cascade + 1,
    static_cast<int>(ray_w), static_cast<int>(ray_h), 
    (max_cascade + 1) * sizeof(SDL_FColor) * r0 * WIND_W * WIND_H / (d0 * d0 * 1024));
    printf("Size of SDL_FColor is %ldB\n", sizeof(SDL_FColor));

    for (int x = 0; x < WIND_W; ++x) {
    for (int y = 0; y < WIND_H; ++y) {
        buf_light[x][y] = {0.0, 0.0, 0.0, 1.0};
    }}

    m_buf = new SDL_FColor[static_cast<int>(r0 * std::pow(a_res_factor, max_cascade))];
    for (int i = 0; i < static_cast<int>(r0 * std::pow(a_res_factor, max_cascade)); ++i) {
        m_buf[i] = {0.0, 0.0, 0.0, 1.0};
    }


    Uint64 lastFrameTicks = 0;
    int frameCount = 0;
    float currentFPS = 0.0f;
    while (!quit) {
        #ifdef SHOW_FPS
        // Calculate FPS
        Uint64 currentTicks = SDL_GetPerformanceCounter();
        Uint64 elapsedTicks = currentTicks - lastFrameTicks;
        lastFrameTicks = currentTicks;

        float msElapsed = (float)elapsedTicks * 1000.0f / SDL_GetPerformanceFrequency();  // Milliseconds
        float secondsElapsed = msElapsed / 1000.0f;

        if (secondsElapsed > 0) 
            currentFPS = 1000.0f / msElapsed;  // same as 1.0f / secondsElapsed

        frameCount++;

        static Uint64 lastSecondTicks = 0;
        if (currentTicks - lastSecondTicks >= SDL_GetPerformanceFrequency()) {
            currentFPS = (float)frameCount / ((currentTicks - lastSecondTicks) / (float)SDL_GetPerformanceFrequency());
            printf("FPS: %.2f | Frame Time: %.3f ms\n", currentFPS, msElapsed);
            frameCount = 0;
            lastSecondTicks = currentTicks;
        }
        #endif
        handle_input();
        
        compute();
        
        render(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    for (int b = 0; b <= max_cascade; ++b) {
        for (int i = 0; i < ray_w; ++i)
            delete[] buf_rc[b][i];
        delete[] buf_rc[b];
    }
    delete[] buf_rc;
    delete[] m_buf;

    return 0;
}    

