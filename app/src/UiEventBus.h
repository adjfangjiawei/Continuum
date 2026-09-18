#pragma once

#include "UiDataService.h"

#include <cstdint>

#include <wx/event.h>
#include <wx/window.h>

namespace continuum
{

wxDECLARE_EVENT(
    wxEVT_CONTINUUM_UI_CHANGE,
    wxThreadEvent
);

class UiEventBus final
{
public:
    static UiEventBus& Instance();

    UiEventBus(const UiEventBus&) = delete;
    UiEventBus& operator=(const UiEventBus&) = delete;

    void Start(wxEvtHandler* target);
    void Stop();

    bool IsStarted() const;

private:
    UiEventBus();
    ~UiEventBus();

    wxEvtHandler* target_;
    std::uint64_t subscriptionToken_;
};

}
