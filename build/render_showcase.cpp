
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct ParticleRow {
    int id;
    int gid;
    int comp;
    double x, y, z;
    double mass;
};

struct Snapshot {
    double time;
    std::vector<ParticleRow> particles;
    double c1x, c1y, c1z;
    double c2x, c2y, c2z;
    double sep;
};

static void compute_core(const std::vector<ParticleRow>& pts, int target_gid, double& cx, double& cy, double& cz) {
    bool has_bh = false;
    double bhx = 0, bhy = 0, bhz = 0, bhm = 0;
    for (const auto& p : pts) {
        if (p.gid == target_gid && p.comp == 3) {
            has_bh = true;
            bhx = p.x; bhy = p.y; bhz = p.z; bhm = p.mass;
            break;
        }
    }
    double w_sum = 0.0, sx = 0.0, sy = 0.0, sz = 0.0;
    if (has_bh) {
        w_sum += bhm;
        sx += bhx * bhm; sy += bhy * bhm; sz += bhz * bhm;
        for (const auto& p : pts) {
            if (p.gid == target_gid && p.comp == 1) {
                double dx = p.x - bhx, dy = p.y - bhy, dz = p.z - bhz;
                if (dx*dx + dy*dy + dz*dz <= 2.25) {
                    w_sum += p.mass;
                    sx += p.x * p.mass; sy += p.y * p.mass; sz += p.z * p.mass;
                }
            }
        }
        cx = sx / w_sum; cy = sy / w_sum; cz = sz / w_sum;
        return;
    }
    for (const auto& p : pts) {
        if (p.gid == target_gid) {
            w_sum += p.mass;
            sx += p.x * p.mass; sy += p.y * p.mass; sz += p.z * p.mass;
        }
    }
    if (w_sum > 0) { cx = sx / w_sum; cy = sy / w_sum; cz = sz / w_sum; }
    else { cx = cy = cz = 0; }
}

// Note: In CGBitmapContext, we flip Y so (0,0) is top-left in screen coordinates (width=1600, height=900).
static void draw_text(CGContextRef ctx, const std::string& text, double x, double y,
                      double font_size, bool bold, double r, double g, double b, int align = 0) {
    CFStringRef font_name = bold ? CFSTR("HelveticaNeue-Bold") : CFSTR("HelveticaNeue-Medium");
    CTFontRef font = CTFontCreateWithName(font_name, font_size, nullptr);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGFloat comps[4] = {r, g, b, 1.0};
    CGColorRef color = CGColorCreate(rgb, comps);

    CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    CFTypeRef values[] = { font, color };
    CFDictionaryRef attrs = CFDictionaryCreate(
        kCFAllocatorDefault, (const void**)&keys, (const void**)&values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFStringRef cf_text = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
    CFAttributedStringRef attr_str = CFAttributedStringCreate(kCFAllocatorDefault, cf_text, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(attr_str);

    CGFloat ascent = 0, descent = 0, leading = 0;
    double width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    double draw_x = x;
    if (align == 1) draw_x = x - 0.5 * width;
    else if (align == 2) draw_x = x - width;

    CGContextSaveGState(ctx);
    // Flip text matrix so text renders upright in top-left origin coordinate space
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));
    CGContextSetTextPosition(ctx, draw_x, y);
    CTLineDraw(line, ctx);
    CGContextRestoreGState(ctx);

    CFRelease(line);
    CFRelease(attr_str);
    CFRelease(cf_text);
    CFRelease(attrs);
    CGColorRelease(color);
    CGColorSpaceRelease(rgb);
    CFRelease(font);
}

