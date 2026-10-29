/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#include "SpaceArt.h"

namespace brewui
{
namespace
{
    using P = juce::Point<float>;
    using C = juce::Colour;
    using brew::Scene;

    /** A screen-space quadrilateral: a = top-left, b = top-right, c = bottom-right, d = bottom-left. */
    struct Quad
    {
        P a, b, c, d;
        P at (float u, float v) const
        {
            const P top = a + (b - a) * u;
            const P bot = d + (c - d) * u;
            return top + (bot - top) * v;
        }
        juce::Path path() const
        {
            juce::Path p;
            p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath();
            return p;
        }
        Quad sub (float u0, float v0, float u1, float v1) const
        {
            return { at (u0, v0), at (u1, v0), at (u1, v1), at (u0, v1) };
        }
    };

    struct Room
    {
        juce::Rectangle<float> area, back;
        Quad floor, ceil, left, right, backWall;
        float backScale = 0.4f;

        /** World depth fraction (0 = front, 1 = back wall) -> screen fraction with perspective. */
        float persp (float t) const
        {
            const float k = 1.0f / backScale - 1.0f;
            return (1.0f - 1.0f / (1.0f + t * k)) / (1.0f - backScale);
        }
        // Side walls: depth t (0 front .. 1 back), height v (0 top .. 1 bottom)
        P leftAt  (float t, float v) const { return left.at (persp (t), v); }
        P rightAt (float t, float v) const { return right.at (1.0f - persp (t), v); }
        // Floor / ceiling: across u (0 left .. 1 right), depth t (0 front .. 1 back)
        P floorAt (float u, float t) const { return floor.at (u, 1.0f - persp (t)); }
        P ceilAt  (float u, float t) const { return ceil.at (u, persp (t)); }

        Quad leftPanel (float t0, float t1, float v0, float v1) const
        { return { leftAt (t0, v0), leftAt (t1, v0), leftAt (t1, v1), leftAt (t0, v1) }; }
        Quad rightPanel (float t0, float t1, float v0, float v1) const
        { return { rightAt (t1, v0), rightAt (t0, v0), rightAt (t0, v1), rightAt (t1, v1) }; }
        Quad floorPanel (float u0, float u1, float t0, float t1) const
        { return { floorAt (u0, t1), floorAt (u1, t1), floorAt (u1, t0), floorAt (u0, t0) }; }
    };

    Room makeRoom (juce::Rectangle<float> area, const brew::SpaceInfo& info, float sizeScale)
    {
        Room r;
        r.area = area;
        const float ratio = info.depthM / juce::jmax (0.5f, info.widthM);
        r.backScale = juce::jlimit (0.14f, 0.58f, 0.9f / (0.6f + ratio) / std::sqrt (juce::jmax (0.5f, sizeScale)));
        const float hRatio = info.heightM > 0.0f ? juce::jlimit (0.55f, 1.7f, info.heightM / info.widthM * 1.4f) : 0.8f;

        const float bw = area.getWidth() * r.backScale;
        const float bh = juce::jmin (area.getHeight() * 0.86f, area.getHeight() * r.backScale * hRatio * 1.15f);
        const float vx = area.getCentreX();
        const float vy = area.getY() + area.getHeight() * 0.44f;
        r.back = juce::Rectangle<float> (bw, bh).withCentre ({ vx, vy });

        const auto A = area;  const auto B = r.back;
        r.floor    = { B.getBottomLeft(), B.getBottomRight(), A.getBottomRight(), A.getBottomLeft() };
        r.ceil     = { A.getTopLeft(), A.getTopRight(), B.getTopRight(), B.getTopLeft() };
        r.left     = { A.getTopLeft(), B.getTopLeft(), B.getBottomLeft(), A.getBottomLeft() };
        r.right    = { B.getTopRight(), A.getTopRight(), A.getBottomRight(), B.getBottomRight() };
        r.backWall = { B.getTopLeft(), B.getTopRight(), B.getBottomRight(), B.getBottomLeft() };
        return r;
    }

    void fillQuad (juce::Graphics& g, const Quad& q, C c) { g.setColour (c); g.fillPath (q.path()); }

    void fillQuadGrad (juce::Graphics& g, const Quad& q, C c1, P p1, C c2, P p2)
    {
        g.setGradientFill (juce::ColourGradient (c1, p1, c2, p2, false));
        g.fillPath (q.path());
    }

    void line (juce::Graphics& g, P a, P b, C c, float w) { g.setColour (c); g.drawLine ({ a, b }, w); }

    struct Palette { C floor, wall, ceil, back, light; };

