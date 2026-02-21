/*
* This is a personal academic project. Dear PVS-Studio, please check it.
* PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
*/
#include "MenuModule.h"
#include "../Core/Logo.h"
#include "../InkHUD2.h"  // For InkHUD2::instance()
#include <cstdio>
#include <ctime>
#include "RTC.h"
#include "airtime.h"
#include "PowerStatus.h"
#include "main.h"  // For shutdownAtMsec
#include "mesh/NodeDB.h"  // For node info
#include "mesh/generated/meshtastic/admin.pb.h"  // For BackupLocation enum
#include <Arduino.h>  // For delay()

namespace InkHUD2 {

MenuModule::MenuModule() {
    handlesInput = false;   // Only capture input when open (set in open())
    lockRendering = false;  // Only block other modules when open (set in open())
    alwaysRender = false;
    priority = 150;
}

MenuModule::~MenuModule() {
    // Menu items are not owned by this class
}

void MenuModule::onRender(RenderContext& ctx) {
    if (tipVisible && tipText) {
        renderTip(ctx);
    } else if (visible) {
        switch (currentView) {
            case MenuView::ALERT:
                renderAlert(ctx);
                break;
            case MenuView::SHUTDOWN:
                renderShutdown(ctx);
                break;
            case MenuView::SHUTDOWN_FINAL:
                renderShutdownFinal(ctx);
                break;
            case MenuView::MENU:
            default:
                if (currentMenu) {
                    renderMenu(ctx);
                }
                break;
        }
    }
}

void MenuModule::onInput(Input input) {
    if (tipVisible) {
        // Any input dismisses tip
        dismissTip();
        return;
    }

    // Handle alert view - any click dismisses
    if (currentView == MenuView::ALERT) {
        alertMessage = nullptr;  // Clear message to prevent stale display
        if (alertFromExternal) {
            // Alert was shown from outside (e.g., MapModule) - close completely
            alertFromExternal = false;
            close();
        } else {
            // Alert was shown from menu action - return to menu
            currentView = MenuView::MENU;
            requestUpdate();
        }
        return;
    }

    // Shutdown view ignores input
    if (currentView == MenuView::SHUTDOWN) {
        return;
    }

    if (!visible || !currentMenu) {
        return;
    }

    // Single-button navigation for T-Echo Plus:
    // - SELECT (short press) = move to next item
    // - BACK (long press) = activate/select item

    switch (input) {
        case Input::UP:
            menuList.selectPrev();
            selectedIndex = menuList.getSelectedIndex();
            requestUpdate();
            break;

        case Input::DOWN:
            menuList.selectNext();
            selectedIndex = menuList.getSelectedIndex();
            requestUpdate();
            break;

        case Input::SELECT:
            // Short press = next item (wrap around)
            menuList.selectNext();
            selectedIndex = menuList.getSelectedIndex();
            requestUpdate();
            break;

        case Input::BACK: {
            // Long press = activate selected item
            // First try MenuList for TOGGLE/VALUE handling
            if (menuList.activateSelected()) {
                requestUpdate();
                break;
            }

            // Handle other types manually
            MenuItem* itemPtr = menuList.getSelectedItem();
            if (!itemPtr) break;
            MenuItem& item = *itemPtr;

            switch (item.type) {
                case MenuItemType::ACTION:
                    if (item.action && *item.action) {
                        (*item.action)();
                    }
                    break;

                case MenuItemType::SUBMENU:
                    if (item.submenu && item.submenuCount > 0) {
                        menuStack.push_back({currentMenu, currentMenuCount, selectedIndex});
                        currentMenu = item.submenu;
                        currentMenuCount = item.submenuCount;
                        selectedIndex = 0;
                        // Sync menuList with new submenu
                        menuList.setItems(currentMenu, currentMenuCount);
                        menuList.setSelectedIndex(0);
                        requestUpdate();
                    }
                    break;

                case MenuItemType::BACK:
                    if (!menuStack.empty()) {
                        auto& prev = menuStack.back();
                        currentMenu = prev.menu;
                        currentMenuCount = prev.count;
                        selectedIndex = prev.selectedIndex;
                        menuStack.pop_back();
                        // Sync menuList with parent menu
                        menuList.setItems(currentMenu, currentMenuCount);
                        menuList.setSelectedIndex(selectedIndex);
                        requestUpdate();
                    } else {
                        close();
                    }
                    break;

                case MenuItemType::TOGGLE:
                case MenuItemType::VALUE:
                case MenuItemType::LABEL:
                    // TOGGLE and VALUE handled by menuList.activateSelected() above
                    // LABEL is not interactive
                    break;
            }
            break;
        }

        default:
            break;
    }
}

void MenuModule::open() {
    if (visible) return;

    visible = true;
    active = true;
    handlesInput = true;
    lockRendering = true;
    currentMenu = rootMenu;
    currentMenuCount = rootMenuCount;
    selectedIndex = 0;
    menuStack.clear();

    // Sync with MenuList
    menuList.setItems(currentMenu, currentMenuCount);
    menuList.setSelectedIndex(0);

    requestUpdate();
}

void MenuModule::close() {
    if (!visible) return;

    visible = false;
    active = false;
    handlesInput = false;
    lockRendering = false;
    menuStack.clear();
    currentView = MenuView::MENU;
    alertMessage = nullptr;

    requestUpdate();
}

void MenuModule::setRootMenu(MenuItem* items, uint8_t count) {
    rootMenu = items;
    rootMenuCount = count;
    currentMenu = items;
    currentMenuCount = count;
}

void MenuModule::showTip(const char* text) {
    tipText = text;
    tipVisible = true;
    active = true;
    requestUpdate();
}

void MenuModule::dismissTip() {
    tipVisible = false;
    tipText = nullptr;

    if (!visible) {
        active = false;
    }

    requestUpdate();
}

void MenuModule::renderStatusBar(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t lineH = layout->lineHeight();
    uint16_t margin = layout->statusBarMargin();
    int16_t centerX = ctx.width() / 2;

    // Get current time and date from RTC
    uint32_t rtcTime = getValidTime(RTCQuality::RTCQualityDevice, true);
    uint8_t hours = 0, minutes = 0;
    int year = 2024, month = 1, day = 1;
    if (rtcTime > 0) {
        time_t t = static_cast<time_t>(rtcTime);
        struct tm* tm_info = localtime(&t);
        if (tm_info) {
            hours = tm_info->tm_hour;
            minutes = tm_info->tm_min;
            year = tm_info->tm_year + 1900;
            month = tm_info->tm_mon + 1;
            day = tm_info->tm_mday;
        }
    }

    // Large time at top center (HH:MM)
    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hours, minutes);
    ctx.text(centerX, margin, timeStr, Align::CENTER, Color::BLACK);

