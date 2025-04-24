#include "gd_win32.h"
#include "gd_log.h"
#include <cfgmgr32.h>
#define INITGUID
#include <Hidclass.h>
#include "modules/Notifications.h"

static HCMNOTIFICATION s_NotifyContext;
static ULONG s_DeviceNotification = 0;
static ULONG s_LastDeviceNotification = ~0uL;


static DWORD CALLBACK NotificationCallback(
    _In_ HCMNOTIFICATION       hNotify,
    _In_opt_ PVOID             Context,
    _In_ CM_NOTIFY_ACTION      Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD                 EventDataSize
)
{
    if (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ||
        Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
    {
        InterlockedIncrement(&s_DeviceNotification);

        const auto ActionString = (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL) ? "Arrival" : "Removal";
        switch (EventData->FilterType)
        {
        case CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE:
            GD_Log("[CM] Device interface %s: %S\n", ActionString, EventData->u.DeviceInterface.SymbolicLink);
            break;
        case CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE:
            GD_Log("[CM] Device handle %s\n");
            break;
        case CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE:
            GD_Log("[CM] Device instance %s: %S\n", ActionString, EventData->u.DeviceInstance.InstanceId);
            break;
        default:
            GD_Log("[CM] Unknown filter type %d %s\n", EventData->FilterType, ActionString);
            break;
        }
    }
    return ERROR_SUCCESS;
}

bool Notifications_DevicesChanged()
{
    ULONG DeviceNotification = InterlockedCompareExchange(&s_DeviceNotification, 0, 0);
    if (s_LastDeviceNotification != DeviceNotification)
    {
        s_LastDeviceNotification = DeviceNotification;
        return true;
    }
    return false;
}

void Notifications_Init()
{
    CM_NOTIFY_FILTER Filter{ sizeof(Filter) };

    Filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    Filter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_HID;
    CONFIGRET ret = CM_Register_Notification(&Filter, NULL, NotificationCallback, &s_NotifyContext);
    if (ret != CR_SUCCESS)
    {
        GD_Log("CM_Register_Notification failed: %d\n", ret);
    }
}

void Notifications_Shutdown()
{
    if (s_NotifyContext)
    {
        CM_Unregister_Notification(s_NotifyContext);
        s_NotifyContext = NULL;
    }
}
