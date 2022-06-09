#include "InputManager.hpp"
#include "../../Compositor.hpp"

void CInputManager::newTabletTool(wlr_input_device* pDevice) {
    const auto PNEWTABLET = &m_lTablets.emplace_back();

    PNEWTABLET->wlrTablet = pDevice->tablet;
    PNEWTABLET->wlrDevice = pDevice;
    PNEWTABLET->wlrTabletV2 = wlr_tablet_create(g_pCompositor->m_sWLRTabletManager, g_pCompositor->m_sSeat.seat, pDevice);
    PNEWTABLET->wlrTablet->data = PNEWTABLET;

    Debug::log(LOG, "Attaching tablet to cursor!");

    wlr_cursor_attach_input_device(g_pCompositor->m_sWLRCursor, pDevice);

    PNEWTABLET->hyprListener_Destroy.initCallback(&pDevice->events.destroy, [](void* owner, void* data) {
        const auto PTAB = (STablet*)owner;

        g_pInputManager->m_lTablets.remove(*PTAB);

        Debug::log(LOG, "Removed a tablet");
    }, PNEWTABLET, "Tablet");

    PNEWTABLET->hyprListener_Axis.initCallback(&pDevice->tablet->events.axis, [](void* owner, void* data) {

        Debug::log(LOG, "Tablet axis");

        const auto EVENT = (wlr_tablet_tool_axis_event*)data;
        const auto PTAB = (STablet*)owner;

        switch (EVENT->tool->type) {
            case WLR_TABLET_TOOL_TYPE_MOUSE:
                wlr_cursor_move(g_pCompositor->m_sWLRCursor, PTAB->wlrDevice, EVENT->dx, EVENT->dy);
                g_pInputManager->refocus();
                break;
            default:
                double x = (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_X) ? EVENT->x : NAN;
                double y = (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_Y) ? EVENT->y : NAN;
                wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, PTAB->wlrDevice, x, y);
                g_pInputManager->refocus();
                break;
        }

        const auto PTOOL = g_pInputManager->ensureTabletToolPresent(PTAB, EVENT->tool);

        // TODO: this might be wrong
        if (PTOOL->active)
            g_pInputManager->refocus();

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
            wlr_tablet_v2_tablet_tool_notify_pressure(PTOOL->wlrTabletToolV2, EVENT->pressure);

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE)
            wlr_tablet_v2_tablet_tool_notify_distance(PTOOL->wlrTabletToolV2, EVENT->distance);

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
            wlr_tablet_v2_tablet_tool_notify_rotation(PTOOL->wlrTabletToolV2, EVENT->rotation);

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER)
            wlr_tablet_v2_tablet_tool_notify_slider(PTOOL->wlrTabletToolV2, EVENT->slider);

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL)
            wlr_tablet_v2_tablet_tool_notify_wheel(PTOOL->wlrTabletToolV2, EVENT->wheel_delta, 0);

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_X)
            PTOOL->tiltX = EVENT->tilt_x;

        if (EVENT->updated_axes & WLR_TABLET_TOOL_AXIS_TILT_Y)
            PTOOL->tiltY = EVENT->tilt_y;

        if (EVENT->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y))
            wlr_tablet_v2_tablet_tool_notify_tilt(PTOOL->wlrTabletToolV2, PTOOL->tiltX, PTOOL->tiltY);

    }, PNEWTABLET, "Tablet");

    PNEWTABLET->hyprListener_Tip.initCallback(&pDevice->tablet->events.tip, [](void* owner, void* data) {
        const auto EVENT = (wlr_tablet_tool_tip_event*)data;
        const auto PTAB = (STablet*)owner;

        Debug::log(LOG, "Tablet tip");

        const auto PTOOL = g_pInputManager->ensureTabletToolPresent(PTAB, EVENT->tool);

        // TODO: this might be wrong
        if (EVENT->state == WLR_TABLET_TOOL_TIP_DOWN)
            wlr_send_tablet_v2_tablet_tool_down(PTOOL->wlrTabletToolV2);
        else
            wlr_send_tablet_v2_tablet_tool_up(PTOOL->wlrTabletToolV2);
            
    }, PNEWTABLET, "Tablet");

    PNEWTABLET->hyprListener_Button.initCallback(&pDevice->tablet->events.button, [](void* owner, void* data) {
        const auto EVENT = (wlr_tablet_tool_button_event*)data;
        const auto PTAB = (STablet*)owner;

        Debug::log(LOG, "Tablet button");

        const auto PTOOL = g_pInputManager->ensureTabletToolPresent(PTAB, EVENT->tool);

        wlr_tablet_v2_tablet_tool_notify_button(PTOOL->wlrTabletToolV2, (zwp_tablet_pad_v2_button_state)EVENT->button, (zwp_tablet_pad_v2_button_state)EVENT->state);
            
    }, PNEWTABLET, "Tablet");

    PNEWTABLET->hyprListener_Proximity.initCallback(&pDevice->tablet->events.proximity, [](void* owner, void* data) {
        const auto EVENT = (wlr_tablet_tool_proximity_event*)data;
        const auto PTAB = (STablet*)owner;

        Debug::log(LOG, "Tablet proxim");

        const auto PTOOL = g_pInputManager->ensureTabletToolPresent(PTAB, EVENT->tool);

        if (EVENT->state == WLR_TABLET_TOOL_PROXIMITY_OUT) {
            PTOOL->active = false;
            Debug::log(LOG, "Tool active -> false");
        } else {
            PTOOL->active = true;
            Debug::log(LOG, "Tool active -> true");
            g_pInputManager->refocus();
        }
            
    }, PNEWTABLET, "Tablet");
}

