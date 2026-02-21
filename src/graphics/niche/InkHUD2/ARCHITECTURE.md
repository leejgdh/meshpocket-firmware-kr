# InkHUD2 Architecture

## Overview

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                           InkHUD2 ARCHITECTURE                                ║
╚══════════════════════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────────────────────┐
│                           MESHTASTIC FIRMWARE                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │   NodeDB    │  │ MeshService │  │   Router    │  │   Button    │          │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘          │
└─────────┼────────────────┼────────────────┼────────────────┼─────────────────┘
          │                │                │                │
          │    Events      │   Messages     │   Packets      │  Input
          ▼                ▼                ▼                ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              Events.cpp                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │  Event { type: EventType, data: EventData }                              │ │
│  │  ──────────────────────────────────────────                              │ │
│  │  NODE_DISCOVERED | NODE_UPDATED | NODE_LOST                              │ │
│  │  MESSAGE_RECEIVED | MESSAGE_SENT | MESSAGE_FAILED                        │ │
│  │  BOOT_COMPLETE | SHUTDOWN_REQUESTED | LOW_BATTERY                        │ │
│  │  BLUETOOTH_CONNECTED | GPS_FIX_ACQUIRED | ...                            │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │  Input { UP | DOWN | LEFT | RIGHT | SELECT | BACK | DOUBLE_TAP | ... }  │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                    onEvent(e)        │         onInput(i)
                                      ▼
╔══════════════════════════════════════════════════════════════════════════════╗
║                         InkHUD2 (Singleton + OSThread)                        ║
║  ┌────────────────────────────────────────────────────────────────────────┐  ║
║  │  init(driver, font, rotation)   // Initialize display + modules        │  ║
║  │  runOnce()                      // OSThread callback (100ms interval)  │  ║
║  │  update()                       // Main loop - check if render needed  │  ║
║  │  render()                       // Pipe.render() → Driver.update()     │  ║
║  └────────────────────────────────────────────────────────────────────────┘  ║
║                                                                               ║
║  Owns:                                                                        ║
║  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       ║
║  │  Buffer  │  │   Font   │  │  Layout  │  │   Pipe   │  │  Events  │       ║
║  └──────────┘  └──────────┘  └──────────┘  └────┬─────┘  └──────────┘       ║
║                                                  │                            ║
╚══════════════════════════════════════════════════╪════════════════════════════╝
                                                   │
                                                   ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                                    PIPE                                       │