    Palette paletteFor (Scene s)
    {
        switch (s)
        {
            case Scene::Booth:     return { C (0xff3a3634), C (0xff4a4440), C (0xff2c2927), C (0xff554d47), C (0xffffd9a0) };
            case Scene::Living:    return { C (0xff6b4a2e), C (0xffc9b28e), C (0xffe0d2b8), C (0xffd8c39e), C (0xffffe4b5) };
            case Scene::Room:      return { C (0xff7a6a58), C (0xffcfc6b6), C (0xffe6e0d4), C (0xffd9d0c0), C (0xfffff1d6) };
            case Scene::Studio:    return { C (0xff6e4c2f), C (0xff8a7058), C (0xff3d332b), C (0xff9a7d60), C (0xffffcf8a) };
            case Scene::Office:    return { C (0xff5d6066), C (0xffd5d3cc), C (0xffeceae4), C (0xffcfccc3), C (0xfff4f7ff) };
            case Scene::Bathroom:  return { C (0xffb9c2c4), C (0xffdfe8e8), C (0xfff2f4f2), C (0xffd6e2e2), C (0xffffffff) };
            case Scene::Corridor:  return { C (0xff6a6458), C (0xffc4baa4), C (0xffd8d0bc), C (0xffb8ad94), C (0xfffff0c8) };
            case Scene::Chamber:   return { C (0xff5a3a22), C (0xffa0703f), C (0xff6e4a2a), C (0xffb07c48), C (0xffffc878) };
            case Scene::Stairwell: return { C (0xff7d7b76), C (0xffa9a69f), C (0xff8f8c86), C (0xffb5b2ab), C (0xffeef0ff) };
            case Scene::Hall:      return { C (0xff7b4f2a), C (0xffb07a42), C (0xff5b3b22), C (0xffc08a4e), C (0xffffd68a) };
            case Scene::Church:    return { C (0xff6d6152), C (0xffb3a286), C (0xff7d705c), C (0xffc4b294), C (0xffffe2a8) };
            case Scene::Cathedral: return { C (0xff5f5446), C (0xffa49276), C (0xff6c604f), C (0xffb8a585), C (0xffffd890) };
            case Scene::Warehouse: return { C (0xff6f6d68), C (0xff8b8478), C (0xff4d4a45), C (0xff978f82), C (0xffdfe8f0) };
            case Scene::Courtyard: return { C (0xff8d7f6c), C (0xffcbb08a), C (0xff8fb4d8), C (0xffd2b892), C (0xffffffff) };
            case Scene::Car:
            case Scene::Plate:     break;
        }
        return { C (0xff555555), C (0xff888888), C (0xff666666), C (0xff999999), C (0xffffffff) };
    }

    // ---- Room shell -----------------------------------------------------------
    void drawShell (juce::Graphics& g, const Room& r, const Palette& p, bool skyCeiling)
    {
        const auto& A = r.area;  const auto& B = r.back;
        if (skyCeiling)
            fillQuadGrad (g, r.ceil, C (0xff6f9fd0), A.getTopLeft(), C (0xffd8e6f0), { A.getX(), B.getY() });
        else
            fillQuadGrad (g, r.ceil, p.ceil.darker (0.35f), A.getTopLeft(), p.ceil, { A.getX(), B.getY() });

        fillQuadGrad (g, r.left,  p.wall.darker (0.25f), A.getTopLeft(),  p.wall, B.getTopLeft());
        fillQuadGrad (g, r.right, p.wall.darker (0.25f), A.getTopRight(), p.wall, B.getTopRight());
        fillQuadGrad (g, r.floor, p.floor.darker (0.2f), A.getBottomLeft(), p.floor.brighter (0.15f), B.getBottomLeft());
        fillQuadGrad (g, r.backWall, p.back.brighter (0.1f), B.getCentre(), p.back.darker (0.1f), B.getTopLeft());

        // soft light falling on the back wall
        g.setGradientFill (juce::ColourGradient (p.light.withAlpha (0.35f), B.getCentreX(), B.getY() + B.getHeight() * 0.3f,
                                                 p.light.withAlpha (0.0f), B.getCentreX() + B.getWidth() * 0.7f, B.getBottom(), true));
        g.fillPath (r.backWall.path());

        // edges
        const C edge = C (0x30000000);
        for (auto* q : { &r.left, &r.right })
            { g.setColour (edge); g.strokePath (q->path(), juce::PathStrokeType (1.0f)); }
    }