    // Status row below time: Bat | Date | FW (3 columns)
    int16_t statusY = margin + lineH + layout->elementSpacing();
    uint16_t smallLineH = layout->smallLineHeight();

    // Prepare text content first to measure widths
    float batVolts = 0.0f;
    if (powerStatus) {
        batVolts = powerStatus->getBatteryVoltageMv() / 1000.0f;
    }
    char batStr[16];
    snprintf(batStr, sizeof(batStr), "%.2fV", batVolts);

    char dateStr[16];
    snprintf(dateStr, sizeof(dateStr), "%04d/%02d/%02d", year, month, day);

    // Measure text widths
    uint16_t batWidth = ctx.textWidthScaled(batStr, Layout::smallScale);
    uint16_t dateWidth = ctx.textWidthScaled(dateStr, Layout::smallScale);
    uint16_t fwWidth = ctx.textWidthScaled(firmwareVersion, Layout::smallScale);

    // Proportional columns based on text width + fixed padding
    int16_t screenW = ctx.width();
    uint16_t colPadding = layout->menuColumnPadding();
    uint16_t col1Width = batWidth + colPadding;
    uint16_t col2Width = dateWidth + colPadding;
    uint16_t col3Width = fwWidth + colPadding;
    uint16_t totalColWidth = col1Width + col2Width + col3Width;

    // Scale to fit screen
    int16_t available = screenW - 2 * margin;
    col1Width = col1Width * available / totalColWidth;
    col2Width = col2Width * available / totalColWidth;
    col3Width = available - col1Width - col2Width;

    // Column centers
    int16_t col1Center = margin + col1Width / 2;
    int16_t col2Center = margin + col1Width + col2Width / 2;
    int16_t col3Center = margin + col1Width + col2Width + col3Width / 2;

    // Separators offset from column boundaries
    uint16_t sepOffset = colPadding / 2;
    int16_t sep1X = margin + col1Width + sepOffset;
    int16_t sep2X = margin + col1Width + col2Width + sepOffset;

    // Draw text centered in each column
    ctx.textScaled(col1Center, statusY, "Bat", Layout::smallScale, Align::CENTER, Color::BLACK);
    ctx.textScaled(col1Center, statusY + smallLineH, batStr, Layout::smallScale, Align::CENTER, Color::BLACK);

