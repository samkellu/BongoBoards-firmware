/* Copyright 2025 Sam Kelly (@samkellu)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "doom.h"
#include "math.h"

// Player information
static player_info player;

// Game management information
static uint32_t game_time;
static uint16_t last_frame;
static bool initialized = false;

// Gun animation
static int gun_x = GUN_X;
static int gun_y = GUN_Y + 10;
static int gun_anim_state = 0;

// Level
static segment* walls = NULL;
static int num_walls = 0;

// Enemy information
static enemy enemies[NUM_ENEMIES];
static uint16_t last_enemy_update;

#ifdef RENDER_DEBUG
    static int raycast_calls = 0;
#endif

// =================== MATH =================== //

float dot(vec2 u, vec2 v) { return u.x * v.x + u.y * v.y; }

double cross(vec2 u, vec2 v) { return u.x * v.y - u.y * v.x; }

float dist2(vec2 u, vec2 v) { return dot(sub(u, v), sub(u, v)); }

vec2 sub(vec2 u, vec2 v) { return (vec2) {u.x - v.x, u.y - v.y}; }

vec2 add(vec2 u, vec2 v) { return (vec2) {u.x + v.x, u.y + v.y}; }

float magnitude(vec2 u) { 
    float mag2 = u.x * u.x + u.y * u.y;
    return 1 / inv_sqrt(mag2);
}

vec2 norm(vec2 u) {
    float mag2 = u.x * u.x + u.y * u.y;
    float inv_mag = inv_sqrt(mag2);
    vec2 res = { u.x * inv_mag, u.y * inv_mag };
    return res;
}

vec2 proj(vec2 u, vec2 v) { 

    float t = dot(u, v) / dot(v, v);
    return (vec2) {t * v.x, t * v.y};
}

float point_ray_dist2(vec2 p, segment s) {

    vec2 up = sub(p, s.u);
    vec2 uv = sub(s.v, s.u);
    vec2 p_proj = add(proj(up, uv), s.u);
    vec2 u_p_proj = sub(p_proj, s.u);

    float k = uv.x != 0 ? u_p_proj.x / uv.x : u_p_proj.y / uv.y;
    return k <= 0 ? dist2(p, s.u) : k >= 1 ? dist2(p, s.v) : dist2(p, p_proj);
}

// Returns the distance along the ray at which the intersection occurs
float raycast(vec2 ray_origin, vec2 ray_direction, segment s, bool* hit) {

    #ifdef RENDER_DEBUG
        raycast_calls++;
    #endif

    if (hit) *hit = false;

    ray_direction = norm(ray_direction);
    vec2 u = sub(ray_origin, s.u);
    vec2 v = sub(s.v, s.u);
    vec2 r = { -ray_direction.y, ray_direction.x };

    float vr_dot = dot(v, r);

    // If segment is parallel to the ray
    if (vr_dot == 0)
        return -1.0f;

    float t = cross(v, u) / vr_dot;
    float q = dot(u, r) / vr_dot;

    if (t >= 0 && q >= 0 && q <= 1)
    {
        if (hit) *hit = true;
        return t;
    }

    return -1.0f;
}

bool point_lies_in_cone(segment cone_l, segment cone_r, vec2 u) {

    bool ccw_of_cone_r = (cone_r.v.x - cone_r.u.x) * (u.y - cone_r.u.y) < (cone_r.v.y - cone_r.u.y) * (u.x - cone_r.u.x);
    bool cw_of_cone_l = (cone_l.v.x - cone_l.u.x) * (u.y - cone_l.u.y) > (cone_l.v.y - cone_l.u.y) * (u.x - cone_l.u.x);
    return ccw_of_cone_r && cw_of_cone_l;
}

// A classic https://en.wikipedia.org/wiki/Fast_inverse_square_root
float inv_sqrt(float num) {
    
    float x2 = num * 0.5f, y = num;
    uint32_t i;
    memcpy(&i, &y, sizeof(float));
    i = 0x5f3759df - ( i >> 1 );
    memcpy(&y, &i, sizeof(float));
    return y * (1.5f - ( x2 * y * y ));
}

bool collision_detection(vec2 v, bool wall_collisions_only) {

    int collision_dist2 = WALL_COLLISION_DIST * WALL_COLLISION_DIST;
    for (int i = 0; i < num_walls; i++) {
        segment w = walls[i];
        float d2 = point_ray_dist2(v, w);
        if (d2 < collision_dist2) {
            if (i == 0 && player.score >= 5) {
                doom_setup();
            }

           return true;
        }
    }

    if (wall_collisions_only) return false;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy e = enemies[i];
        collision_dist2 = e.width * e.width;
        float d2 = dist2(v, e.pos);
        if (d2 < collision_dist2) return true;
    }

    return false;
}


// =================== MAP GENERATION =================== //


segment* bsp_wallgen(segment* walls, int* num_walls, int l, int r, int t, int b, int depth) {

    if (depth == 0) {
        // Prevent walls being created in small cells
        if (r - MIN_ROOM_WIDTH <= l + MIN_ROOM_WIDTH || b - MIN_ROOM_WIDTH <= t + MIN_ROOM_WIDTH) return walls;
        // Prevent thin walls being created
        if (r - l < MIN_WALL_WIDTH || b - t < MIN_WALL_WIDTH) return walls;

        walls = (segment*) realloc(walls, sizeof(segment) * (*num_walls + 4));
        walls[(*num_walls)++] = (segment) {
            {l + MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            {l + MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {l + MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            {r - MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {r - MIN_ROOM_WIDTH, t + MIN_ROOM_WIDTH},
            {r - MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            CHECK
        };

        walls[(*num_walls)++] = (segment) {
            {r - MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            {l + MIN_ROOM_WIDTH, b - MIN_ROOM_WIDTH},
            CHECK
        };

        return walls;
    }

    // Split on longest axis
    if (r - l >= b - t) {
        if (r - l < MIN_ROOM_WIDTH) return walls;
        
        int split = rand() % (r - l);
        if (split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, l + split, t, b, depth - 1);
        }

        if (r - l - split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l + split, r, t, b, depth - 1);
        }

    } else {
        if (b-t < MIN_ROOM_WIDTH) return walls;

        int split = rand() % (t - b);
        if (split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, r, t, t + split, depth - 1);
        }

        if (b - t - split > MIN_ROOM_WIDTH) {
            walls = bsp_wallgen(walls, num_walls, l, r, t + split, b, depth - 1);
        }
    }

    return walls;
}


// =================== GRAPHICS =================== //


#ifdef RENDER_DEBUG

void print_dll(dll* root) {

    dll* debug_curs = root;
    printf("Printing DLL: \n");
    if (!root) {
        printf("Root was null!\n\n");
        return;
    }

    while (debug_curs) {
        endpoint* e = (endpoint*) debug_curs->data;
        segment* s = e->segment;
        printf("%f : (%f,%f) -- (%f, %f) isEnd: %s ->\n", debug_curs->sorting_factor, s->u.x, s->u.y, s->v.x, s->v.y, e->is_end ? "yes" : "no");
        debug_curs = debug_curs->next;
    }
    
    printf("\n\n");
}

#endif

dll* merge_sort_dll(dll* root) {

    if (!root || !root->next)
        return root;

    // Split linked list
    dll* fast_ptr = root;
    dll* slow_ptr = root;
    while (fast_ptr && fast_ptr->next) {
        fast_ptr = fast_ptr->next->next;
        if (fast_ptr) {
            slow_ptr = slow_ptr->next;
        }
    }


    dll* r_root = slow_ptr->next;
    slow_ptr->next = NULL;

    dll* left = merge_sort_dll(root);
    dll* right = merge_sort_dll(r_root);

    // Merge
    dll new_root = {NULL, NULL, NULL, -1.0};
    dll* curs = &new_root;
    while (left || right) {
        if (left && (!right || left->sorting_factor < right->sorting_factor)) {
            curs->next = left;
            left->prev = curs;
            left = left->next;
        } else {
            curs->next = right;
            right->prev = curs;
            right = right->next;
        }
        
        curs = curs->next;
    }

    new_root.next->prev = NULL;
    return new_root.next;
}

// 2.5D raycast renderer for the map and entities around the player
void render_map(bool is_shooting) {

    #ifdef DS_DEBUG
        printf("\n\n=================== BEGIN FRAME ======================\n\n");
    #endif

    #ifdef RENDER_DEBUG
        raycast_calls = 0;
    #endif

    // Construct the left and right bounds of the FOV cone
    segment cone_l = {player.pos, {0, 0}};
    float bound_angle = player.angle - FOV_RADS / 2;
    cone_l.v.x = player.pos.x + DOV * cosf(bound_angle);
    cone_l.v.y = player.pos.y + DOV * sinf(bound_angle);

    segment cone_r = {player.pos, {0, 0}};
    bound_angle = player.angle + FOV_RADS / 2;
    cone_r.v.x = player.pos.x + DOV * cosf(bound_angle);
    cone_r.v.y = player.pos.y + DOV * sinf(bound_angle);
    
    // Stores the depth at each pixel and the phase of the wall it hit for occlusion later
    depth_buf_info depth_buf[SCREEN_WIDTH];
    segment ray = {player.pos, {0, 0}};

    dll* root = NULL;
    dll* curs = NULL;

    // bounds for the reverse of the fov cone, used for special case
    vec2 cone_r_dir = sub(cone_r.v, player.pos);
    segment neg_cone_r = {player.pos, sub(player.pos, cone_r_dir)};
    vec2 cone_l_dir = sub(cone_l.v, player.pos);
    segment neg_cone_l = {player.pos, sub(player.pos, cone_l_dir)};

    vec2 reference_vec = {cosf(player.angle), sinf(player.angle)};
    for (int i = 0; i < num_walls; i++) {
        segment wall = walls[i];
        bool relevant = false;

        // Check if either endpoint lies in fov cone
        bool u_in_fov = point_lies_in_cone(cone_l, cone_r, wall.u);
        bool v_in_fov = point_lies_in_cone(cone_l, cone_r, wall.v);
        relevant = u_in_fov || v_in_fov;

        // If not check if it fully intersects the cone
        if (!relevant) raycast(player.pos, reference_vec, wall, &relevant);

        // If wall doesn't intersect the FOV cone at all, skip it
        if (!relevant) continue;
        
        // Get angle of each endpoint relative to player's view direction (sort key for sweepline algorithm)
        vec2 point_vec = sub(wall.u, player.pos);
        float theta_u = atan2f(cross(reference_vec, point_vec), dot(point_vec, reference_vec));

        point_vec = sub(wall.v, player.pos);
        float theta_v = atan2f(cross(reference_vec, point_vec), dot(point_vec, reference_vec));

        // When walls have an endpoint behind the player, the incorrect endpoint may be calculated as the "start" and "end" 
        // as far as the sweepline is concerned. By checking the specific case and side of p which the wall lies on, we can alleviate this issue.
        if ((theta_u < 0) != (theta_v < 0) && u_in_fov != v_in_fov) {
            if (u_in_fov) {
                bool v_in_neg_fov = point_lies_in_cone(neg_cone_l, neg_cone_r, wall.v);
                if (v_in_neg_fov) {
                    bool wall_intersects_side_cone = (wall.v.x - wall.u.x) * (player.pos.y - wall.u.y) < (wall.v.y - wall.u.y) * (player.pos.x - wall.u.x);
                    theta_v += (wall_intersects_side_cone ? -1 : 1) * 2 * PI;
                }
            } else {
                bool u_in_neg_fov = point_lies_in_cone(neg_cone_l, neg_cone_r, wall.u);
                if (u_in_neg_fov) {
                    bool wall_intersects_side_cone = (wall.v.x - wall.u.x) * (player.pos.y - wall.u.y) < (wall.v.y - wall.u.y) * (player.pos.x - wall.u.x);
                    theta_u += (wall_intersects_side_cone ? 1 : -1) * 2 * PI;
                }
            }
        }
        
        bool u_is_end = theta_u > theta_v;

        // Break segments down into endpoints to facilitate sweepline rendering algorithm
        endpoint* u_point = (endpoint*) malloc(sizeof(endpoint));
        *u_point = (endpoint) {&walls[i], NULL, u_is_end};

        dll* u_node = (dll*) malloc(sizeof(dll));
        *u_node = (dll) {u_point, NULL, NULL, theta_u};
        if (curs) {
            curs->next = u_node;
            u_node->prev = curs;
            curs = curs->next;
        } else {
            curs = u_node;
            root = u_node;
        }
        
        endpoint* v_point = (endpoint*) malloc(sizeof(endpoint));
        *v_point = (endpoint) {&walls[i], u_node, !u_is_end};
        
        dll* v_node = (dll*) malloc(sizeof(dll));
        *v_node = (dll) {v_point, NULL, curs, theta_v};
        u_point->adjacent = v_node;
        curs->next = v_node;
        v_node->prev = curs;
        curs = curs->next;
    }

    #ifdef RENDER_DEBUG
        render_debug(root, cone_l, cone_r);
    #endif
    
    // Sort endpoints in clockwise order across the FOV cone
    root = merge_sort_dll(root);
    dll* sweep_curs = root;
    segment* closest_wall = NULL;

    #ifdef DS_DEBUG
        print_dll(root);
    #endif

    // Skips every second raycast on walls for performance
    for (int i = 0; i < SCREEN_WIDTH; i += 2) {
        float ray_angle = player.angle + (i * FOV_RADS / SCREEN_WIDTH) - (FOV_RADS / 2);
        
        ray.v.x = cosf(ray_angle);
        ray.v.y = sinf(ray_angle);
        ray.v = norm(ray.v);
        
        float theta = atan2f(cross(reference_vec, ray.v), dot(reference_vec, ray.v));
        float closest_distance = -1.0;

        // Only consider a segment as a rendering target if the sweepline has passed its "start" endpoint
        while (sweep_curs && sweep_curs->sorting_factor < theta) {

            endpoint* data = (endpoint*) sweep_curs->data;
            
            // Remove endpoints from "active" list behind cursor when they end
            if (data->is_end) {

                if (closest_wall == data->segment) {
                    closest_wall = NULL;
                }

                dll* next = data->adjacent->next;
                dll* prev = data->adjacent->prev;

                if (next) next->prev = prev;
                if (prev) prev->next = next;
                else root = next;

                free(data->adjacent->data);
                free(data->adjacent);

                next = sweep_curs->next;
                prev = sweep_curs->prev;

                if (next) next->prev = prev;
                if (prev) prev->next = next;
                else root = next;

                if (!next && !prev) root = NULL;

                free(sweep_curs->data);
                free(sweep_curs);

                sweep_curs = next;

            } else {
                // Check if newly encountered segment is closer than the previous closest active segment, if so update
                if (closest_wall) {
                    bool hit = false;
                    float hit_distance_new = raycast(ray.u, ray.v, *(data->segment), &hit);
                    if (hit) {
                        float hit_distance_current = raycast(ray.u, ray.v, *closest_wall, &hit);
                        if (hit_distance_new < hit_distance_current) {
                            closest_wall = data->segment;
                            closest_distance = hit_distance_new;
                        }
                    }
                }

                sweep_curs = sweep_curs->next;
            }
        }

        if (!closest_wall) {

            // Find the closest wall from the "Active" segment list behind the sweep cursor.
            // As segments are non-intersecting, we only need to check intersection with this "closest" segment for future rays
            // until this segment ends, or a new one begins, triggering this segment to be recalculated.
            curs = root;
            while (curs && curs != sweep_curs) {
                endpoint* e = (endpoint*) curs->data;
                segment* s = e->segment;
                curs = curs->next;
                bool hit = false;
                float hit_distance = raycast(ray.u, ray.v, *s, &hit);
                if (!hit) continue;
                
                if (closest_distance < 0 || hit_distance < closest_distance) {
                    closest_distance = hit_distance;
                    closest_wall = s;
                }
            }
        }
        
        depth_buf_info info = {MAX_VIEW_DIST, 0, 0, 0};
        
        // If there is a wall for the ray to hit.
        if (closest_wall) {

            // use precomputed value from getting closest wall if available
            if (closest_distance < 0) {
                closest_distance = raycast(ray.u, ray.v, *closest_wall, NULL);
            }

            info.depth = closest_distance;

            // Draws lines at the edges of walls
            vec2 hit_pt = { ray.u.x + ray.v.x * info.depth, ray.u.y + ray.v.y * info.depth };
            int wall_len = 1 / inv_sqrt(dist2(closest_wall->u, closest_wall->v));
            int wall2pt = 1 / inv_sqrt(dist2(closest_wall->u, hit_pt));
            
            #ifdef RENDER_DEBUG
                segment s = { ray.u, hit_pt };
                bresenham_line(s, 70);
            #endif
            
            info.length = 1000 / info.depth;
            switch (closest_wall->tex) {
                case CHECK:
                    info.phase = wall2pt % 10 < 5;
                    if (wall2pt < 2 || wall2pt > wall_len - 2) {
                        vertical_line(i, info.length, 1, 2);
                        
                    } else {
                        info.is_checked = true;
                        check_line(i, info.length, info.phase);
                    }
                    
                    break;

                case LINES:
                    info.phase = (wall2pt + 1) % 20 < 3;
                    vertical_line(i, info.length, 1, MAX(1, info.length - 1));

                    if (info.phase) {
                        vertical_line(i, info.length, 1, 1);
                    } else if (wall2pt < 2 || wall2pt > wall_len - 2) {
                        vertical_line(i, info.length, 1, 1);
                    }
                    
                    break;

                case DOOR:
                    vertical_line(i, info.length, 1, 1);
                    break;
            }
        }
        
        depth_buf[i] = info;
        depth_buf[i+1] = depth_buf[i];
    }

    // dealloc segment list
    while (root) {
        dll* val = root;
        root = root->next;
        free(val->data);
        free(val);
    }
    
    // TODO: New collection for renderable entities rather than enemies.
    // Order by distance for proper occlusion.

    render_obj* objects = NULL;
    int n_objects = 0;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy* e = &enemies[i];
        int to_add = e->projectile.active ? 2 : 1;
        objects = (render_obj*) realloc(objects, sizeof(render_obj) * (n_objects + to_add));

        vec2 e_vec = sub(e->pos, player.pos);
        float enemy_angle = atan2f(cross(reference_vec, e_vec), dot(reference_vec, e_vec));
        if (is_shooting && enemy_angle >= -FOV_RADS / 8 && enemy_angle < FOV_RADS / 8) {
            objects[n_objects++] = (render_obj) {&e->s_hurt[e->anim_state], e->pos};
            if (--e->health < 0) {
                reload_enemy(e);
                player.score++;
            }
        } else {
            objects[n_objects++] = (render_obj) {&e->s[e->anim_state], e->pos};
        }

        if (e->projectile.active) {
            objects[n_objects++] = (render_obj) {e->projectile.s, e->projectile.pos};
        }
    }

    for (int i = 0; i < n_objects; i++) {
        
        render_obj obj = objects[i];
        vec2 to_obj = sub(obj.pos, player.pos);
        float obj_angle = atan2f(cross(reference_vec, to_obj), dot(reference_vec, to_obj));

        if (obj_angle < -FOV_RADS / 2 || obj_angle > FOV_RADS / 2) continue;
        
        // Walk across lateral pixels affected by sprite, if any have depth more than object distance draw the object.
        float obj_dist = magnitude(to_obj);
        int scale_height = obj.s->height * 50 / obj_dist;
        int scale_width = obj.s->width * 50 / obj_dist;
        
        int obj_screen_x = SCREEN_WIDTH * (obj_angle + (FOV_RADS / 2) / FOV_RADS);
        int obj_screen_l = MAX(MIN(obj_screen_x - scale_width / 2, SCREEN_WIDTH - 1), 0);
        int obj_screen_r = MAX(MIN(obj_screen_x + scale_width / 2, SCREEN_WIDTH - 1), 0);

        bool draw = false;
        for (int j = obj_screen_l; j < obj_screen_r; j++) {
            if (depth_buf[j].depth > obj_dist) {
                draw = true;
                break;
            }
        }

        if (!draw) continue;

        int obj_screen_y = WALL_OFFSET - scale_height / 3;
        oled_write_bmp_P_scaled(*obj.s, scale_height, scale_width, obj_screen_x - scale_width / 2, obj_screen_y);

        // Redraw walls where entity sprite should be behind
        for (int j = obj_screen_l; j < obj_screen_r; j++) {
            depth_buf_info info = depth_buf[j];
            if (info.depth > obj_dist) continue;

            for (int k = 0; k < UI_HEIGHT; k++) {
                oled_write_pixel(j, k, 0);
            }
            
            vertical_line(j, SCREEN_HEIGHT, 0, 1);
            if (j % 2 != 0) continue;

            if (info.is_checked) {
                check_line(j, info.length, info.phase);
            
            } else {
                vertical_line(j, info.length, 1, 2);
            }
        }
    }

    free(objects);
}

void draw_gun(bool moving, bool show_flash) {

    // Walking animation
    if (moving) {
        gun_anim_state = (gun_anim_state + 5) % 7200;
        gun_x = GUN_X + 6 * sin((float) gun_anim_state * 0.06);
        gun_y = GUN_Y + 3 + 3 * cos((float) (gun_anim_state + 90) * 0.12);
    
    // Slowly move gun back to centre when not moving
    } else {
        gun_anim_state = 0;
        if (gun_y != GUN_Y) gun_y += GUN_Y > gun_y ? 1 : -1;
        if (gun_x != GUN_X) {
            int inc = abs(GUN_X - gun_x) > 2 ? 2 : 1;
            gun_x += GUN_X > gun_x ? inc : -inc;
        }
    } 

    oled_write_bmp_P(gun_sprite, gun_x - GUN_WIDTH / 2, gun_y - GUN_HEIGHT);
    if (show_flash) {
        oled_write_bmp_P(muzzle_flash_sprite, gun_x - FLASH_WIDTH / 2 + 2, gun_y - GUN_HEIGHT - 3 * FLASH_HEIGHT / 4);
    }
}

void vertical_line(int x, int half_length, bool color, int skip) {
    
    for (int i = 0; i < half_length; i += skip) {
        if (WALL_OFFSET - i >= 0) {
            oled_write_pixel(x, WALL_OFFSET - i, color);
        }

        // Ensures that the wall doesnt overlap with the UI
        if (WALL_OFFSET + i < UI_HEIGHT) {
            oled_write_pixel(x, WALL_OFFSET + i, color);
        }
    }
}

void check_line(int x, int half_length, bool phase) {
   
    int lower = WALL_OFFSET - half_length;
    int upper = WALL_OFFSET + half_length;

    for (int i = lower; i < upper; i+=2) {
        if (phase) {
            if (i == lower || (i >= lower + half_length && i <= lower + 3 * half_length / 2)) {
                i += half_length / 2;
            }
        } else {
            if ((i >= lower + half_length / 2 && i <= lower + half_length) || (i >= lower + 3 * half_length / 2 && i <= upper)) {
                i += half_length / 2;
            }
        }
        
        if (i > UI_HEIGHT) break;
        
        oled_write_pixel(x, i, 1);
    }

    oled_write_pixel(x, WALL_OFFSET - half_length, 1);
    if (WALL_OFFSET + half_length < UI_HEIGHT) {
        oled_write_pixel(x, WALL_OFFSET + half_length, 1);
    }
}

void oled_write_bmp_P(sprite img, int x, int y) {

    int row = 0, col = 0;
    for (int i = 0; i < img.size; i++) {
        uint8_t c = pgm_read_byte(img.bmp++);
        uint8_t m = img.mask == NULL ? 0x00 : pgm_read_byte(img.mask++);

        for (int j = 0; j < 8; j++) {
            bool px = c & (1 << (7 - j));
            bool pxm = m & (1 << (7 - j));
            if (px) oled_write_pixel(x + col, y + row, true);
            if (pxm) oled_write_pixel(x + col, y + row, false);
            if (++col == img.width) {
                row++;
                col = 0;
                if (row + y >= UI_HEIGHT) return;
            }
        }
    }
}

void oled_write_bmp_P_scaled(sprite img, int draw_height, int draw_width, int x, int y) {

    if (draw_height < 1 || draw_width < 1) return;

    int row = 0, col = 0;
    for (int i = 0; i < img.size; i++) {
        uint8_t c = pgm_read_byte(img.bmp++);
        uint8_t m = pgm_read_byte(img.mask++);

        for (int j = 0; j < 8; j++) {
            int draw_row = draw_height * row / img.height;

            int draw_row_lim = draw_height * (row + 1) / img.height;
            int draw_col = draw_width * col / img.width;
            int draw_col_lim = draw_width * (col + 1) / img.width;
            bool px = c & (1 << (7 - j));
            bool pxm = m & (1 << (7 - j));

            for (int k = draw_row; k < draw_row_lim; k++) {
                if (y + k < 0) continue;
                if (y + k >= UI_HEIGHT) break;

                for (int l = draw_col; l < draw_col_lim; l++) {
                    if (x + l < 0) continue;
                    if (x + l >= SCREEN_WIDTH) break;
                    if (px) oled_write_pixel(x + l, y + k, true);
                    if (pxm) oled_write_pixel(x + l, y + k, false);
                }
            }

            if (++col == img.width) {
                row++;
                col = 0;
            }
        }
    }
}

// =================== ENEMY LOGIC ===================

void reload_enemy(enemy* e) {

    e->health = 10;
    while (1) {
        e->pos = get_valid_spawn();
        if (dist2(e->pos, player.pos) > e->width * e->width) return;
    }
}

void enemy_update() {

    int enemy_vision_range2 = ENEMY_VIEW_DISTANCE * ENEMY_VIEW_DISTANCE;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemy* e = &enemies[i];
        float player_dist2 = dist2(e->pos, player.pos);
        if (player_dist2 > enemy_vision_range2) continue;

        // Move towards player if not too close
        if (abs(e->pos.y - player.pos.y) > 1) {
            vec2 eny = {e->pos.x, e->pos.y + ENEMY_WALK_SPEED * (player.pos.y - e->pos.y > 0 ? 1 : -1)};
            if (!collision_detection(eny, true)) e->pos.y = eny.y;
        }

        if (abs(e->pos.x - player.pos.x) > 1) {
            vec2 enx = {e->pos.x + ENEMY_WALK_SPEED * (player.pos.x - e->pos.x > 0 ? 1 : -1), e->pos.y};
            if (!collision_detection(enx, true)) e->pos.x = enx.x;
        }

        // Attack if possible
        if (e->attack_cooldown-- == 0)
        {
            e->attack_cooldown = ENEMY_SHOT_COOLDOWN;
            e->projectile.active = true;
            e->projectile.pos = e->pos;
            e->projectile.direction = norm(sub(player.pos, e->pos));
        }
    }
}

void enemy_attack_update() {

    for (int i = 0; i < NUM_ENEMIES; i++) {
        projectile* proj = &enemies[i].projectile;
        if (!proj->active) continue;

        // Calculate new position
        vec2 npos = {
            proj->pos.x + proj->direction.x * PROJECTILE_SPEED,
            proj->pos.y + proj->direction.y * PROJECTILE_SPEED
        };

        // Handle collision with walls
        bool hit = collision_detection(npos, true);
        if (hit) {
            proj->active = false;
        } else {
            proj->pos = npos;
        }

        // Handle collision with player. Immunity timer preveents multiple hits landing at the same time
        if (!player.immunte_timer && dist2(proj->pos, player.pos) <= WALL_COLLISION_DIST) {
            player.hp--;
            player.immune_timer = PLAYER_IMMUNITY_TIMER;
            proj->active = false;
        }
    }
}


// =================== GAME LOGIC =================== //


// Returns a position on the map which is not within a wall.
vec2 get_valid_spawn(void) {

    int col_dist2 = WALL_COLLISION_DIST * WALL_COLLISION_DIST;
    while (1) {
        vec2 new_pos = {
            col_dist2 + (rand() % (MAP_WIDTH - col_dist2)),
            col_dist2 + (rand() % (MAP_HEIGHT - col_dist2))
        };

        bool valid = true;
        for (int i = 6; i < num_walls; i += 4) {
            vec2 lt = walls[i+1].u;
            vec2 rb = walls[i+3].u;

            valid &= new_pos.x > rb.x + col_dist2
                  || new_pos.x < lt.x - col_dist2
                  || new_pos.y < lt.y - col_dist2
                  || new_pos.y > rb.y + col_dist2;
                        
            if (!valid) continue;
        }

        if (valid) return new_pos;
    }
}

void doom_setup(void) {

    // Runs intro sequence
    oled_write_bmp_P(doom_logo_sprite, 0, 0);
    game_time = timer_read();
    srand(game_time);

    // Initializes the map and door
    walls = (segment*) malloc(sizeof(segment) * 6);
    num_walls = 1;
    walls[num_walls++] = (segment) {{0, MAP_HEIGHT}, {MAP_WIDTH, MAP_HEIGHT}, CHECK};
    walls[num_walls++] = (segment) {{MAP_WIDTH, MAP_HEIGHT}, {MAP_WIDTH, 0}, CHECK};
    walls[num_walls++] = (segment) {{MAP_WIDTH, 0}, {0, 0}, CHECK};
    walls[num_walls++] = (segment) {{0, 0}, {0, MAP_HEIGHT}, CHECK};

    segment* door_wall = &walls[1 + rand() % 4];
    float wall_len = sqrtf(dist2(door_wall->v, door_wall->u));
    float door_width_perc = DOOR_WIDTH / wall_len;
    float door_placement_perc = (rand() % (int) (100 - door_width_perc)) / (float) 100;
    float dx = door_wall->v.x - door_wall->u.x;
    float dy = door_wall->v.y - door_wall->u.y;

    vec2 door_start = {
        door_wall->u.x + (dx * door_placement_perc),
        door_wall->u.y + (dy * door_placement_perc)
    };

    vec2 door_end = {
        door_wall->u.x + (dx * (door_placement_perc + door_width_perc)),
        door_wall->u.y + (dy * (door_placement_perc + door_width_perc))
    };

    walls[0] = (segment) {door_start, door_end, DOOR};
    // Truncate wall to remove door hole
    walls[num_walls++] = (segment) {door_end, {door_wall->v.x, door_wall->v.y}, CHECK};
    door_wall->v = door_start;

    walls = bsp_wallgen(walls, &num_walls, 0, MAP_WIDTH, 0, MAP_HEIGHT, MAP_GEN_REC_DEPTH);

    // Initializes the list of possible enemy spawn locations
    for (int i = 0; i < NUM_ENEMIES; i++) {
        enemies[i] = (enemy) {
            get_valid_spawn(),
            10,
            8,
            0,
            imp_sheet,
            sizeof(imp_sheet),
            imp_hurt_sheet,
            sizeof(imp_hurt_sheet),
            ENEMY_SHOT_COOLDOWN,
            (projectile) {
                (vec2) { 0, 0 },
                (vec2) { 0, 0 },
                &fireball_sprite,
                false
            }
        };
    }

    // Initializes player state
    player.pos = get_valid_spawn();
    player.angle = 0;
    player.shot_timer = 0;
    player.score = 0;
    player.has_key = false;
    last_frame = timer_read();
    initialized = true;

    #ifdef RENDER_DEBUG
        // Use emulator to render
        render();
    #endif
}

void doom_dispose(void) {
    
    free(walls);
    initialized = false;
}

#ifdef RENDER_DEBUG
void bresenham_line(segment s, int offset)
{
    int x0 = (int) s.u.x + offset,
        x1 = (int) s.v.x + offset,
        y0 = (int) s.u.y + offset,
        y1 = (int) s.v.y + offset;

    int dx =  abs (x1 - x0),
        sx = x0 < x1 ? 1 : -1;

    int dy = -abs (y1 - y0),
        sy = y0 < y1 ? 1 : -1;

    int err = dx + dy,
        e2;
   
    // Bresenham from https://gist.github.com/bert/1085538
    for (;;) {
        oled_write_pixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { 
            err += dy; 
            x0 += sx;
        }

        if (e2 <= dx) { 
            err += dx;
            y0 += sy;
        }
    }
}

void render_debug(dll* root, segment cone_l, segment cone_r) {

    int offset = 70;
    dll* curs = root;
    while (curs)
    {
        bresenham_line(*((endpoint*) curs->data)->segment, offset);
        curs = curs->next;
    }

    bresenham_line(walls[0], offset+1);
    bresenham_line(walls[0], offset+2);
    bresenham_line(walls[0], offset-1);
    bresenham_line(walls[0], offset-2);
    
    oled_write_pixel(player.pos.x + offset, player.pos.y + offset, 1);
    
    bresenham_line(cone_l, offset);
    bresenham_line(cone_l, offset+1);
    bresenham_line(cone_l, offset-1);
    bresenham_line(cone_r, offset);
    bresenham_line(cone_r, offset+1);
    bresenham_line(cone_r, offset-1);
    
    for (int i = 0; i < NUM_ENEMIES; i++)
    {
        enemy e = enemies[i];
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset - 1, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset + 1, e.pos.y + offset, 1);
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset - 1, 1);
        oled_write_pixel(e.pos.x + offset, e.pos.y + offset + 1, 1);

        if (e.projectile.active) {
            oled_write_pixel(e.projectile.pos.x + offset, e.projectile.pos.y + offset, 1);
            oled_write_pixel(e.projectile.pos.x + offset - 1, e.projectile.pos.y + offset, 1);
            oled_write_pixel(e.projectile.pos.x + offset + 1, e.projectile.pos.y + offset, 1);
            oled_write_pixel(e.projectile.pos.x + offset, e.projectile.pos.y + offset - 1, 1);
            oled_write_pixel(e.projectile.pos.x + offset, e.projectile.pos.y + offset + 1, 1);
        }
    }
}
#endif

void doom_update(controls c) {

    if (!initialized || timer_elapsed32(game_time) < START_TIME_MILLI) return;
    
    // Limit framerate
    uint16_t time_elapsed = timer_elapsed(last_frame);
    if (time_elapsed < FRAME_TIME_MILLI) return;
    
    // Update player state
    if (player.immune_timer > 0) player.immune_timer--;
    if (player.shot_timer > 0) player.shot_timer--;
    if (c.shoot && player.shot_timer == 0) player.shot_timer = PLAYER_SHOT_COOLDOWN;

    if (c.l) {
        player.angle -= ROTATION_SPEED_RADS < 0 ? ROTATION_SPEED_RADS + 2 * PI : ROTATION_SPEED_RADS;
    }

    if (c.r) {
        player.angle += ROTATION_SPEED_RADS >= 2 * PI ? ROTATION_SPEED_RADS - 2 * PI : ROTATION_SPEED_RADS;
    }

    if (!c.d != !c.u) {
        int walk_dist = c.u ? WALK_SPEED : -WALK_SPEED;
        vec2 pnx = {player.pos.x + walk_dist * cosf(player.angle), player.pos.y};
        vec2 pny = {player.pos.x, player.pos.y + walk_dist * sinf(player.angle)};
        if (!collision_detection(pnx, false)) player.pos.x = pnx.x;
        if (!collision_detection(pny, false)) player.pos.y = pny.y;
    }
    
    // Update enemy animation states
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (i == 0) {
            enemies[i].anim_state = time_elapsed % 2000 < 1000 ? 0 : 1;
        } else {
            enemies[i].anim_state = enemies[0].anim_state;
        }
    }

    // Update enemy positions and projectiles
    if (timer_elapsed(last_enemy_update) > ENEMY_UPDATE_RATE) {
        last_enemy_update = timer_read();
        enemy_update();
        enemy_attack_update();
    }

    // Render the map and entities
    oled_clear();
    render_map(player.shot_timer > 0 && c.shoot);
    draw_gun(c.u, player.shot_timer > 0);

    // Draw the UI elements
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        oled_write_pixel(i, UI_HEIGHT, 1);
    }

    #ifdef RENDER_DEBUG
        time_elapsed = timer_elapsed32(last_frame);
        int fpms = 1000 / (float) time_elapsed;
        oled_set_cursor(0, 0);
        oled_write("FPS:", false);
        oled_write(get_u16_str(fpms, ' '), false);
        oled_write("num raycast calls:", false);
        oled_write(get_u16_str(raycast_calls, ' '), false);
        
        render();
        last_frame = timer_read();
        return;
    #endif
    
    // Displays the current game time
    oled_set_cursor(0, 7);
    oled_write_P(PSTR("TIME: "), false);
    oled_write(get_u16_str((timer_elapsed(game_time) - START_TIME_MILLI) / 1000, ' '), false);
    
    // Displays the players current score
    oled_set_cursor(12, 7);
    oled_write_P(PSTR("SCORE:"), false);
    oled_write(get_u8_str(player.score, ' '), false);

    last_frame = timer_read();
}

const char* get_u32_str(uint32_t value, char pad) {
    static char buf[11] = {0};
    buf[10] = '\0';

    for (int i = 0; i < 10; i++) {
        char c = 0x30 + value % 10;
        buf[9 - i] = (c == 0x30 && i == 0) ? c : value > 0 ? c : pad;
        value /= 10;
    }

    return buf;
}