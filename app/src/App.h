#pragma once

#include <wx/app.h>

namespace continuum
{

class ContinuumApp final : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;
};

}
