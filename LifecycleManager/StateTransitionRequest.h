#ifndef STATE_TRANSITION_REQUEST_H
#define STATE_TRANSITION_REQUEST_H
#include "ApplicationContext.h"
#include <thread>
#include <semaphore>

namespace WPEFramework
{
    namespace Plugin
    {
        struct StateTransitionRequest
        {
            StateTransitionRequest(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState state): mContext(context), mTargetState(state)
            {
            }
            ApplicationContext* mContext;
            Exchange::ILifecycleManager::LifecycleState mTargetState;
        };
    }
}
#endif
