#include "Theme.h"

namespace mdmate {

AppTheme g_theme = AppTheme::Light;

namespace {

constexpr ThemeColors kLightTheme{
    RGB(255, 255, 255),
    RGB(20, 20, 20),
    RGB(255, 255, 255),
    RGB(30, 30, 30),
    {RGB(15, 15, 15), RGB(20, 20, 20), RGB(28, 28, 28), RGB(35, 35, 35), RGB(45, 45, 45), RGB(55, 55, 55)},
    RGB(95, 95, 95),
    RGB(120, 40, 100),
    RGB(30, 30, 30),
    RGB(150, 150, 150),
    RGB(20, 90, 200),
    RGB(170, 40, 110),
};

constexpr ThemeColors kDarkTheme{
    RGB(30, 30, 30),
    RGB(225, 225, 225),
    RGB(30, 30, 30),
    RGB(215, 215, 215),
    {RGB(255, 255, 255), RGB(240, 240, 240), RGB(225, 225, 225), RGB(210, 210, 210), RGB(195, 195, 195),
     RGB(180, 180, 180)},
    RGB(170, 170, 170),
    RGB(230, 150, 80),
    RGB(210, 210, 210),
    RGB(110, 110, 110),
    RGB(110, 170, 255),
    RGB(240, 150, 190),
};

constexpr ThemeColors kPixelTheme{
    RGB(210, 206, 197),
    RGB(40, 38, 35),
    RGB(210, 206, 197),
    RGB(50, 48, 45),
    {RGB(25, 60, 95), RGB(30, 70, 108), RGB(35, 80, 120), RGB(45, 90, 130), RGB(55, 100, 138), RGB(65, 108, 145)},
    RGB(120, 108, 92),
    RGB(165, 75, 20),
    RGB(50, 48, 45),
    RGB(160, 152, 138),
    RGB(40, 105, 165),
    RGB(180, 95, 30),
};

}

const ThemeColors& CurrentTheme() {
    switch (g_theme) {
        case AppTheme::Dark:
            return kDarkTheme;
        case AppTheme::Pixel:
            return kPixelTheme;
        default:
            return kLightTheme;
    }
}

}