│  ═══════════════════════════════════════════════════════════════════════════ │
│  Module Management & Event Routing                                            │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │  Slots (1, 2, or 4)                                                      │ │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐                     │ │
│  │  │ Slot 0  │  │ Slot 1  │  │ Slot 2  │  │ Slot 3  │  ← Regular Modules  │ │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘                     │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │  System Modules (sorted by priority, render on top)                      │ │
│  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐    │ │
│  │  │ Battery (50) │ │ Notify (100)│ │ Menu (150)   │ │ Boot (200)   │    │ │
│  │  └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘    │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  dispatchEvent(e)  →  All modules receive event                              │
│  dispatchInput(i)  →  System module (if handlesInput) OR focused slot        │
│  render(ctx)       →  Slots first, then system modules by priority           │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                                  MODULES                                      │
│  ═══════════════════════════════════════════════════════════════════════════ │
│                                                                               │
│  ┌─────────────────────────┐      ┌─────────────────────────────────────────┐│
│  │      Module (base)      │      │           SystemModule (base)           ││
│  │  ───────────────────    │      │  ───────────────────────────────────    ││
│  │  onRender(ctx)          │      │  inherits Module +                      ││
│  │  onEvent(e)             │      │  lockRendering: bool                    ││
│  │  onInput(input)         │      │  alwaysRender: bool                     ││
│  │  onActivate/Deactivate  │      │  priority: uint8_t                      ││
│  │  handlesInput: bool     │      │  checkAutoTransition()                  ││
│  └─────────────────────────┘      └─────────────────────────────────────────┘│
│                                                                               │
│  ┌───────────────────────────────────────────────────────────────────────┐   │
│  │ REGULAR MODULES (render in slots)                                      │   │
│  │ ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐           │   │
│  │ │  MessageModule  │ │  NodeListModule │ │    MapModule    │           │   │
│  │ │  ─────────────  │ │  ─────────────  │ │  ─────────────  │           │   │
│  │ │  - ChatView     │ │  - All nodes    │ │  - Map view     │           │   │
│  │ │  - DMView       │ │  - Favorites    │ │  - Settings     │           │   │
│  │ │  - Tab bar      │ │  - Recent       │ │  - MenuList     │           │   │
│  │ │  - Footer       │ │  - StatusBar    │ │  - StatusBar    │           │   │
│  │ │  - StatusBar    │ │  - ContentArea  │ │  - ContentArea  │           │   │
│  │ │  - ContentArea  │ │                 │ │                 │           │   │
│  │ └─────────────────┘ └─────────────────┘ └─────────────────┘           │   │
│  └───────────────────────────────────────────────────────────────────────┘   │
│  ┌───────────────────────────────────────────────────────────────────────┐   │
│  │ SYSTEM MODULES (overlays, always on top)                               │   │
│  │ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐   │   │
│  │ │BatteryModule │ │NotifyModule  │ │  MenuModule  │ │  BootModule  │   │   │
│  │ │ priority=50  │ │ priority=100 │ │ priority=150 │ │ priority=200 │   │   │
│  │ │ alwaysRender │ │ overlay bar  │ │ lockRendering│ │ lockRendering│   │   │
│  │ │ top-right    │ │ top popup    │ │ full screen  │ │ full screen  │   │   │
│  │ │              │ │              │ │ - MenuList   │ │ - Logo       │   │   │
│  │ │              │ │              │ │ - StatusBar  │ │ - Pairing    │   │   │
│  │ │              │ │              │ │ - Shutdown   │ │              │   │   │
│  │ └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘   │   │
│  └───────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                             onRender(ctx)
                                      ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                                   CORE                                        │