static void save_png(CGContextRef ctx, const std::string& path) {
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CFStringRef cf_path = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cf_path, kCFURLPOSIXPathStyle, false);
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    if (dest) {
        CGImageDestinationAddImage(dest, image, nullptr);
        CGImageDestinationFinalize(dest);
        CFRelease(dest);
    }
    CFRelease(url);
    CFRelease(cf_path);
    CGImageRelease(image);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: render_showcase <csv_path> <frames_dir>\n";
        return 1;
    }
    std::string csv_path = argv[1];
    std::string frames_dir = argv[2];

    std::ifstream in(csv_path);
    if (!in) {
        std::cerr << "Could not open " << csv_path << "\n";
        return 1;
    }

    std::string header_line;
    std::getline(in, header_line);
    bool has_comp = (header_line.find("component") != std::string::npos);

    std::map<double, std::vector<ParticleRow>> frame_map;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> cols;
        while (std::getline(ss, item, ',')) cols.push_back(item);
        if (cols.size() < 9) continue;

        double t = std::stod(cols[0]);
        ParticleRow row{};
        row.id = std::stoi(cols[1]);
        row.gid = std::stoi(cols[2]);
        if (has_comp) {
            row.comp = std::stoi(cols[3]);
            row.x = std::stod(cols[4]);
            row.y = std::stod(cols[5]);
            row.z = std::stod(cols[6]);
            row.mass = std::stod(cols[10]);
        } else {
            row.comp = 0;
            row.x = std::stod(cols[3]);
            row.y = std::stod(cols[4]);
            row.z = std::stod(cols[5]);
            row.mass = std::stod(cols[9]);
        }
        frame_map[t].push_back(row);
    }

    std::vector<Snapshot> snaps;
    snaps.reserve(frame_map.size());
    for (auto& kv : frame_map) {
        Snapshot s;
        s.time = kv.first;
        s.particles = std::move(kv.second);
        compute_core(s.particles, 0, s.c1x, s.c1y, s.c1z);
        compute_core(s.particles, 1, s.c2x, s.c2y, s.c2z);
        double dx = s.c1x - s.c2x, dy = s.c1y - s.c2y, dz = s.c1z - s.c2z;
        s.sep = std::sqrt(dx*dx + dy*dy + dz*dz);
        snaps.push_back(std::move(s));
    }

    const int W = 1600;
    const int H = 900;
    std::vector<uint8_t> buffer(W * H * 4, 0);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        buffer.data(), W, H, 8, W * 4, color_space,
        static_cast<uint32_t>(kCGImageAlphaPremultipliedLast) | static_cast<uint32_t>(kCGBitmapByteOrder32Big));

    // Flip coordinate system so (0,0) is top-left
    CGContextTranslateCTM(ctx, 0, H);
    CGContextScaleCTM(ctx, 1.0, -1.0);

    const double max_time = snaps.empty() ? 24.0 : snaps.back().time;
    const std::size_t total_N = snaps.empty() ? 0 : snaps[0].particles.size();

    for (std::size_t idx = 0; idx < snaps.size(); ++idx) {
        const Snapshot& s = snaps[idx];

        // Background #060914
        CGContextSetRGBFillColor(ctx, 0.024, 0.035, 0.078, 1.0);
        CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

        // Panel rectangles:
        // Left Panel (X-Y Orbital Plane): x=70, y=130, w=720, h=700
        CGRect rect_xy = CGRectMake(70, 130, 720, 700);
        // Top-Right Panel (X-Z Vertical Profile): x=860, y=130, w=680, h=320
        CGRect rect_xz = CGRectMake(860, 130, 680, 320);
        // Bottom-Right Panel (SMBH Core Separation Curve d(t)): x=860, y=510, w=680, h=320
        CGRect rect_sep = CGRectMake(860, 510, 680, 320);

        auto draw_panel_box = [&](CGRect r) {
            CGContextSetRGBFillColor(ctx, 0.043, 0.067, 0.141, 1.0);
            CGContextFillRect(ctx, r);
            CGContextSetRGBStrokeColor(ctx, 0.118, 0.161, 0.231, 1.0);
            CGContextSetLineWidth(ctx, 1.5);
            CGContextStrokeRect(ctx, r);
        };
        draw_panel_box(rect_xy);
        draw_panel_box(rect_xz);
        draw_panel_box(rect_sep);

        // Coordinate transforms
        const double lim_xy = 14.0;
        const double lim_z = 7.5;
        auto map_xy = [&](double x, double y) -> CGPoint {
            double px = rect_xy.origin.x + ((x + lim_xy) / (2.0 * lim_xy)) * rect_xy.size.width;
            double py = rect_xy.origin.y + ((lim_xy - y) / (2.0 * lim_xy)) * rect_xy.size.height;
            return CGPointMake(px, py);
        };
        auto map_xz = [&](double x, double z) -> CGPoint {
            double px = rect_xz.origin.x + ((x + lim_xy) / (2.0 * lim_xy)) * rect_xz.size.width;
            double py = rect_xz.origin.y + ((lim_z - z) / (2.0 * lim_z)) * rect_xz.size.height;
            return CGPointMake(px, py);
        };

        // Subtle gridlines on XY and XZ
        CGContextSaveGState(ctx);
        CGContextSetRGBStrokeColor(ctx, 0.118, 0.161, 0.231, 0.55);
        CGContextSetLineWidth(ctx, 1.0);
        for (int g = -10; g <= 10; g += 5) {
            CGPoint p1 = map_xy(g, -lim_xy), p2 = map_xy(g, lim_xy);
            CGContextMoveToPoint(ctx, p1.x, p1.y); CGContextAddLineToPoint(ctx, p2.x, p2.y);
            CGPoint q1 = map_xy(-lim_xy, g), q2 = map_xy(lim_xy, g);
            CGContextMoveToPoint(ctx, q1.x, q1.y); CGContextAddLineToPoint(ctx, q2.x, q2.y);

            CGPoint rx1 = map_xz(g, -lim_z), rx2 = map_xz(g, lim_z);
            CGContextMoveToPoint(ctx, rx1.x, rx1.y); CGContextAddLineToPoint(ctx, rx2.x, rx2.y);

            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", g);
            draw_text(ctx, buf, p1.x, rect_xy.origin.y + rect_xy.size.height + 18, 12, false, 0.45, 0.53, 0.65, 1);
            draw_text(ctx, buf, rect_xy.origin.x - 10, q1.y + 4, 12, false, 0.45, 0.53, 0.65, 2);
            draw_text(ctx, buf, rx1.x, rect_xz.origin.y + rect_xz.size.height + 16, 11, false, 0.45, 0.53, 0.65, 1);
        }
        for (int gz = -5; gz <= 5; gz += 5) {
            CGPoint rz1 = map_xz(-lim_xy, gz), rz2 = map_xz(lim_xy, gz);
            CGContextMoveToPoint(ctx, rz1.x, rz1.y); CGContextAddLineToPoint(ctx, rz2.x, rz2.y);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", gz);
            draw_text(ctx, buf, rect_xz.origin.x - 10, rz1.y + 4, 11, false, 0.45, 0.53, 0.65, 2);
        }
        CGContextStrokePath(ctx);
        CGContextRestoreGState(ctx);

        // Draw Particles in XY (clipped to rect_xy) and XZ (clipped to rect_xz)
        auto draw_particles_in_view = [&](CGRect clip_rect, bool is_xy) {
            CGContextSaveGState(ctx);
            CGContextClipToRect(ctx, clip_rect);
            CGContextSetBlendMode(ctx, kCGBlendModePlusLighter);

            // Pass 1: Live Dark Matter Halo (component == 2) - soft diffuse indigo/violet glow
            for (const auto& p : s.particles) {
                if (p.comp != 2) continue;
                CGPoint pt = is_xy ? map_xy(p.x, p.y) : map_xz(p.x, p.z);
                if (p.gid == 0) CGContextSetRGBFillColor(ctx, 0.28, 0.32, 0.85, 0.14);
                else CGContextSetRGBFillColor(ctx, 0.58, 0.25, 0.88, 0.14);
                CGContextFillEllipseInRect(ctx, CGRectMake(pt.x - 4.5, pt.y - 4.5, 9.0, 9.0));
            }

            // Pass 2: Spiral Stellar Disk (component == 0) - glowing cyan & solar amber
            for (const auto& p : s.particles) {
                if (p.comp != 0) continue;
                CGPoint pt = is_xy ? map_xy(p.x, p.y) : map_xz(p.x, p.z);
                if (p.gid == 0) CGContextSetRGBFillColor(ctx, 0.22, 0.74, 0.97, 0.22);
                else CGContextSetRGBFillColor(ctx, 0.98, 0.57, 0.24, 0.22);
                CGContextFillEllipseInRect(ctx, CGRectMake(pt.x - 3.6, pt.y - 3.6, 7.2, 7.2));

                if (p.gid == 0) CGContextSetRGBFillColor(ctx, 0.35, 0.82, 1.0, 0.82);
                else CGContextSetRGBFillColor(ctx, 1.0, 0.64, 0.30, 0.82);
                CGContextFillEllipseInRect(ctx, CGRectMake(pt.x - 1.7, pt.y - 1.7, 3.4, 3.4));
            }

            // Pass 3: Dense Galactic Bulge (component == 1) - warm golden-white core
            for (const auto& p : s.particles) {
                if (p.comp != 1) continue;
                CGPoint pt = is_xy ? map_xy(p.x, p.y) : map_xz(p.x, p.z);
                CGContextSetRGBFillColor(ctx, 1.0, 0.92, 0.45, 0.78);
                CGContextFillEllipseInRect(ctx, CGRectMake(pt.x - 2.1, pt.y - 2.1, 4.2, 4.2));
            }

            CGContextSetBlendMode(ctx, kCGBlendModeNormal);

            // Pass 4: SMBH Nuclear Core Trajectory Trails
            if (idx > 0) {
                CGFloat dash[2] = {5.0, 4.0};
                CGContextSetLineDash(ctx, 0, dash, 2);
                CGContextSetLineWidth(ctx, 1.8);

                CGContextSetRGBStrokeColor(ctx, 0.22, 0.74, 0.97, 0.75);
                CGContextBeginPath(ctx);
                for (std::size_t k = 0; k <= idx; ++k) {
                    CGPoint pt = is_xy ? map_xy(snaps[k].c1x, snaps[k].c1y) : map_xz(snaps[k].c1x, snaps[k].c1z);
                    if (k == 0) CGContextMoveToPoint(ctx, pt.x, pt.y);
                    else CGContextAddLineToPoint(ctx, pt.x, pt.y);
                }
                CGContextStrokePath(ctx);

                CGContextSetRGBStrokeColor(ctx, 0.98, 0.57, 0.24, 0.75);
                CGContextBeginPath(ctx);
                for (std::size_t k = 0; k <= idx; ++k) {
                    CGPoint pt = is_xy ? map_xy(snaps[k].c2x, snaps[k].c2y) : map_xz(snaps[k].c2x, snaps[k].c2z);
                    if (k == 0) CGContextMoveToPoint(ctx, pt.x, pt.y);
                    else CGContextAddLineToPoint(ctx, pt.x, pt.y);
                }
                CGContextStrokePath(ctx);
                CGContextSetLineDash(ctx, 0, nullptr, 0);
            }

            // Pass 5: Central Supermassive Black Hole (SMBH) Markers
            CGPoint c1_pt = is_xy ? map_xy(s.c1x, s.c1y) : map_xz(s.c1x, s.c1z);
            CGPoint c2_pt = is_xy ? map_xy(s.c2x, s.c2y) : map_xz(s.c2x, s.c2z);
            for (CGPoint cpt : {c1_pt, c2_pt}) {
                CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 0.95);
                CGContextFillEllipseInRect(ctx, CGRectMake(cpt.x - 4.0, cpt.y - 4.0, 8.0, 8.0));
                CGContextSetRGBStrokeColor(ctx, 0.05, 0.08, 0.18, 1.0);
                CGContextSetLineWidth(ctx, 1.5);
                CGContextStrokeEllipseInRect(ctx, CGRectMake(cpt.x - 4.0, cpt.y - 4.0, 8.0, 8.0));
            }

            CGContextRestoreGState(ctx);
        };

        draw_particles_in_view(rect_xy, true);
        draw_particles_in_view(rect_xz, false);

        // --- Panel 3: Live Dynamical Friction Orbital Decay Curve d(t) ---
        const double max_sep = 12.5;
        auto map_sep = [&](double t_val, double d_val) -> CGPoint {
            double px = rect_sep.origin.x + 15 + ((t_val / std::max(1.0, max_time)) * (rect_sep.size.width - 30));
            double py = rect_sep.origin.y + 15 + ((1.0 - std::min(max_sep, d_val) / max_sep) * (rect_sep.size.height - 30));
            return CGPointMake(px, py);
        };

        CGContextSaveGState(ctx);
        CGContextSetRGBStrokeColor(ctx, 0.118, 0.161, 0.231, 0.55);
        CGContextSetLineWidth(ctx, 1.0);
        for (int d_tick = 0; d_tick <= 12; d_tick += 3) {
            CGPoint p1 = map_sep(0, d_tick), p2 = map_sep(max_time, d_tick);
            CGContextMoveToPoint(ctx, p1.x, p1.y); CGContextAddLineToPoint(ctx, p2.x, p2.y);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", d_tick);
            draw_text(ctx, buf, rect_sep.origin.x - 8, p1.y + 4, 11, false, 0.45, 0.53, 0.65, 2);
        }
        for (int t_tick = 0; t_tick <= static_cast<int>(max_time); t_tick += 4) {
            CGPoint p1 = map_sep(t_tick, 0), p2 = map_sep(t_tick, max_sep);
            CGContextMoveToPoint(ctx, p1.x, p1.y); CGContextAddLineToPoint(ctx, p2.x, p2.y);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", t_tick);
            draw_text(ctx, buf, p1.x, rect_sep.origin.y + rect_sep.size.height + 16, 11, false, 0.45, 0.53, 0.65, 1);
        }
        CGContextStrokePath(ctx);

        // Full reference curve (dashed slate)
        CGFloat dash[2] = {4.0, 4.0};
        CGContextSetLineDash(ctx, 0, dash, 2);
        CGContextSetRGBStrokeColor(ctx, 0.28, 0.35, 0.48, 0.85);
        CGContextSetLineWidth(ctx, 1.8);
        CGContextBeginPath(ctx);
        for (std::size_t k = 0; k < snaps.size(); ++k) {
            CGPoint pt = map_sep(snaps[k].time, snaps[k].sep);
            if (k == 0) CGContextMoveToPoint(ctx, pt.x, pt.y);
            else CGContextAddLineToPoint(ctx, pt.x, pt.y);
        }
        CGContextStrokePath(ctx);
        CGContextSetLineDash(ctx, 0, nullptr, 0);

        // Active curve up to current frame (glowing cyan)
        CGContextSetRGBStrokeColor(ctx, 0.22, 0.74, 0.97, 1.0);
        CGContextSetLineWidth(ctx, 2.8);
        CGContextBeginPath(ctx);
        for (std::size_t k = 0; k <= idx; ++k) {
            CGPoint pt = map_sep(snaps[k].time, snaps[k].sep);
            if (k == 0) CGContextMoveToPoint(ctx, pt.x, pt.y);
            else CGContextAddLineToPoint(ctx, pt.x, pt.y);
        }
        CGContextStrokePath(ctx);

        // Current point marker
        CGPoint cur_pt = map_sep(s.time, s.sep);
        CGContextSetRGBFillColor(ctx, 0.99, 0.88, 0.28, 1.0);
        CGContextFillEllipseInRect(ctx, CGRectMake(cur_pt.x - 5.0, cur_pt.y - 5.0, 10.0, 10.0));
        CGContextRestoreGState(ctx);

        // Annotations inside Separation Plot
        draw_text(ctx, "1st Pericenter (t=9.6, d=0.69)", map_sep(9.6, 0.69).x - 10, map_sep(9.6, 0.69).y + 20, 11.5, false, 0.65, 0.75, 0.88, 1);
        draw_text(ctx, "DM Halo Braking Apocenter (t=14.4)", map_sep(14.4, 3.58).x, map_sep(14.4, 3.58).y - 12, 11.5, false, 0.65, 0.75, 0.88, 1);
        draw_text(ctx, "Core Coalescence (d->0.09)", map_sep(21.6, 0.09).x - 15, map_sep(21.6, 0.09).y - 12, 11.5, true, 0.99, 0.88, 0.28, 1);

        // Legend box inside XY Panel
        CGRect leg = CGRectMake(rect_xy.origin.x + 14, rect_xy.origin.y + 14, 245, 108);
        CGContextSetRGBFillColor(ctx, 0.059, 0.090, 0.165, 0.88);
        CGContextFillRect(ctx, leg);
        CGContextSetRGBStrokeColor(ctx, 0.20, 0.25, 0.33, 1.0);
        CGContextStrokeRect(ctx, leg);

        auto draw_legend_row = [&](double y_off, double r, double g, double b, const char* label) {
            CGContextSetRGBFillColor(ctx, r, g, b, 1.0);
            CGContextFillEllipseInRect(ctx, CGRectMake(leg.origin.x + 12, leg.origin.y + y_off - 8, 9, 9));
            draw_text(ctx, label, leg.origin.x + 28, leg.origin.y + y_off, 12, false, 0.82, 0.86, 0.92, 0);
        };
        draw_legend_row(22, 0.22, 0.74, 0.97, "Galaxy 1 Spiral Disk");
        draw_legend_row(44, 0.98, 0.57, 0.24, "Galaxy 2 Spiral Disk");
        draw_legend_row(66, 1.00, 0.92, 0.45, "Galactic Bulges + SMBH Core");
        draw_legend_row(88, 0.45, 0.35, 0.92, "Live Dark Matter Halos");

        // Titles & Labels
        std::string stage_desc;
        if (s.time < 7.5) {
            stage_desc = "STAGE 1: Flat-Curve Spiral Approach & Dark Matter Halo Overlap";
        } else if (s.time < 11.5) {
            stage_desc = "STAGE 2: First Pericenter Passage (t = 9.6) — Sweeping Tidal Tails & Stellar Bridge";
        } else if (s.time < 17.5) {
            stage_desc = "STAGE 3: Dynamical Friction Braking & Apocenter Turnaround (t = 14.4)";
        } else {
            stage_desc = "STAGE 4: Secondary Infall & Galactic Core Coalescence (d -> 0.09)";
        }

        draw_text(ctx, "Barnes-Hut N-Body Simulator: Multi-Component Spiral Galaxy Merger (DM Halo + SMBH)",
                  W * 0.5, 36, 22, true, 0.97, 0.98, 0.99, 1);
        draw_text(ctx, stage_desc, W * 0.5, 66, 16, true, 0.22, 0.74, 0.97, 1);

        char stat_buf[256];
        std::snprintf(stat_buf, sizeof(stat_buf),
                      "Time t = %05.2f   |   SMBH Core Separation = %05.2f   |   N = %zu Particles (Disk + Bulge + DM Halo + SMBH)   |   Barnes-Hut theta = 0.5",
                      s.time, s.sep, total_N);
        draw_text(ctx, stat_buf, W * 0.5, 92, 13.5, false, 0.62, 0.68, 0.78, 1);

        draw_text(ctx, "Orbital Collision Plane (X-Y)", rect_xy.origin.x + rect_xy.size.width * 0.5, 120, 15.5, true, 0.89, 0.91, 0.94, 1);
        draw_text(ctx, "X (N-body length units)", rect_xy.origin.x + rect_xy.size.width * 0.5, rect_xy.origin.y + rect_xy.size.height + 42, 13, false, 0.58, 0.64, 0.72, 1);
        draw_text(ctx, "Y", rect_xy.origin.x - 34, rect_xy.origin.y + rect_xy.size.height * 0.5, 13, true, 0.58, 0.64, 0.72, 1);

        draw_text(ctx, "3D Inclined Vertical Profile (X-Z)", rect_xz.origin.x + rect_xz.size.width * 0.5, 120, 15.0, true, 0.89, 0.91, 0.94, 1);
        draw_text(ctx, "Z", rect_xz.origin.x - 30, rect_xz.origin.y + rect_xz.size.height * 0.5, 13, true, 0.58, 0.64, 0.72, 1);

        draw_text(ctx, "Dynamical Friction Orbital Decay — SMBH Core Separation d(t)", rect_sep.origin.x + rect_sep.size.width * 0.5, 498, 15.0, true, 0.89, 0.91, 0.94, 1);
        draw_text(ctx, "Simulation Time t (N-body units)", rect_sep.origin.x + rect_sep.size.width * 0.5, rect_sep.origin.y + rect_sep.size.height + 40, 13, false, 0.58, 0.64, 0.72, 1);
        draw_text(ctx, "d(t)", rect_sep.origin.x - 34, rect_sep.origin.y + rect_sep.size.height * 0.5, 13, true, 0.58, 0.64, 0.72, 1);

        char frame_name[512];
        std::snprintf(frame_name, sizeof(frame_name), "%s/frame_%04zu.png", frames_dir.c_str(), idx);
        save_png(ctx, frame_name);
    }

    CGContextRelease(ctx);
    CGColorSpaceRelease(color_space);
    std::cout << "Rendered " << snaps.size() << " high-DPI frames via macOS CoreGraphics.\n";
    return 0;
}
