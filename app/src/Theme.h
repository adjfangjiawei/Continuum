#pragma once

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/string.h>
#include <wx/window.h>

namespace continuum
{

inline wxString U(const char* value)
{
    return wxString::FromUTF8(value);
}

struct Theme final
{
    static wxColour Background()
    {
        return wxColour(9, 14, 21);
    }

    static wxColour Window()
    {
        return wxColour(16, 23, 33);
    }

    static wxColour Top()
    {
        return wxColour(17, 26, 38);
    }

    static wxColour Sidebar()
    {
        return wxColour(13, 20, 30);
    }

    static wxColour Surface()
    {
        return wxColour(21, 30, 43);
    }

    static wxColour Surface2()
    {
        return wxColour(26, 37, 52);
    }

    static wxColour Surface3()
    {
        return wxColour(32, 45, 63);
    }

    static wxColour Input()
    {
        return wxColour(12, 19, 29);
    }

    static wxColour Border()
    {
        return wxColour(43, 58, 78);
    }

    static wxColour BorderStrong()
    {
        return wxColour(58, 77, 101);
    }

    static wxColour Text()
    {
        return wxColour(232, 237, 245);
    }

    static wxColour Muted()
    {
        return wxColour(146, 160, 180);
    }

    static wxColour Faint()
    {
        return wxColour(104, 119, 141);
    }

    static wxColour Blue()
    {
        return wxColour(91, 140, 255);
    }

    static wxColour Cyan()
    {
        return wxColour(63, 199, 193);
    }

    static wxColour Green()
    {
        return wxColour(68, 198, 147);
    }

    static wxColour Yellow()
    {
        return wxColour(242, 184, 75);
    }

    static wxColour Red()
    {
        return wxColour(239, 106, 114);
    }

    static wxColour Purple()
    {
        return wxColour(167, 135, 255);
    }

    static wxColour Orange()
    {
        return wxColour(242, 138, 75);
    }

    static wxFont Font(int pointSize, wxFontWeight weight = wxFONTWEIGHT_NORMAL)
    {
        wxFont font(
            wxFontInfo(pointSize)
                .Family(wxFONTFAMILY_SWISS)
                .FaceName(U("Segoe UI"))
        );
        font.SetWeight(weight);
        return font;
    }

    static void Apply(wxWindow* window, const wxColour& background = Window())
    {
        if (window == nullptr)
        {
            return;
        }

        window->SetBackgroundColour(background);
        window->SetForegroundColour(Text());
        window->SetFont(Font(10));
    }
};

}
