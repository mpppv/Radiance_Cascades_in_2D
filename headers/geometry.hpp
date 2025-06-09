#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>
#include <iostream>

const float EPSILON = 1e-4f;


struct vec2 {
    float x, y;

    vec2() : x(0), y(0) {}
    vec2(float _x, float _y) : x(_x), y(_y) {}
    vec2(float _a) : x(_a), y(_a) {}

    bool operator==(const vec2 &o) const { return x == o.x && y == o.y; }
    bool operator>=(const vec2 &o) const { return x >= o.x && y >= o.y; }
    bool operator<=(const vec2 &o) const { return x <= o.x && y <= o.y; }

    vec2 operator+(const vec2 &o) const { return vec2(x + o.x, y + o.y); }
    vec2 operator-(const vec2 &o) const { return vec2(x - o.x, y - o.y);  }
    vec2 operator*(float s) const { return vec2(x * s, y * s); }
    vec2 operator/(float s) const {
        if (s == 0.0f) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Division by zero in vec2 operator/");
            return vec2(0, 0);
        }
        return vec2(x / s, y / s);
    }
    float length_sq() const { return x * x + y * y; }
    float length() const { return std::sqrt(length_sq()); }

    vec2 norm() const {
        float len = length();
        if (len == 0.0f) {
            return vec2(0, 0);
        }
        return *this / len;
    }
};

inline float dot(vec2 a, vec2 b) {
    return a.x * b.x + a.y * b.y;
}

struct ClosestObjectInfo {
    bool hasObject = false;
    int objectID = -1;
    vec2 closestPointOnObject;
};



struct material_t {
    SDL_FColor color;
    float emissivity;      // 0.0 - 1.0

    material_t() : color{0.0f, 0.0f, 0.0f, 1.0f}, emissivity(0.0f) {}
    material_t(SDL_FColor _color, float _emissivity) : color(_color), emissivity(_emissivity) {}
};



enum shapes {
    LINE,
    RECTANGLE,
    CIRCLE,
    TRIANGLE
};


class Object {
public:
    unsigned int shape;
    vec2 centre;
    material_t material;

    Object() : shape(CIRCLE), centre(0, 0), material() {}

    Object(unsigned int _shape, vec2 _centre, struct material_t _material)
        : shape(_shape), centre(_centre), material(_material) {}

    virtual ~Object() = default;

    virtual float sdf(vec2 point) = 0;

    virtual void draw(material_t* buf, int scr_w, int scr_h) = 0;

    virtual vec2 get_normal(vec2 incident) {
        // Numerical gradient calculation as a default
        const float epsilon = 0.001f;
        float grad_x = sdf(vec2(incident.x + epsilon, incident.y)) - sdf(vec2(incident.x - epsilon, incident.y));
        float grad_y = sdf(vec2(incident.x, incident.y + epsilon)) - sdf(vec2(incident.x, incident.y - epsilon));
        return vec2(grad_x, grad_y).norm();
    }
};

// --- Derived Shape Classes ---

class Circle : public Object {
public:
    float radius;

    Circle(vec2 _centre, float _radius, struct material_t _material)
        : Object(CIRCLE, _centre, _material), radius(_radius) {}

    float sdf(vec2 point) override {
        return (point - centre).length() - radius;
    }

    void draw(material_t* buf, int scr_w, int scr_h) override {
        int x_start = static_cast<int>(centre.x - radius);
        x_start = x_start < 0 ? 0 : x_start;
        int x_end = static_cast<int>(centre.x + radius);
        x_end = x_end > scr_w ? scr_w : x_end;
        int y_start = static_cast<int>(centre.y - radius);
        y_start = y_start < 0 ? 0 : y_start;
        int y_end = static_cast<int>(centre.y + radius);
        y_end = y_end > scr_h ? scr_h : y_end;

        for (int x = x_start; x < x_end; ++x) {
            for (int y = y_start; y < y_end; ++y) {
                if (this->sdf(vec2(static_cast<float>(x), static_cast<float>(y))) <= 0) {
                    *(buf + x * scr_h + y) = this->material;
                }
            }
        }
    }
};

class Rectangle : public Object {
public:
    vec2 size;

    Rectangle(vec2 _centre, vec2 _size, struct material_t _material)
        : Object(RECTANGLE, _centre, _material), size(_size) {}

    float sdf(vec2 point) override {
        vec2 p = point - centre;
        vec2 d = vec2(std::abs(p.x), std::abs(p.y)) - size;
        return (vec2(std::max(d.x, 0.0f), std::max(d.y, 0.0f))).length() + std::min(std::max(d.x, d.y), 0.0f);
    }

    void draw(material_t* buf, int scr_w, int scr_h) override {
        int x_start = static_cast<int>(centre.x - size.x);
        x_start = x_start < 0 ? 0 : x_start;
        int x_end = static_cast<int>(centre.x + size.x);
        x_end = x_end > scr_w ? scr_w : x_end;
        int y_start = static_cast<int>(centre.y - size.y);
        y_start = y_start < 0 ? 0 : y_start;
        int y_end = static_cast<int>(centre.y + size.y);
        y_end = y_end > scr_h ? scr_h : y_end;

        for (int x = x_start; x < x_end; ++x) {
            for (int y = y_start; y < y_end; ++y) {
                if (this->sdf(vec2(static_cast<float>(x), static_cast<float>(y))) <= 0) {
                    *(buf + x * scr_h + y) = this->material;
                }
            }
        }
    }
};