│  ═══════════════════════════════════════════════════════════════════════════ │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                          RenderContext                                   │ │
│  │  ─────────────────────────────────────────────────────────────────────  │ │
│  │  Passed to modules - composition NOT inheritance                         │ │
│  │                                                                          │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │ │
│  │  │  Primitives  │  │     Text     │  │   Effects    │                   │ │
│  │  │  ──────────  │  │  ──────────  │  │  ──────────  │                   │ │
│  │  │  pixel()     │  │  text()      │  │  hatch()     │                   │ │
│  │  │  line()      │  │  textScaled()│  │  clear()     │                   │ │
│  │  │  rect()      │  │  textWidth() │  │              │                   │ │
│  │  │  fillRect()  │  │  textWrapped │  │              │                   │ │
│  │  │  circle()    │  │              │  │              │                   │ │
│  │  │  roundRect() │  │              │  │              │                   │ │
│  │  │  triangle()  │  │              │  │              │                   │ │
│  │  │  hLine()     │  │              │  │              │                   │ │
│  │  │  vLine()     │  │              │  │              │                   │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                   │ │
│  │                                                                          │ │
│  │  setClip(rect)  // For slot-based rendering                              │ │
│  │  X(0.5), Y(0.5) // Relative → absolute coordinates                       │ │
│  │  width(), height() // Current clip dimensions                            │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │    Buffer    │  │     Font     │  │    Layout    │  │TextRenderer  │      │
│  │  ──────────  │  │  ──────────  │  │  ──────────  │  │  ──────────  │      │
│  │  1-bit pixel │  │  CJK + ASCII │  │  Constants   │  │  Clip-aware  │      │
│  │  200x200     │  │  18x18 px    │  │  Scales      │  │  UTF-8       │      │
│  │  setPixel()  │  │  getGlyph()  │  │  Spacing     │  │  Wrapping    │      │
│  │  clearRect() │  │  binarySearch│  │  lineHeight()│  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘      │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                               UI COMPONENTS                                   │
│  ═══════════════════════════════════════════════════════════════════════════ │
│  Shared across modules - no code duplication                                  │
│                                                                               │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐               │
│  │    StatusBar    │  │     Footer      │  │   ContentArea   │               │
│  │  ─────────────  │  │  ─────────────  │  │  ─────────────  │               │
│  │  render(title,  │  │  render(hint)   │  │  calculateArea( │               │
│  │         icon)   │  │                 │  │    statusBottom, │               │
│  │                 │  │  Icons:         │  │    footerTop)   │               │
│  │  Icons:         │  │  - Hint text    │  │                 │               │
│  │  - ENVELOPE     │  │  - Indicators   │  │  Returns: x,y,  │               │
│  │  - USERS        │  │                 │  │           w,h   │               │
│  │  - GEAR         │  │                 │  │                 │               │
│  │  - INFO         │  │                 │  │                 │               │
│  │  - MAP          │  │                 │  │                 │               │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘               │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                             MenuList                                     │ │
│  │  ─────────────────────────────────────────────────────────────────────  │ │
│  │  Reusable menu rendering component                                       │ │
│  │                                                                          │ │
│  │  setItems(items[], count)  // Set menu items                             │ │
│  │  selectNext() / selectPrev()  // Navigation                              │ │
│  │  activateSelected()  // Handle TOGGLE/VALUE types                        │ │
│  │  render(ctx, startY, endY, margin)  // Draw menu                         │ │
│  │                                                                          │ │
│  │  Used by: MenuModule, MapModule (settings)                               │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                             MenuItem                                     │ │
│  │  ─────────────────────────────────────────────────────────────────────  │ │
│  │  label: const char*                                                      │ │
│  │  type: ACTION | TOGGLE | VALUE | SUBMENU | BACK | LABEL                  │ │
│  │  toggleValue: bool* | value.options[] | submenu* | action*               │ │
│  │  onChange: callback                                                      │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              DISPLAY DRIVER                                   │
│  ═══════════════════════════════════════════════════════════════════════════ │
│                                                                               │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                        DisplayDriver (interface)                         │ │
│  │  ─────────────────────────────────────────────────────────────────────  │ │
│  │  init()                   // Initialize hardware                         │ │
│  │  width(), height()        // Display dimensions                          │ │
│  │  busy()                   // Check if update in progress                 │ │
│  │  update(data, fullRefresh) // Send buffer to display                     │ │
│  │  sleep(), wake()          // Power management                            │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                        │
│                                      ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                     EInkAdapter (implementation)                         │ │
│  │  ─────────────────────────────────────────────────────────────────────  │ │
│  │  Wraps existing E-Ink driver from InkHUD                                 │ │
│  │  FAST refresh by default (no ghosting)                                   │ │
│  │  FULL refresh only on explicit request                                   │ │
│  │                                                                          │ │
│  │  finalizeUpdate() - copies current→old memory after FAST                 │ │
│  │  busy() check - prevents buffer modification during update               │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                        │
│                                      ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────────┐ │
│  │                         E-INK DISPLAY (200x200)                          │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

### 1. Event Flow
```
Meshtastic → Events.cpp → InkHUD2.onEvent() → Pipe.dispatchEvent() → Module.onEvent()
```

### 2. Input Flow
```
Button → InkHUD2.onInput() → Pipe.dispatchInput()
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
           SystemModule                      Slot Module
           (if handlesInput)                 (if no system handling)
```

### 3. Render Flow
```
InkHUD2.runOnce() → Pipe.needsUpdate()? → Pipe.render(ctx) → Driver.update()
                                                   │
                    ┌──────────────────────────────┤
                    ▼                              ▼
              Slot Modules              System Modules (by priority)
              (in clip regions)         (overlay on top)
```

---

## Screen Layout

```
┌────────────────────────────────────────────────────────────┐
│ StatusBar                                        [Battery] │ ← StatusBar + BatteryModule
├────────────────────────────────────────────────────────────┤
│ ╔════════════════════════════════════════════════════════╗ │
│ ║                                                        ║ │
│ ║                    Content Area                        ║ │ ← ContentArea
│ ║                                                        ║ │
│ ║   (Module renders here - messages, nodes, map, etc)    ║ │
│ ║                                                        ║ │
│ ╚════════════════════════════════════════════════════════╝ │
├────────────────────────────────────────────────────────────┤
│ Footer / Hint                                              │ ← Footer
└────────────────────────────────────────────────────────────┘
```

