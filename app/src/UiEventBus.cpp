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
    target_ = target;

    subscriptionToken_ =
        UiDataService::Instance().Subscribe(
            [this](const UiChangeEvent& change) {
                wxEvtHandler* target = target_;

                if (target == nullptr)
                {
                    return;
                }

                auto* event = new wxThreadEvent(
                    wxEVT_CONTINUUM_UI_CHANGE
                );

                event->SetPayload(change);

                /*
                 * wxQueueEvent 可以从后台线程安全地把事件
                 * 投递到拥有目标窗口的 wxWidgets 主线程。
                 */
                wxQueueEvent(target, event);
            }
        );
}

void UiEventBus::Stop()
{
    if (subscriptionToken_ != 0)
    {
        UiDataService::Instance().Unsubscribe(
            subscriptionToken_
        );

        subscriptionToken_ = 0;
    }

    target_ = nullptr;
}

bool UiEventBus::IsStarted() const
{
    return target_ != nullptr &&
        subscriptionToken_ != 0;
}

}
