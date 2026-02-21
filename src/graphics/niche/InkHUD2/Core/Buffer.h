#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace InkHUD2 {

enum class Color : uint8_t {
    BLACK = 0,  // E-ink: 0 = ink on = black
    WHITE = 1   // E-ink: 1 = no ink = white
};

struct Rect {
    int16_t x, y;
    uint16_t w, h;

    bool contains(int16_t px, int16_t py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    Rect inset(int16_t amount) const {
        // Guard against underflow when amount is too large
        uint16_t newW = (2 * amount >= w) ? 0 : static_cast<uint16_t>(w - 2 * amount);
        uint16_t newH = (2 * amount >= h) ? 0 : static_cast<uint16_t>(h - 2 * amount);
        return {
            static_cast<int16_t>(x + amount),
            static_cast<int16_t>(y + amount),
            newW,
            newH
        };
    }
};

class Buffer {
public:
    // rotation: 0=normal, 1=90CW, 2=180, 3=270CW (same as InkHUD)
    Buffer(uint16_t driverWidth, uint16_t driverHeight, uint8_t rotation = 0)
        : drvW(driverWidth), drvH(driverHeight), rot(rotation)
    {
        // Buffer format matches e-ink driver expectations:
        // - Each row packed into bytes (8 pixels per byte)
        // - MSB is leftmost pixel
        rowBytes = ((drvW - 1) / 8) + 1;
        size_t byteCount = static_cast<size_t>(rowBytes) * drvH;
        data = static_cast<uint8_t*>(malloc(byteCount));
        if (data) {
            clear();
        } else {
            drvW = 0;
            drvH = 0;
            rowBytes = 0;
        }
    }

    // Check if buffer is valid (allocation succeeded)
    bool isValid() const { return data != nullptr; }

    ~Buffer() {
        if (data) {
            free(data);
            data = nullptr;
        }
    }

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Movable
    Buffer(Buffer&& other) noexcept
        : data(other.data), drvW(other.drvW), drvH(other.drvH),
          rowBytes(other.rowBytes), rot(other.rot)
    {
        other.data = nullptr;
        other.drvW = other.drvH = other.rowBytes = 0;
    }

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            free(data);
            data = other.data;
            drvW = other.drvW;
            drvH = other.drvH;
            rowBytes = other.rowBytes;
            rot = other.rot;
            other.data = nullptr;
            other.drvW = other.drvH = other.rowBytes = 0;
        }
        return *this;
    }

    void setPixel(int16_t x, int16_t y, Color c) {
        // Check bounds against logical dimensions
        uint16_t logW = width();
        uint16_t logH = height();
        if (x < 0 || x >= logW || y < 0 || y >= logH) return;

        // Apply rotation to get physical coordinates
        int16_t px, py;
        applyRotation(x, y, &px, &py);

        // Write to buffer in driver format (MSB = leftmost pixel)
        size_t byteNum = static_cast<size_t>(py) * rowBytes + (px / 8);
        uint8_t bitNum = 7 - (px % 8);  // MSB is leftmost pixel

        // E-ink: bit 0 = black (ink on), bit 1 = white (no ink)
        if (c == Color::BLACK) {
            data[byteNum] &= ~(1 << bitNum);  // Clear bit for BLACK
        } else {
            data[byteNum] |= (1 << bitNum);   // Set bit for WHITE
        }
    }

    Color getPixel(int16_t x, int16_t y) const {
        uint16_t logW = width();
        uint16_t logH = height();
        if (x < 0 || x >= logW || y < 0 || y >= logH) return Color::WHITE;

        int16_t px, py;
        applyRotation(x, y, &px, &py);

        size_t byteNum = static_cast<size_t>(py) * rowBytes + (px / 8);
        uint8_t bitNum = 7 - (px % 8);

        return (data[byteNum] & (1 << bitNum)) ? Color::WHITE : Color::BLACK;
    }

    void clear() {
        if (data) {
            size_t byteCount = static_cast<size_t>(rowBytes) * drvH;
            memset(data, 0xFF, byteCount);  // All white (all bits = 1)
        }
    }

    void clearRect(Rect r) {
        for (int16_t py = r.y; py < r.y + r.h && py < height(); ++py) {
            for (int16_t px = r.x; px < r.x + r.w && px < width(); ++px) {
                if (px >= 0 && py >= 0) {
                    setPixel(px, py, Color::WHITE);
                }
            }
        }
    }

    void fill(Color c) {
        if (data) {
            size_t byteCount = static_cast<size_t>(rowBytes) * drvH;
            memset(data, c == Color::BLACK ? 0x00 : 0xFF, byteCount);
        }
    }

    uint8_t* getData() { return data; }
    const uint8_t* getData() const { return data; }

    // Logical dimensions (after rotation)
    uint16_t width() const { return (rot % 2) ? drvH : drvW; }
    uint16_t height() const { return (rot % 2) ? drvW : drvH; }

    // Physical driver dimensions
    uint16_t driverWidth() const { return drvW; }
    uint16_t driverHeight() const { return drvH; }

    void setRotation(uint8_t r) { rot = r % 4; }
    uint8_t rotation() const { return rot; }

private:
    // Convert logical coordinates to physical driver coordinates
    void applyRotation(int16_t x, int16_t y, int16_t* px, int16_t* py) const {
        switch (rot) {
        case 1:  // 90 CW
            *px = (drvW - 1) - y;
            *py = x;
            break;
        case 2:  // 180
            *px = (drvW - 1) - x;
            *py = (drvH - 1) - y;
            break;
        case 3:  // 270 CW (same as 90 CCW)
            *px = y;
            *py = (drvH - 1) - x;
            break;
        default:  // case 0: no rotation
            *px = x;
            *py = y;
            break;
        }
    }

    uint8_t* data = nullptr;
    uint16_t drvW = 0;      // Physical driver width
    uint16_t drvH = 0;      // Physical driver height
    uint16_t rowBytes = 0;  // Bytes per row in driver format
    uint8_t rot = 0;        // Rotation: 0, 1, 2, 3
};

} // namespace InkHUD2
