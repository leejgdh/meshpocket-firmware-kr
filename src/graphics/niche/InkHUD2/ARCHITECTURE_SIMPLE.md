# InkHUD2 - Simple Architecture Overview

```
╔═══════════════════════════════════════════════════════════════╗
║                    InkHUD2 - SIMPLE OVERVIEW                  ║
╚═══════════════════════════════════════════════════════════════╝


                         MESHTASTIC
                             │
            ┌────────────────┼────────────────┐
            │                │                │
         Events           Messages          Button
            │                │                │
            └────────────────┼────────────────┘
                             │
                             ▼
    ╔═══════════════════════════════════════════════════════════╗
    ║                       InkHUD2                             ║
    ║                    (main class)                           ║
    ╚═══════════════════════════════════════════════════════════╝
                             │
                             ▼
    ┌───────────────────────────────────────────────────────────┐
    │                         PIPE                              │
    │                (module management)                        │
    │                                                           │
    │   Slots (regular modules):     System (on top of all):   │
    │   ┌─────┐ ┌─────┐             ┌─────────┐ ┌─────────┐    │
    │   │Msg  │ │Nodes│ │Map│       │Battery  │ │Menu     │    │
    │   └─────┘ └─────┘             └─────────┘ └─────────┘    │
    └───────────────────────────────────────────────────────────┘
                             │
                             ▼
    ┌───────────────────────────────────────────────────────────┐
    │                    RENDER CONTEXT                         │
    │                     (drawing)                             │
    │                                                           │
    │   pixel, line, rect, circle, text, textWrapped, hatch    │
    └───────────────────────────────────────────────────────────┘
                             │
                             ▼
    ┌───────────────────────────────────────────────────────────┐
    │                    UI COMPONENTS                          │
    │                    (reusable)                             │
    │                                                           │
    │   StatusBar  │  ContentArea  │  Footer  │  MenuList      │
    └───────────────────────────────────────────────────────────┘
                             │
                             ▼
    ┌───────────────────────────────────────────────────────────┐
    │                     E-INK DISPLAY                         │
    │                       200×200                             │
    └───────────────────────────────────────────────────────────┘
```

---

## Module Screen Layout

```
    ┌─────────────────────────────────────────┬──────┐
    │  [Icon] Title                           │ Bat  │  ← StatusBar
    ├─────────────────────────────────────────┴──────┤
    │                                                │
    │                                                │
    │              Content Area                      │  ← Content
    │         (messages, nodes, map)                 │
    │                                                │
    │                                                │
    ├────────────────────────────────────────────────┤
    │  Click: next   Hold: select                    │  ← Footer
    └────────────────────────────────────────────────┘
```

---

## Modules

```
    REGULAR (in slots):          SYSTEM (overlays):
    ┌─────────────────┐          ┌─────────────────┐
    │ MessageModule   │          │ BatteryModule   │ always visible
    │ NodeListModule  │          │ MenuModule      │ full screen
    │ MapModule       │          │ BootModule      │ on startup
    └─────────────────┘          │ NotifyModule    │ popup
                                 └─────────────────┘
```

---

## Data Flow

```
    EVENTS:     Meshtastic → InkHUD2 → Pipe → All modules

    INPUT:      Button → InkHUD2 → Pipe → Active module

    RENDER:     Module.onRender(ctx) → Buffer → E-ink
```

---

## File Structure (Brief)

```
    InkHUD2/
    ├── InkHUD2.cpp      ← Main class
    ├── Core/            ← Buffer, Font, Layout, RenderContext
    ├── UI/              ← StatusBar, Footer, MenuList
    ├── Modules/         ← Message, NodeList, Map, Menu, Battery
    └── Pipe/            ← Module management
```

---

## Summary (4 Key Points)

1. **InkHUD2** — Entry point, bridges Meshtastic and display
2. **Pipe** — Dispatches events to modules, manages slots
3. **Modules** — Draw content via `RenderContext`
4. **UI/** — Shared components (StatusBar, MenuList) - no code duplication