class Triangle : public Object {
public:
    vec2 p0, p1, p2;

    Triangle(vec2 _p0, vec2 _p1, vec2 _p2, struct material_t _material)
        : Object(TRIANGLE, (_p0 + _p1 + _p2) / 3.0f, _material), p0(_p0), p1(_p1), p2(_p2) {}

    float sdf(vec2 point) override {
        vec2 e0 = p1 - p0; vec2 v0 = point - p0;
        vec2 e1 = p2 - p1; vec2 v1 = point - p1;
        vec2 e2 = p0 - p2; vec2 v2 = point - p2;

        vec2 pq0 = v0 - e0 * std::max(0.0f, std::min(1.0f, dot(v0, e0) / dot(e0, e0)));
        vec2 pq1 = v1 - e1 * std::max(0.0f, std::min(1.0f, dot(v1, e1) / dot(e1, e1)));
        vec2 pq2 = v2 - e2 * std::max(0.0f, std::min(1.0f, dot(v2, e2) / dot(e2, e2)));

        float s = e0.x * e2.y - e0.y * e2.x;
        float d = std::min(std::min(dot(pq0, pq0), dot(pq1, pq1)), dot(pq2, pq2));

        return d * (s * (v0.x * e2.y - v0.y * e2.x) > 0.0f ? 1.0f : -1.0f);
    }

    void draw(material_t* buf, int scr_w, int scr_h) override {
        int x_start = static_cast<int>(std::min({p0.x, p1.x, p2.x}));
        x_start = x_start < 0 ? 0 : x_start;
        int x_end = static_cast<int>(std::max({p0.x, p1.x, p2.x}));
        x_end = x_end > scr_w ? scr_w : x_end;
        int y_start = static_cast<int>(std::min({p0.y, p1.y, p2.y}));
        y_start = y_start < 0 ? 0 : y_start;
        int y_end = static_cast<int>(std::max({p0.y, p1.y, p2.y}));
        y_end = y_end > scr_h ? scr_h : y_end;

        for (int x = x_start; x < x_end; ++x) {
            for (int y = y_start; y < y_end; ++y) {
                if (this->sdf(vec2(static_cast<float>(x), static_cast<float>(y))) <= 0) {
                    *(buf + x * scr_h + y) = this->material;
                }
            }
        }
    }
};

class Line : public Object {
public:
    vec2 start, end;
    float thickness;

    Line(vec2 _start, vec2 _end, float _thickness, struct material_t _material)
        : Object(LINE, (_start + _end) / 2.0f, _material), start(_start), end(_end), thickness(_thickness) {}

    float sdf(vec2 point) override {
        vec2 pa = point - start;
        vec2 ba = end - start;
        float h = std::max(0.0f, std::min(1.0f, dot(pa, ba) / dot(ba, ba)));
        return (pa - ba * h).length() - thickness;
    }

    void draw(material_t* buf, int scr_w, int scr_h) override {
        int x_start = static_cast<int>(std::min(start.x, end.x) - thickness);
        x_start = x_start < 0 ? 0 : x_start;
        int x_end = static_cast<int>(std::max(start.x, end.x) + thickness);
        x_end = x_end > scr_w ? scr_w : x_end;
        int y_start = static_cast<int>(std::min(start.y, end.y) - thickness);
        y_start = y_start < 0 ? 0 : y_start;
        int y_end = static_cast<int>(std::max(start.y, end.y) + thickness);
        y_end = y_end > scr_h ? scr_h : y_end;

        for (int x = x_start; x < x_end; ++x) {
            for (int y = y_start; y < y_end; ++y) {
                if (this->sdf(vec2(static_cast<float>(x), static_cast<float>(y))) <= 0) {
                    *(buf + x * scr_h + y) = this->material;
                }
            }
        }
    }
};



SDL_FColor b_intrp(vec2 point, vec2 p0, vec2 p1, vec2 p2, vec2 p3,
                          SDL_FColor c0, SDL_FColor c1, SDL_FColor c2, SDL_FColor c3) {

    float tx = (point.x - p0.x) / (p1.x - p0.x);
    float ty = (point.y - p0.y) / (p2.y - p0.y);

    tx = std::max(0.0f, std::min(1.0f, tx));
    ty = std::max(0.0f, std::min(1.0f, ty));

    auto lerp_color = [](SDL_FColor a, SDL_FColor b, float t) {
        return SDL_FColor{
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        };
    };

    SDL_FColor top_interp = lerp_color(c0, c1, tx);
    SDL_FColor bottom_interp = lerp_color(c2, c3, tx);
    //SDL_FColor final =lerp_color(top_interp, bottom_interp, ty);
    //printf("someone got(%3f, %3f, %3f, %3f)\n", final.r, final.g, final.b, final.a);
    return lerp_color(top_interp, bottom_interp, ty);
}

#endif