### Slot Layouts
```
┌──────────────────┐  ┌─────────┬─────────┐  ┌────────┬────────┐
│                  │  │         │         │  │   0    │   1    │
│     1 SLOT       │  │    0    │    1    │  ├────────┼────────┤
│                  │  │         │         │  │   2    │   3    │
└──────────────────┘  └─────────┴─────────┘  └────────┴────────┘
```

---

## File Structure

```
InkHUD2/
├── InkHUD2.h/cpp           # Singleton orchestrator
├── Events.h/cpp            # Meshtastic event bridge
│
├── Core/
│   ├── Buffer.h            # 1-bit frame buffer
│   ├── Font.h/cpp          # CJK + ASCII glyph lookup
│   ├── Layout.h            # All UI constants & scales
│   ├── RenderContext.h/cpp # Drawing API (composition)
│   ├── Logo.h/cpp          # Meshtastic logo
│   └── Settings.h          # Persistent settings
│
├── Text/
│   └── TextRenderer.h/cpp  # Clip-aware text rendering
│
├── UI/                     # SHARED UI COMPONENTS
│   ├── StatusBar.h/cpp     # Header with icon + title
│   ├── Footer.h/cpp        # Bottom hint bar
│   ├── ContentArea.h       # Content bounds calculation
│   ├── MenuItem.h          # Menu item struct & types
│   └── MenuList.h/cpp      # Reusable menu renderer
│
├── Pipe/
│   ├── Pipe.h/cpp          # Module management
│   └── Events.h            # Event/Input enums
│
├── Modules/
│   ├── Module.h/cpp        # Base class
│   ├── MessageModule       # Chat, DM views
│   ├── NodeListModule      # Node list
│   ├── MapModule           # 2D map + settings
│   ├── BatteryModule       # Battery overlay
│   ├── NotificationModule  # Popup notifications
│   ├── MenuModule          # Main menu + shutdown
│   └── BootModule          # Boot/pairing screens
│
├── Views/
│   ├── ChatView.h/cpp      # Channel messages
│   └── DMView.h/cpp        # Direct messages
│
├── Fonts/
│   └── UnifiedFont18px.h   # 2526 glyphs (CJK+ASCII+Cyrillic)
│
└── Drivers/
    └── EInkAdapter.h       # E-ink display interface
```

---

## Key Architectural Decisions

1. **Composition over Inheritance** - Modules receive `RenderContext`, not inherit from GFX
2. **Shared UI Components** - `StatusBar`, `Footer`, `ContentArea`, `MenuList` are reusable
3. **Separation of Concerns** - `Pipe` manages modules, `Events` bridges to Meshtastic
4. **E-ink Friendly** - FAST refresh by default, FULL only on explicit request
5. **Single Font** - CJK font 18px for all languages (2526 glyphs)
6. **Priority-based Overlays** - System modules render on top by priority order

---

## Module Types

### Regular Modules
- Render in slots (1, 2, or 4 slots)
- Receive clipped `RenderContext`
- Examples: `MessageModule`, `NodeListModule`, `MapModule`

### System Modules
- Overlay on top of regular modules
- Can lock rendering (full screen takeover)
- Can always render (like battery icon)
- Sorted by priority (higher = on top)
- Examples: `BatteryModule`, `MenuModule`, `BootModule`

---

## UI Components

### StatusBar
- Renders header with icon + title
- Icons: ENVELOPE, USERS, GEAR, INFO, MAP
- Returns content start Y position

### Footer
- Renders hint text at bottom
- Returns footer Y position

### ContentArea
- Calculates bounds between StatusBar and Footer
- Provides left/top/right/bottom helpers

### MenuList (NEW)
- Reusable menu item renderer
- Handles navigation (selectNext/selectPrev)
- Handles TOGGLE/VALUE activation
- Supports scrolling for long lists
- Used by: MenuModule, MapModule settings

### MenuItem
- Types: ACTION, TOGGLE, VALUE, SUBMENU, BACK, LABEL
- Stores label, type-specific data, onChange callback