    // ---- Scene details --------------------------------------------------------
    void archWindow (juce::Graphics& g, P bl, P br, P tl, P tr, float archH, C fill, C frame)
    {
        juce::Path p;
        const P apex = (tl + tr) * 0.5f + P (0.0f, -archH);
        p.startNewSubPath (bl);
        p.lineTo (tl);
        p.quadraticTo (tl + (apex - tl) * 0.2f + P (0.0f, -archH * 0.8f), apex);
        p.quadraticTo (tr + (apex - tr) * 0.2f + P (0.0f, -archH * 0.8f), tr);
        p.lineTo (br);
        p.closeSubPath();
        g.setColour (fill); g.fillPath (p);
        g.setColour (frame); g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    void drawRows (juce::Graphics& g, const Room& r, int rows, float t0, float t1, float u0, float u1, C c, float curve)
    {
        for (int i = 0; i < rows; ++i)
        {
            const float t = t0 + (t1 - t0) * (float) i / (float) juce::jmax (1, rows - 1);
            juce::Path p;
            for (int k = 0; k <= 20; ++k)
            {
                const float u = u0 + (u1 - u0) * (float) k / 20.0f;
                const float bend = curve * std::sin (juce::MathConstants<float>::pi * (float) k / 20.0f);
                const P pt = r.floorAt (u, juce::jlimit (0.0f, 1.0f, t + bend));
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            const float w = juce::jmap (1.0f - t, 1.0f, 4.0f);
            g.setColour (c.darker (0.4f)); g.strokePath (p, juce::PathStrokeType (w + 1.5f));
            g.setColour (c);               g.strokePath (p, juce::PathStrokeType (w));
        }
    }

    void drawHall (juce::Graphics& g, const Room& r, const Palette& p)
    {
        // stage
        fillQuad (g, r.floorPanel (0.08f, 0.92f, 0.8f, 1.0f), p.floor.brighter (0.45f));
        // organ pipes on back wall
        const auto& B = r.back;
        for (int i = 0; i < 17; ++i)
        {
            const float x = B.getX() + B.getWidth() * (0.25f + 0.5f * (float) i / 16.0f);
            const float h = B.getHeight() * (0.18f + 0.12f * std::abs (std::sin ((float) i * 0.7f)));
            g.setColour (C (0xffd9c7a0).withAlpha (0.85f));
            g.fillRoundedRectangle (x - 1.5f, B.getY() + B.getHeight() * 0.06f, 3.0f, h, 1.5f);
        }
        // balconies on side walls
        for (float v : { 0.30f, 0.52f })
        {
            fillQuad (g, r.leftPanel (0.05f, 0.85f, v, v + 0.06f), p.wall.darker (0.5f));
            fillQuad (g, r.rightPanel (0.05f, 0.85f, v, v + 0.06f), p.wall.darker (0.5f));
            line (g, r.leftAt (0.05f, v), r.leftAt (0.85f, v), p.light.withAlpha (0.5f), 1.2f);
            line (g, r.rightAt (0.05f, v), r.rightAt (0.85f, v), p.light.withAlpha (0.5f), 1.2f);
        }
        drawRows (g, r, 11, 0.02f, 0.72f, 0.05f, 0.95f, C (0xff7a2f22), -0.03f);
        // ceiling lights
        for (int i = 0; i < 4; ++i)
            for (int k = 0; k < 5; ++k)
            {
                const P c = r.ceilAt (0.15f + 0.175f * (float) k, 0.15f + 0.2f * (float) i);
                g.setColour (p.light.withAlpha (0.8f));
                g.fillEllipse (c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
            }
    }

    void drawChurch (juce::Graphics& g, const Room& r, const Palette& p, bool cathedral)
    {
        const int bays = cathedral ? 7 : 5;
        for (int i = 0; i < bays; ++i)
        {
            const float t0 = (float) i / (float) bays + 0.03f, t1 = (float) (i + 1) / (float) bays - 0.05f;
            const float v0 = cathedral ? 0.12f : 0.22f, v1 = cathedral ? 0.55f : 0.6f;
            const C glass = C (0xfff0c878).withAlpha (0.85f);
            {
                auto q = r.leftPanel (t0, t1, v0, v1);
                archWindow (g, q.d, q.c, q.a, q.b, (q.d.y - q.a.y) * 0.25f, glass, p.wall.darker (0.5f));
            }
            {
                auto q = r.rightPanel (t0, t1, v0, v1);
                archWindow (g, q.d, q.c, q.a, q.b, (q.d.y - q.a.y) * 0.25f, glass, p.wall.darker (0.5f));
            }
            // columns
            const float tc = (float) i / (float) bays;
            line (g, r.leftAt (tc, 0.0f), r.leftAt (tc, 1.0f), p.wall.darker (0.35f), juce::jmap (1.0f - tc, 1.5f, 6.0f));
            line (g, r.rightAt (tc, 0.0f), r.rightAt (tc, 1.0f), p.wall.darker (0.35f), juce::jmap (1.0f - tc, 1.5f, 6.0f));
            if (cathedral)   // vault ribs
            {
                juce::Path rib;
                const P l = r.ceilAt (0.0f, tc), rr = r.ceilAt (1.0f, tc), m = r.ceilAt (0.5f, tc);
                rib.startNewSubPath (l);
                rib.quadraticTo (P (m.x, m.y - (rr.x - l.x) * 0.05f), rr);
                g.setColour (p.ceil.darker (0.5f)); g.strokePath (rib, juce::PathStrokeType (1.5f));
            }
        }
        // rose window
        const auto& B = r.back;
        const P c (B.getCentreX(), B.getY() + B.getHeight() * 0.3f);
        const float rad = B.getWidth() * 0.18f;
        g.setColour (C (0xfff2c26a)); g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (p.back.darker (0.5f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.5f);
        for (int k = 0; k < 8; ++k)
            g.drawLine ({ c, c.getPointOnCircumference (rad, (float) k * juce::MathConstants<float>::pi / 4.0f) }, 1.0f);
        // altar glow + pews
        fillQuad (g, r.floorPanel (0.3f, 0.7f, 0.86f, 1.0f), p.floor.brighter (0.4f));
        g.setColour (C (0xff5a3a24));
        for (int i = 0; i < 9; ++i)
        {
            const float t = 0.05f + 0.08f * (float) i;
            for (auto [u0, u1] : { std::pair<float, float> { 0.08f, 0.44f }, std::pair<float, float> { 0.56f, 0.92f } })
                fillQuad (g, r.floorPanel (u0, u1, t, t + 0.025f), C (0xff4e321f));
        }
    }

    void drawWarehouse (juce::Graphics& g, const Room& r, const Palette& p)
    {
        for (int i = 0; i < 9; ++i)
        {
            const float t = (float) i / 8.0f;
            line (g, r.ceilAt (0.0f, t), r.ceilAt (1.0f, t), C (0xff2e2b27), juce::jmap (1.0f - t, 1.0f, 4.0f));
        }
        for (float u : { 0.33f, 0.66f })
            line (g, r.ceilAt (u, 0.0f), r.ceilAt (u, 1.0f), C (0xff2e2b27), 2.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float t0 = (float) i / 8.0f + 0.02f, t1 = (float) (i + 1) / 8.0f - 0.02f;
            fillQuad (g, r.leftPanel (t0, t1, 0.08f, 0.24f), C (0xffcfdde8));
            fillQuad (g, r.rightPanel (t0, t1, 0.08f, 0.24f), C (0xffcfdde8));
        }
        for (auto [u, t] : { std::pair<float, float> { 0.15f, 0.55f }, { 0.75f, 0.35f }, { 0.25f, 0.82f } })
            fillQuad (g, r.floorPanel (u, u + 0.1f, t, t + 0.05f), C (0xff9a7a4f));
        juce::ignoreUnused (p);
    }

    void drawCorridor (juce::Graphics& g, const Room& r, const Palette& p)
    {
        for (int i = 0; i < 6; ++i)
        {
            const float t0 = 0.08f + (float) i * 0.15f;
            fillQuad (g, r.leftPanel (t0, t0 + 0.05f, 0.25f, 1.0f), C (0xff7a5634));
            fillQuad (g, r.rightPanel (t0 + 0.07f, t0 + 0.12f, 0.25f, 1.0f), C (0xff7a5634));
            const P c = r.ceilAt (0.5f, t0 + 0.03f);
            g.setColour (p.light);
            const float w = juce::jmap (1.0f - t0, 4.0f, 22.0f);
            g.fillRoundedRectangle (c.x - w * 0.5f, c.y - w * 0.12f, w, w * 0.24f, 2.0f);
        }
        for (int i = 0; i < 14; ++i)
            line (g, r.floorAt (0.0f, (float) i / 14.0f), r.floorAt (1.0f, (float) i / 14.0f), p.floor.darker (0.25f), 1.0f);
    }

    void drawStairwell (juce::Graphics& g, const Room& r, const Palette& p)
    {
        const auto& B = r.back;
        const int steps = 10;
        for (int flight = 0; flight < 3; ++flight)
        {
            const float yBase = B.getBottom() - (float) flight * B.getHeight() * 0.32f;
            const bool ltr = flight % 2 == 0;
            for (int s = 0; s < steps; ++s)
            {
                const float f = (float) s / (float) steps;
                const float x = ltr ? B.getX() + B.getWidth() * (0.05f + 0.9f * f) : B.getRight() - B.getWidth() * (0.05f + 0.9f * f);
                const float y = yBase - B.getHeight() * 0.3f * f;
                g.setColour (p.back.darker (0.15f + 0.02f * (float) s));
                g.fillRect (ltr ? x : x - B.getWidth() * 0.09f, y - 3.0f, B.getWidth() * 0.09f, 3.0f + B.getHeight() * 0.03f);
            }
            g.setColour (C (0xff3a3733));
            const float y0 = yBase - B.getHeight() * 0.12f;
            g.drawLine (ltr ? B.getX() + 4 : B.getRight() - 4, y0, ltr ? B.getRight() - 4 : B.getX() + 4, y0 - B.getHeight() * 0.3f, 1.5f);
        }
        for (int i = 0; i < 5; ++i)
            line (g, r.leftAt (0.1f + 0.18f * (float) i, 0.55f), r.leftAt (0.1f + 0.18f * (float) i, 0.15f), C (0xff4a4743), 1.0f);
    }

    void drawChamber (juce::Graphics& g, const Room& r, const Palette& p)
    {
        for (int i = 0; i < 18; ++i)
        {
            const float t = (float) i / 18.0f;
            line (g, r.leftAt (t, 0.0f), r.leftAt (t, 1.0f), p.wall.darker (0.3f), 1.0f);
            line (g, r.rightAt (t, 0.0f), r.rightAt (t, 1.0f), p.wall.darker (0.3f), 1.0f);
        }
        const auto& B = r.back;
        for (int i = 1; i < 12; ++i)
        {
            const float x = B.getX() + B.getWidth() * (float) i / 12.0f;
            g.setColour (p.back.darker (0.3f)); g.drawVerticalLine ((int) x, B.getY(), B.getBottom());
        }
        // speaker + mic in classic echo-chamber setup are drawn by the caller
    }

    void drawStudio (juce::Graphics& g, const Room& r, const Palette& p)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float t0 = 0.1f + 0.22f * (float) i;
            fillQuad (g, r.leftPanel (t0, t0 + 0.14f, 0.2f, 0.6f), C (0xff4a3f36));
            fillQuad (g, r.rightPanel (t0, t0 + 0.14f, 0.2f, 0.6f), C (0xff4a3f36));
        }
        const auto& B = r.back;
        juce::Random rng (11);
        for (int i = 0; i < 10; ++i)
            for (int k = 0; k < 6; ++k)
            {
                const float cw = B.getWidth() * 0.6f / 10.0f, ch = B.getHeight() * 0.4f / 6.0f;
                g.setColour (p.back.darker (0.1f + 0.35f * rng.nextFloat()));
                g.fillRect (B.getX() + B.getWidth() * 0.2f + cw * (float) i, B.getY() + B.getHeight() * 0.15f + ch * (float) k, cw - 1, ch - 1);
            }
        fillQuad (g, r.floorPanel (0.25f, 0.75f, 0.25f, 0.7f), C (0xff6d2a20));
        juce::ignoreUnused (p);
    }

    void drawOffice (juce::Graphics& g, const Room& r, const Palette& p)
    {
        for (int i = 0; i < 4; ++i)
        {
            const float t0 = 0.08f + 0.23f * (float) i;
            fillQuadGrad (g, r.leftPanel (t0, t0 + 0.17f, 0.18f, 0.62f), C (0xffbcd6ee), r.leftAt (t0, 0.2f),
                          C (0xfff2f8ff), r.leftAt (t0 + 0.17f, 0.6f));
        }
        for (int i = 0; i < 3; ++i)
            for (int k = 0; k < 2; ++k)
            {
                const float u = 0.35f + 0.3f * (float) k, t = 0.15f + 0.25f * (float) i;
                fillQuad (g, r.floorPanel (u, u + 0.16f, t, t + 0.1f), C (0xff8a7558));
            }
        for (int i = 0; i < 3; ++i)
            fillQuad (g, Quad { r.ceilAt (0.4f, 0.2f + 0.25f * (float) i), r.ceilAt (0.6f, 0.2f + 0.25f * (float) i),
                                r.ceilAt (0.6f, 0.13f + 0.25f * (float) i), r.ceilAt (0.4f, 0.13f + 0.25f * (float) i) }, p.light);
    }

    void drawLiving (juce::Graphics& g, const Room& r, const Palette& p)
    {
        const auto& B = r.back;
        const auto win = juce::Rectangle<float> (B.getWidth() * 0.4f, B.getHeight() * 0.42f)
                            .withCentre ({ B.getCentreX() + B.getWidth() * 0.15f, B.getY() + B.getHeight() * 0.38f });
        g.setGradientFill (juce::ColourGradient (C (0xff9cc4e4), win.getTopLeft(), C (0xffeaf3fa), win.getBottomLeft(), false));
        g.fillRect (win);
        g.setColour (C (0xff8a2e2a).withAlpha (0.85f));
        g.fillRect (win.getX() - win.getWidth() * 0.12f, win.getY() - 3, win.getWidth() * 0.16f, win.getHeight() + 8);
        g.fillRect (win.getRight() - win.getWidth() * 0.04f, win.getY() - 3, win.getWidth() * 0.16f, win.getHeight() + 8);
        fillQuad (g, r.floorPanel (0.2f, 0.8f, 0.25f, 0.72f), C (0xff8c5a3c));
        // sofa against the back wall
        const P s0 = r.floorAt (0.18f, 0.9f), s1 = r.floorAt (0.62f, 0.9f);
        const float sh = (s1.x - s0.x) * 0.32f;
        g.setColour (C (0xff3f5a6a));
        g.fillRoundedRectangle (s0.x, s0.y - sh, s1.x - s0.x, sh, 4.0f);
        g.setColour (C (0xff4d6b7c));
        g.fillRoundedRectangle (s0.x + 3, s0.y - sh * 0.55f, s1.x - s0.x - 6, sh * 0.5f, 3.0f);
        juce::ignoreUnused (p);
    }

    void drawRoom (juce::Graphics& g, const Room& r, const Palette& p)
    {
        fillQuad (g, r.rightPanel (0.45f, 0.7f, 0.25f, 1.0f), C (0xff8c6a48));
        const auto& B = r.back;
        g.setColour (C (0xffcfe2f0));
        g.fillRect (juce::Rectangle<float> (B.getWidth() * 0.3f, B.getHeight() * 0.3f).withCentre ({ B.getCentreX(), B.getY() + B.getHeight() * 0.35f }));
        juce::ignoreUnused (p);
    }

    void drawTiles (juce::Graphics& g, const Room& r, const Palette& p)
    {
        const C grout = p.wall.darker (0.25f);
        for (int i = 0; i <= 12; ++i)
        {
            const float f = (float) i / 12.0f;
            line (g, r.leftAt (f, 0.0f), r.leftAt (f, 1.0f), grout, 0.8f);
            line (g, r.rightAt (f, 0.0f), r.rightAt (f, 1.0f), grout, 0.8f);
            line (g, r.leftAt (0.0f, f), r.leftAt (1.0f, f), grout, 0.8f);
            line (g, r.rightAt (0.0f, f), r.rightAt (1.0f, f), grout, 0.8f);
            line (g, r.floorAt (f, 0.0f), r.floorAt (f, 1.0f), grout, 0.8f);
            line (g, r.floorAt (0.0f, f), r.floorAt (1.0f, f), grout, 0.8f);
            const auto& B = r.back;
            g.setColour (grout);
            g.drawHorizontalLine ((int) (B.getY() + B.getHeight() * f), B.getX(), B.getRight());
            g.drawVerticalLine ((int) (B.getX() + B.getWidth() * f), B.getY(), B.getBottom());
        }
        // bathtub
        fillQuad (g, r.floorPanel (0.02f, 0.3f, 0.45f, 0.98f), C (0xfff6f8f8));
        g.setColour (C (0x40000000)); g.strokePath (r.floorPanel (0.02f, 0.3f, 0.45f, 0.98f).path(), juce::PathStrokeType (1.0f));
    }

    void drawFoam (juce::Graphics& g, const Room& r, const Palette& p)
    {
        auto foamQuad = [&] (const Quad& q, int nu, int nv)
        {
            for (int i = 0; i < nu; ++i)
                for (int k = 0; k < nv; ++k)
                {
                    const auto cell = q.sub ((float) i / (float) nu, (float) k / (float) nv, (float) (i + 1) / (float) nu, (float) (k + 1) / (float) nv);
                    juce::Path t1, t2;
                    t1.addTriangle (cell.a, cell.b, (cell.a + cell.b + cell.c + cell.d) * 0.25f);
                    t2.addTriangle (cell.d, cell.c, (cell.a + cell.b + cell.c + cell.d) * 0.25f);
                    const bool alt = (i + k) % 2 == 0;
                    g.setColour (p.wall.brighter (alt ? 0.15f : -0.1f)); g.fillPath (t1);
                    g.setColour (p.wall.darker (alt ? 0.3f : 0.1f));     g.fillPath (t2);
                }
        };
        foamQuad (r.left, 6, 8);
        foamQuad (r.right, 6, 8);
        // window to the control room
        const auto& B = r.back;
        foamQuad (r.backWall, 6, 8);
        const auto win = juce::Rectangle<float> (B.getWidth() * 0.55f, B.getHeight() * 0.28f)
                            .withCentre ({ B.getCentreX(), B.getY() + B.getHeight() * 0.36f });
        g.setColour (C (0xff1b2630)); g.fillRect (win);
        g.setColour (C (0xffffb860).withAlpha (0.6f));
        g.fillEllipse (win.getCentreX() - 4, win.getCentreY() - 2, 8, 4);
        g.setColour (C (0xff222222)); g.drawRect (win, 2.0f);
    }

    void drawCourtyard (juce::Graphics& g, const Room& r, const Palette& p)
    {
        auto windows = [&] (bool left)
        {
            for (int i = 0; i < 6; ++i)
                for (int k = 0; k < 4; ++k)
                {
                    const float t0 = 0.06f + (float) i * 0.155f, v0 = 0.12f + (float) k * 0.19f;
                    fillQuad (g, left ? r.leftPanel (t0, t0 + 0.08f, v0, v0 + 0.1f) : r.rightPanel (t0, t0 + 0.08f, v0, v0 + 0.1f),
                              C (0xff3d4c5c));
                }
        };
        windows (true); windows (false);
        const auto& B = r.back;
        for (int i = 0; i < 5; ++i)
            for (int k = 0; k < 3; ++k)
                g.setColour (C (0xff3d4c5c)),
                g.fillRect (B.getX() + B.getWidth() * (0.08f + 0.18f * (float) i), B.getY() + B.getHeight() * (0.1f + 0.22f * (float) k),
                            B.getWidth() * 0.08f, B.getHeight() * 0.12f);
        archWindow (g, { B.getCentreX() - B.getWidth() * 0.1f, B.getBottom() }, { B.getCentreX() + B.getWidth() * 0.1f, B.getBottom() },
                    { B.getCentreX() - B.getWidth() * 0.1f, B.getBottom() - B.getHeight() * 0.18f },
                    { B.getCentreX() + B.getWidth() * 0.1f, B.getBottom() - B.getHeight() * 0.18f }, B.getHeight() * 0.06f,
                    C (0xff2a241d), C (0xff2a241d));
        for (int i = 0; i <= 10; ++i)
        {
            const float f = (float) i / 10.0f;
            line (g, r.floorAt (f, 0.0f), r.floorAt (f, 1.0f), p.floor.darker (0.2f), 0.8f);
            line (g, r.floorAt (0.0f, f), r.floorAt (1.0f, f), p.floor.darker (0.2f), 0.8f);
        }
        // tree
        const P base = r.floorAt (0.78f, 0.55f);
        g.setColour (C (0xff4a3420)); g.fillRect (base.x - 2, base.y - 30, 4.0f, 30.0f);
        g.setColour (C (0xff4f7a3a)); g.fillEllipse (base.x - 18, base.y - 58, 36, 34);
    }

    void drawCar (juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setGradientFill (juce::ColourGradient (C (0xff2a2622), a.getTopLeft(), C (0xff141210), a.getBottomLeft(), false));
        g.fillRect (a);
        // windscreen
        juce::Path ws;
        ws.startNewSubPath (a.getX() + a.getWidth() * 0.12f, a.getY() + a.getHeight() * 0.12f);
        ws.lineTo (a.getRight() - a.getWidth() * 0.12f, a.getY() + a.getHeight() * 0.12f);
        ws.lineTo (a.getRight() - a.getWidth() * 0.04f, a.getY() + a.getHeight() * 0.56f);
        ws.lineTo (a.getX() + a.getWidth() * 0.04f, a.getY() + a.getHeight() * 0.56f);
        ws.closeSubPath();
        g.setGradientFill (juce::ColourGradient (C (0xfff0b064), a.getCentreX(), a.getY() + a.getHeight() * 0.5f,
                                                 C (0xff5d7fa8), a.getCentreX(), a.getY(), false));
        g.fillPath (ws);
        g.setColour (C (0xff3b3a36));  // road + horizon
        g.fillRect (a.getX(), a.getY() + a.getHeight() * 0.47f, a.getWidth(), a.getHeight() * 0.09f);
        // dashboard
        juce::Path dash;
        dash.startNewSubPath (a.getX(), a.getY() + a.getHeight() * 0.62f);
        dash.quadraticTo (a.getCentreX(), a.getY() + a.getHeight() * 0.5f, a.getRight(), a.getY() + a.getHeight() * 0.62f);
        dash.lineTo (a.getRight(), a.getBottom()); dash.lineTo (a.getX(), a.getBottom()); dash.closeSubPath();
        g.setColour (C (0xff1f1c19)); g.fillPath (dash);
        // steering wheel
        const P c (a.getX() + a.getWidth() * 0.3f, a.getY() + a.getHeight() * 0.8f);
        const float rad = a.getHeight() * 0.22f;
        g.setColour (C (0xff0d0c0b));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, rad * 0.16f);
        g.fillEllipse (c.x - rad * 0.25f, c.y - rad * 0.25f, rad * 0.5f, rad * 0.5f);
        // dials glow
        g.setColour (C (0xffe8a33d).withAlpha (0.7f));
        g.drawEllipse (c.x - rad * 0.55f, c.y - rad * 0.75f, rad * 0.4f, rad * 0.4f, 1.5f);
        g.drawEllipse (c.x + rad * 0.15f, c.y - rad * 0.75f, rad * 0.4f, rad * 0.4f, 1.5f);
        // rear-view mirror
        g.setColour (C (0xff0d0c0b));
        g.fillRoundedRectangle (a.getCentreX() - a.getWidth() * 0.07f, a.getY() + a.getHeight() * 0.13f, a.getWidth() * 0.14f, a.getHeight() * 0.06f, 4.0f);
    }

    void drawPlate (juce::Graphics& g, juce::Rectangle<float> a)
    {
        g.setGradientFill (juce::ColourGradient (C (0xff2a2520), a.getCentre(), C (0xff100e0c), a.getTopLeft(), true));
        g.fillRect (a);
        const auto frame = a.reduced (a.getWidth() * 0.1f, a.getHeight() * 0.16f).translated (0, -a.getHeight() * 0.04f);
        g.setColour (C (0xff3a2a1e)); g.fillRoundedRectangle (frame, 6.0f);
        const auto plate = frame.reduced (frame.getWidth() * 0.06f, frame.getHeight() * 0.1f);
        g.setGradientFill (juce::ColourGradient (C (0xffe4e2dc), plate.getTopLeft(), C (0xff8d8b86), plate.getBottomRight(), false));
        g.fillRect (plate);
        g.setColour (C (0x30ffffff));
        for (int i = 0; i < 6; ++i)
            g.drawLine (plate.getX() + plate.getWidth() * (0.1f + 0.15f * (float) i), plate.getY(),
                        plate.getX() + plate.getWidth() * (0.2f + 0.15f * (float) i), plate.getBottom(), 6.0f);
        g.setColour (C (0xffb0a68f));
        for (auto pt : { plate.getTopLeft(), plate.getTopRight(), plate.getBottomLeft(), plate.getBottomRight() })
        {
            const P outer (pt.x < plate.getCentreX() ? frame.getX() + 3 : frame.getRight() - 3,
                           pt.y < plate.getCentreY() ? frame.getY() + 3 : frame.getBottom() - 3);
            for (int k = 0; k < 6; ++k)
            {
                const P p0 = outer + (pt - outer) * ((float) k / 6.0f);
                const P p1 = outer + (pt - outer) * ((float) (k + 1) / 6.0f);
                g.drawLine (p0.x, p0.y, p1.x + 2.0f, p1.y - 2.0f, 1.2f);
            }
        }
        // transducer + pickups
        g.setColour (C (0xff2b2b2b));
        g.fillEllipse (plate.getCentreX() - 9, plate.getCentreY() - 9, 18, 18);
        g.setColour (C (0xffe8a33d));
        g.fillEllipse (plate.getX() + plate.getWidth() * 0.18f - 5, plate.getY() + plate.getHeight() * 0.3f - 5, 10, 10);
        g.fillEllipse (plate.getRight() - plate.getWidth() * 0.18f - 5, plate.getY() + plate.getHeight() * 0.3f - 5, 10, 10);
    }

    void drawSpeakerAndMics (juce::Graphics& g, const Room& r, float posX, float posDist)
    {
        // Speaker: posX -1..1 across the floor; distance 0..1 from the listener towards the back
        const float t = juce::jmap (posDist, 0.25f, 0.85f);
        const P sp = r.floorAt (juce::jmap (posX, 0.18f, 0.82f), t);
        const float sc = juce::jmap (1.0f - r.persp (t), 0.45f, 1.25f) * r.area.getHeight() / 300.0f;

        g.setColour (C (0x50000000));
        g.fillEllipse (sp.x - 14 * sc, sp.y - 4 * sc, 28 * sc, 8 * sc);
        const auto cab = juce::Rectangle<float> (20 * sc, 32 * sc).withBottomY (sp.y).withCentre ({ sp.x, sp.y - 16 * sc });
        g.setColour (C (0xff1b1815)); g.fillRoundedRectangle (cab, 2.5f * sc);
        g.setColour (C (0xffe8a33d)); g.drawRoundedRectangle (cab, 2.5f * sc, 1.2f);
        g.setColour (C (0xff3a342b));
        g.fillEllipse (cab.getCentreX() - 7 * sc, cab.getBottom() - 17 * sc, 14 * sc, 14 * sc);
        g.fillEllipse (cab.getCentreX() - 3.5f * sc, cab.getY() + 3 * sc, 7 * sc, 7 * sc);

        // Listener: a stereo pair of mics on one stand near the front
        const P mp = r.floorAt (0.5f, 0.04f);
        const float ms = r.area.getHeight() / 300.0f;
        const float barY = mp.y - 46.0f * ms;
        g.setColour (C (0x50000000));
        g.fillEllipse (mp.x - 12 * ms, mp.y - 3 * ms, 24 * ms, 6 * ms);
        g.setColour (C (0xff0f0e0c));
        g.drawLine (mp.x, mp.y, mp.x, barY, 2.2f * ms);
        g.drawLine (mp.x - 16 * ms, barY, mp.x + 16 * ms, barY, 2.2f * ms);
        for (float dx : { -16.0f, 16.0f })
        {
            g.setColour (C (0xffd8d2c4));
            g.fillRoundedRectangle (mp.x + dx * ms - 3 * ms, barY - 13 * ms, 6 * ms, 15 * ms, 3 * ms);
            g.setColour (C (0xff7a756c));
            g.fillRect (mp.x + dx * ms - 3 * ms, barY - 4 * ms, 6 * ms, 2 * ms);
        }
    }
}

//==============================================================================
void drawSpaceArt (juce::Graphics& g, juce::Rectangle<float> area, const brew::SpaceInfo& info,
                   float sizeScale, float posX, float posDist)
{
    if (info.scene == Scene::Car)   { drawCar (g, area); return; }
    if (info.scene == Scene::Plate) { drawPlate (g, area); return; }

    const auto room = makeRoom (area, info, sizeScale);
    const auto pal  = paletteFor (info.scene);
    drawShell (g, room, pal, info.scene == Scene::Courtyard);

    switch (info.scene)
    {
        case Scene::Booth:     drawFoam (g, room, pal); break;
        case Scene::Living:    drawLiving (g, room, pal); break;
        case Scene::Room:      drawRoom (g, room, pal); break;
        case Scene::Studio:    drawStudio (g, room, pal); break;
        case Scene::Office:    drawOffice (g, room, pal); break;
        case Scene::Bathroom:  drawTiles (g, room, pal); break;
        case Scene::Corridor:  drawCorridor (g, room, pal); break;
        case Scene::Chamber:   drawChamber (g, room, pal); break;
        case Scene::Stairwell: drawStairwell (g, room, pal); break;
        case Scene::Hall:      drawHall (g, room, pal); break;
        case Scene::Church:    drawChurch (g, room, pal, false); break;
        case Scene::Cathedral: drawChurch (g, room, pal, true); break;
        case Scene::Warehouse: drawWarehouse (g, room, pal); break;
        case Scene::Courtyard: drawCourtyard (g, room, pal); break;
        case Scene::Car:
        case Scene::Plate:     break;
    }

    drawSpeakerAndMics (g, room, posX, posDist);

    // warm vignette
    g.setGradientFill (juce::ColourGradient (C (0x00000000), area.getCentre(), C (0x99000000), area.getTopLeft(), true));
    g.fillRect (area);
}

} // namespace brewui