    ctx.textScaled(col2Center, statusY, "Date", Layout::smallScale, Align::CENTER, Color::BLACK);
    ctx.textScaled(col2Center, statusY + smallLineH, dateStr, Layout::smallScale, Align::CENTER, Color::BLACK);

    ctx.textScaled(col3Center, statusY, "FW", Layout::smallScale, Align::CENTER, Color::BLACK);
    ctx.textScaled(col3Center, statusY + smallLineH, firmwareVersion, Layout::smallScale, Align::CENTER, Color::BLACK);

    // Draw vertical dotted separators
    for (int16_t py = statusY; py < statusY + smallLineH * 2; py += layout->dotSpacing()) {
        ctx.pixel(sep1X, py, Color::BLACK);
        ctx.pixel(sep2X, py, Color::BLACK);
    }
}

void MenuModule::renderMenu(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t lineH = layout->lineHeight();
    uint16_t margin = layout->menuMargin();

    // Menu background
    ctx.clear();

    // Check if we're in a submenu (stack not empty)
    bool isSubmenu = !menuStack.empty();
    int16_t y;

    if (isSubmenu) {
        // Submenu: no status bar, start from top
        y = margin + layout->elementSpacing();
    } else {
        // Root menu: show status bar at top
        renderStatusBar(ctx);

        // Calculate where status bar ends
        uint16_t smallLineH = layout->smallLineHeight();
        int16_t statusBarEnd = layout->statusBarMargin() + lineH + layout->elementSpacing() + smallLineH * 2 + layout->elementSpacing();

        // Dotted separator line
        for (int16_t x = margin; x < ctx.width() - margin; x += layout->dotSpacing()) {
            ctx.pixel(x, statusBarEnd, Color::BLACK);
        }

        y = statusBarEnd + layout->sectionSpacing();
    }

    // Calculate hint height at bottom (reserve space)
    uint16_t hintLineH = layout->hintLineHeight();
    int16_t hintY = ctx.height() - hintLineH - layout->elementSpacing();

    // Render menu items using MenuList
    menuList.render(ctx, y, hintY, margin);

    // Navigation hint at bottom (smallest font)
    ctx.textScaled(ctx.width() / 2, hintY, "Click: Next Hold: Select", Layout::hintScale, Align::CENTER, Color::BLACK);
}

void MenuModule::renderTip(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t padding = layout->padding();
    uint16_t margin = layout->margin();
    uint16_t lineH = layout->lineHeight();

    // Clear screen
    ctx.clear();

    // Tip box centered on screen
    uint16_t boxW = ctx.width() - layout->textInset() * margin;
    uint16_t boxH = layout->tipBoxHeight();
    int16_t boxX = (ctx.width() - boxW) / 2;
    int16_t boxY = (ctx.height() - boxH) / 2;

    // Box background and border
    ctx.fillRoundRect(boxX, boxY, boxW, boxH, padding, Color::WHITE);
    ctx.roundRect(boxX, boxY, boxW, boxH, padding, Color::BLACK);

    // "Tip" header
    ctx.text(ctx.width() / 2, boxY + padding, "Tip", Align::CENTER, Color::BLACK);

    // Tip text (wrapped)
    if (tipText) {
        ctx.textWrapped(boxX + padding, boxY + lineH + padding,
                        boxW - 2 * padding, tipText, Color::BLACK);
    }

    // Dismiss instruction
    ctx.text(ctx.width() / 2, boxY + boxH - lineH,
             "Press any button to continue", Align::CENTER, Color::BLACK);
}

void MenuModule::showAlert(const char* message, bool hideHint) {
    // Track if alert is from external source (menu wasn't open)
    alertFromExternal = !visible;
    alertHideHint = hideHint;

    alertMessage = message;
    currentView = MenuView::ALERT;
    visible = true;
    active = true;
    handlesInput = true;
    lockRendering = true;
    requestUpdate();
}

void MenuModule::startShutdown() {
    // FIRST: Show "Shutting down..." immediately for user feedback
    currentView = MenuView::SHUTDOWN;
    InkHUD2::instance().requestFullRefresh();
    InkHUD2::instance().update();

    // NOW do the backup while user sees "Shutting down..."
    nodeDB->backupPreferences(meshtastic_AdminMessage_BackupLocation_FLASH);
    nodeDB->saveToDisk();

    // Switch to final logo screen
    currentView = MenuView::SHUTDOWN_FINAL;
    InkHUD2::instance().requestFullRefresh();
    InkHUD2::instance().update();

    // Brief pause to show final screen
    delay(500);

    // Trigger actual shutdown
    ::shutdownAtMsec = millis();
}

