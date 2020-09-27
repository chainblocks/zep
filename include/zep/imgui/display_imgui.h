#pragma once
#include "../display.h"
#include "../syntax.h"
#include <imgui/imgui.h>
#include <string>

// Can't include this publicly
//#include "zep/mcommon/logger.h"

namespace Zep
{

inline NVec2f toNVec2f(const ImVec2& im)
{
    return NVec2f(im.x, im.y);
}
inline ImVec2 toImVec2(const NVec2f& im)
{
    return ImVec2(im.x, im.y);
}

static ImWchar greek_range[] = { 0x300, 0x52F, 0x1f00, 0x1fff, 0, 0 };
class ZepDisplay_ImGui : public ZepDisplay
{
public:
    ZepDisplay_ImGui()
    {
        for (size_t i = 0; i < m_fonts.size(); i++)
        {
            m_fonts[i] = nullptr;
        }

        // Have to keep ranges around until after the font is built
        auto& io = ImGui::GetIO();
        m_builder.AddRanges(io.Fonts->GetGlyphRangesDefault()); // Add one of the default ranges
        m_builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); // Add one of the default ranges
        m_builder.AddRanges(greek_range);
        m_builder.BuildRanges(&m_ranges); // Build the final result (ordered ranges with all the unique characters submitted)
    }

    ImFont* GetFont(ZepFontType type) const
    {
        if (m_fonts[(int)type] != nullptr)
        {
            return m_fonts[(int)type];
        }
        return ImGui::GetFont();
    }

    // ImGui specific display methods
    float GetFontPointSize(ZepFontType type) const
    {
        ZEP_UNUSED(type);
        return GetFont(type)->FontSize * GetFont(type)->Scale;
    }

    void SetFontPointSize(ZepFontType type, float size)
    {
        ZEP_UNUSED(type);
        // A crude scaling in ImGui for now...
        // We use global font scale instead of doing it 'properly'
        // See the Qt demo for better scaling, because that's built into Qt.
        GetFontCache(type).charCacheDirty = true;
        GetFont(type)->Scale = size / GetFont(type)->FontSize;
    }

    float GetFontHeightPixels(ZepFontType type) const
    {
        return GetFont(type)->FontSize * GetFont(type)->Scale;
        // So Qt claims to return the below; but I've been unable to get similar fonts to match.
        // There is much more padding in Qt than in ImGui, even though the actual font sizes are the same!
        //return (ImGui::GetFont()->Descent + ImGui::GetFont()->Ascent + 1) * 2.0f;
    }

    NVec2f GetTextSize(ZepFontType type, const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const
    {
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        ImFont* font = GetFont(type);
        const float font_size = font->FontSize;
        ImVec2 text_size = font->CalcTextSizeA(font_size * font->Scale, FLT_MAX, FLT_MAX, (const char*)pBegin, (const char*)pEnd, NULL);
        if (text_size.x == 0.0)
        {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_size = font->CalcTextSizeA(font_size * font->Scale, FLT_MAX, FLT_MAX, &chDefault, (&chDefault + 1), NULL);
        }

        return toNVec2f(text_size);
    }

    void DrawChars(ZepFontType type, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (text_end == nullptr)
        {
            text_end = text_begin + strlen((const char*)text_begin);
        }
        if (m_clipRect.Width() == 0)
        {
            drawList->AddText(GetFont(type), GetFont(type)->Scale * GetFont(type)->FontSize, toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddText(GetFont(type), GetFont(type)->Scale * GetFont(type)->FontSize, toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
            drawList->PopClipRect();
        }
    }

    void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // Background rect for numbers
        if (m_clipRect.Width() == 0)
        {
            drawList->AddLine(toImVec2(start), toImVec2(end), ToPackedABGR(color), width);
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddLine(toImVec2(start), toImVec2(end), ToPackedABGR(color), width);
            drawList->PopClipRect();
        }
    }

    void DrawRectFilled(const NRectf& rc, const NVec4f& color) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // Background rect for numbers
        if (m_clipRect.Width() == 0)
        {
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), ToPackedABGR(color));
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), ToPackedABGR(color));
            drawList->PopClipRect();
        }
    }
    
    virtual void PushFont(ZepFontType type) override
    {
        ImGui::PushFont(GetFont(type));
    }

    virtual void PopFont() override
    {
        ImGui::PopFont();
    }

    void SetClipRect(const NRectf& rc)
    {
        m_clipRect = rc;
    }


    void AddFont(ZepFontType type, float pixelHeight, const std::string& filePath)
    {
        ImFontConfig cfg;
        cfg.OversampleH = 4;
        cfg.OversampleV = 4;

        m_fonts[(int)type] = ImGui::GetIO().Fonts->AddFontFromFileTTF(filePath.c_str(), pixelHeight, &cfg, m_ranges.Data);

        ZLOG(INFO, "Type: " << (int)type << " Font Pixel Size: " << pixelHeight);
    }

private:
    NRectf m_clipRect;
    float m_fontScale = 1.0f;
    ImVector<ImWchar> m_ranges;
    ImFontGlyphRangesBuilder m_builder;

    std::array<ImFont*, size_t(ZepFontType::Count)> m_fonts;
}; // namespace Zep

} // namespace Zep
