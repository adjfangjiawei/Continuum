#include "UiEventBus.h"

#include <wx/app.h>

namespace continuum
{

wxDEFINE_EVENT(
    wxEVT_CONTINUUM_UI_CHANGE,
    wxThreadEvent
);

UiEventBus& UiEventBus::Instance()
{
    static UiEventBus instance;
    return instance;
}

UiEventBus::UiEventBus()
    : target_(nullptr),
      subscriptionToken_(0)
{
}

UiEventBus::~UiEventBus()
{
    Stop();
}

void UiEventBus::Start(wxEvtHandler* target)
{
    if (target == nullptr)
    {
        return;
    }

    Stop();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        target_ = target;
    }

    const std::uint64_t token =
        UiDataService::Instance().Subscribe(
            [this](const UiChangeEvent& change) {
                std::lock_guard<std::mutex> lock(mutex_);

                if (target_ == nullptr)
                {
                    return;
                }

                auto* event = new wxThreadEvent(
                    wxEVT_CONTINUUM_UI_CHANGE
                );

                event->SetPayload(change);
                wxQueueEvent(target_, event);
            }
        );

    bool keepSubscription = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (target_ == target)
        {
            subscriptionToken_ = token;
            keepSubscription = true;
        }
    }

    if (!keepSubscription && token != 0)
    {
        UiDataService::Instance().Unsubscribe(token);
    }
}

void UiEventBus::Stop()
{
    std::uint64_t token = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        target_ = nullptr;
        token = subscriptionToken_;
        subscriptionToken_ = 0;
    }

    if (token != 0)
    {
        UiDataService::Instance().Unsubscribe(token);
    }
}

bool UiEventBus::IsStarted() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return target_ != nullptr &&
        subscriptionToken_ != 0;
}

}