void MenuModule::renderAlert(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t lineH = layout->lineHeight();
    uint16_t margin = layout->menuMargin();

    ctx.clear();

    int16_t contentTop;

    if (alertFromExternal) {
        // External alert (from MapModule etc.) - simple header, no status bar
        ctx.text(ctx.width() / 2, margin, "Info", Align::CENTER, Color::BLACK);

        // Dotted separator
        int16_t sepY = margin + lineH + layout->elementSpacing();
        for (int16_t x = margin; x < ctx.width() - margin; x += layout->dotSpacing()) {
            ctx.pixel(x, sepY, Color::BLACK);
        }
        contentTop = sepY + layout->sectionSpacing() * 2;
    } else {
        // Menu alert - show full status bar
        renderStatusBar(ctx);

        // Calculate content area (below status bar)
        uint16_t smallLineH = layout->smallLineHeight();
        int16_t statusBarEnd = layout->statusBarMargin() + lineH + layout->elementSpacing() + smallLineH * 2 + layout->elementSpacing();

        // Dotted separator
        for (int16_t x = margin; x < ctx.width() - margin; x += layout->dotSpacing()) {
            ctx.pixel(x, statusBarEnd, Color::BLACK);
        }
        contentTop = statusBarEnd + layout->sectionSpacing();
    }

    int16_t hintY = ctx.height() - layout->hintLineHeight() - layout->elementSpacing();

    // Draw alert message (wrapped)
    if (alertMessage) {
        ctx.textWrapped(margin + layout->textInset(), contentTop + lineH,
                        ctx.width() - 2 * margin - 2 * layout->textInset(), alertMessage, Color::BLACK);
    }

    // Hint at bottom (unless hidden for progress alerts)
    if (!alertHideHint) {
        const char* hint = alertFromExternal ? "Click: OK" : "Click: return to menu";
        ctx.textScaled(ctx.width() / 2, hintY, hint, Layout::hintScale, Align::CENTER, Color::BLACK);
    }
}

void MenuModule::renderShutdown(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t w = ctx.width();
    uint16_t h = ctx.height();

    // Clear screen
    ctx.clear();

    // Logo region (60% width, 40% height, above center)
    uint16_t logoWLimit = w * Layout::shutdownLogoWidthPercent / 100;
    uint16_t logoHLimit = h * Layout::shutdownLogoHeightPercent / 100;
    uint16_t logoW = Logo::getWidth(logoWLimit, logoHLimit);
    uint16_t logoH = Logo::getHeight(logoWLimit, logoHLimit);

    int16_t centerX = w / 2;
    int16_t logoCY = h / 3;  // Upper third

    // Draw Meshtastic logo
    Logo::draw(ctx, centerX, logoCY, logoW, logoH);

    // "Shutting Down..." below logo
    int16_t textY = logoCY + logoH / 2 + layout->lineHeight();
    ctx.text(centerX, textY, "Shutting Down...", Align::CENTER, Color::BLACK);
}

void MenuModule::renderShutdownFinal(RenderContext& ctx) {
    const Layout* layout = ctx.getLayout();
    if (!layout) return;

    uint16_t w = ctx.width();
    uint16_t h = ctx.height();

    // Clear screen
    ctx.clear();

    // Logo region (60% width, 40% height, above center)
    uint16_t logoWLimit = w * Layout::shutdownLogoWidthPercent / 100;
    uint16_t logoHLimit = h * Layout::shutdownLogoHeightPercent / 100;
    uint16_t logoW = Logo::getWidth(logoWLimit, logoHLimit);
    uint16_t logoH = Logo::getHeight(logoWLimit, logoHLimit);

    int16_t centerX = w / 2;
    int16_t logoCY = h / 3;  // Upper third

    // Draw Meshtastic logo
    Logo::draw(ctx, centerX, logoCY, logoW, logoH);

    // Node short name below logo
    meshtastic_NodeInfoLite *ourNode = nodeDB->getMeshNode(nodeDB->getNodeNum());
    if (ourNode && ourNode->has_user) {
        int16_t textY = logoCY + logoH / 2 + layout->lineHeight();
        ctx.text(centerX, textY, ourNode->user.short_name, Align::CENTER, Color::BLACK);
    }
}

} // namespace InkHUD2