STabletTool* CInputManager::ensureTabletToolPresent(STablet* pTablet, wlr_tablet_tool* pTool) {
    if (pTool->data == nullptr) {
        const auto PTOOL = &m_lTabletTools.emplace_back();

        Debug::log(LOG, "Creating tablet tool v2 for %x", pTool);

        PTOOL->wlrTabletTool = pTool;
        pTool->data = PTOOL;

        PTOOL->wlrTabletToolV2 = wlr_tablet_tool_create(g_pCompositor->m_sWLRTabletManager, g_pCompositor->m_sSeat.seat, pTool);

        PTOOL->hyprListener_TabletToolDestroy.initCallback(&pTool->events.destroy, [](void* owner, void* data) {
            const auto PTOOL = (STabletTool*)owner;

            PTOOL->wlrTabletTool->data = nullptr;
            g_pInputManager->m_lTabletTools.remove(*PTOOL);
        }, PTOOL, "Tablet Tool V1");

        //TODO: set cursor request
    }

    return (STabletTool*)pTool->data;
}

void CInputManager::newTabletPad(wlr_input_device* pDevice) {
    const auto PNEWPAD = &m_lTabletPads.emplace_back();

    PNEWPAD->wlrTabletPadV2 = wlr_tablet_pad_create(g_pCompositor->m_sWLRTabletManager, g_pCompositor->m_sSeat.seat, pDevice);

    PNEWPAD->hyprListener_Button.initCallback(&pDevice->tablet_pad->events.button, [](void* owner, void* data) {

        Debug::log(LOG, "TabletPad button");

        const auto EVENT = (wlr_tablet_pad_button_event*)data;
        const auto PPAD = (STabletPad*)owner;

        wlr_tablet_v2_tablet_pad_notify_mode(PPAD->wlrTabletPadV2, EVENT->group, EVENT->mode, EVENT->time_msec);
        wlr_tablet_v2_tablet_pad_notify_button(PPAD->wlrTabletPadV2, EVENT->button, EVENT->time_msec, (zwp_tablet_pad_v2_button_state)EVENT->state);

    }, PNEWPAD, "Tablet Pad");

    PNEWPAD->hyprListener_Strip.initCallback(&pDevice->tablet_pad->events.strip, [](void* owner, void* data) {

        const auto EVENT = (wlr_tablet_pad_strip_event*)data;
        const auto PPAD = (STabletPad*)owner;

        Debug::log(LOG, "TabletPad strip");

        wlr_tablet_v2_tablet_pad_notify_strip(PPAD->wlrTabletPadV2, EVENT->strip, EVENT->position, EVENT->source == WLR_TABLET_PAD_STRIP_SOURCE_FINGER, EVENT->time_msec);

    }, PNEWPAD, "Tablet Pad");

    PNEWPAD->hyprListener_Ring.initCallback(&pDevice->tablet_pad->events.strip, [](void* owner, void* data) {

        const auto EVENT = (wlr_tablet_pad_ring_event*)data;
        const auto PPAD = (STabletPad*)owner;

        Debug::log(LOG, "TabletPad ring");

        wlr_tablet_v2_tablet_pad_notify_ring(PPAD->wlrTabletPadV2, EVENT->ring, EVENT->position, EVENT->source == WLR_TABLET_PAD_RING_SOURCE_FINGER, EVENT->time_msec);

    }, PNEWPAD, "Tablet Pad");

    PNEWPAD->hyprListener_Attach.initCallback(&pDevice->tablet_pad->events.strip, [](void* owner, void* data) {

        const auto TABLET = (wlr_tablet_tool*)data;
        const auto PPAD = (STabletPad*)owner;

        Debug::log(LOG, "TabletPad attach");

        PPAD->pTabletParent = (STablet*)TABLET->data;

        if (!PPAD->pTabletParent)
            Debug::log(ERR, "tabletpad got attached to a nullptr tablet!! this might be bad.");

    }, PNEWPAD, "Tablet Pad");

    PNEWPAD->hyprListener_Destroy.initCallback(&pDevice->events.destroy, [](void* owner, void* data) {

        const auto PPAD = (STabletPad*)owner;

        Debug::log(LOG, "TabletPad destroy");

        g_pInputManager->m_lTabletPads.remove(*PPAD);

        Debug::log(LOG, "Removed a tablet pad");

    }, PNEWPAD, "Tablet Pad");
}