#pragma once

namespace UiStyle {
constexpr const char *kColorMainBg = "#121214"; // Even darker background
constexpr const char *kColorSidebarBg = "#1a1a1e"; // Slightly lighter sidebar
constexpr const char *kColorSectionBg = "#202026"; // Section background
constexpr const char *kColorTextMain = "#f0f0f3"; // Brighter text
constexpr const char *kColorTextDim = "#a8a8b0"; // Dim text
constexpr const char *kColorAccent = "#f0f0f3"; // Neutral white/gray accent
constexpr const char *kColorRed = "#ed4245";
constexpr const char *kColorOrange = "#faa61a";
constexpr const char *kColorGreen = "#3ba55c";
constexpr const char *kColorBlue = "#00b0f4";
constexpr const char *kColorYellow = "#fee75c";
constexpr const char *kColorActiveTab = "#e0e0e0"; // Light gray for active tab

// Layout Constants (Standardized based on VRAM/ProcessList tab)
constexpr int kPageMarginLeft = 12;
constexpr int kPageMarginTop = 8;
constexpr int kPageMarginRight = 12;
constexpr int kPageMarginBottom = 8;
constexpr int kPageSpacing = 6;

// Semantic Aliases for new UI
constexpr const char *kBackground = kColorMainBg;
constexpr const char *kSurface = kColorSectionBg;
constexpr const char *kBorder = "#2d2d34";
constexpr const char *kPrimary = kColorAccent;
constexpr const char *kTextPrimary = kColorTextMain;
constexpr const char *kTextSecondary = kColorTextDim;

constexpr const char *kTitle = "color: #ffffff; font-size: 14px; font-weight: 600; font-family: 'Segoe UI Variable Display', 'Segoe UI', sans-serif;";
constexpr const char *kSubtitle = "color: #a8a8b0; font-size: 11px; font-family: 'Segoe UI', sans-serif;";
constexpr const char *kDetail = "color: #f0f0f3; font-size: 12px; font-family: 'Segoe UI', sans-serif;";
constexpr const char *kDetailSmall = "color: #ffffff; font-size: 11px; font-family: 'Segoe UI';";
constexpr const char *kValueLarge = "color: #ffffff; font-size: 15px; font-weight: bold; font-family: 'Segoe UI';";
constexpr const char *kValueStrong = "color: #ffffff; font-size: 12px; font-weight: bold; font-family: 'Segoe UI';";
constexpr const char *kSection = "color: #8b8b94; font-size: 10px; font-family: 'Segoe UI';";
constexpr const char *kNote = "color: #8b8b94; font-size: 9px; font-family: 'Segoe UI';";
constexpr const char *kListItem = "color: #ffffff; font-size: 11px; font-family: 'Segoe UI';";
constexpr const char *kListItemMuted = "color: #8b8b94; font-size: 10px; font-family: 'Segoe UI';";
constexpr const char *kSidebarLabel = "color: #8b8b94; font-size: 9px; font-weight: bold; font-family: 'Segoe UI';";
constexpr const char *kSidebarValue = "color: #ffffff; font-size: 9px; font-weight: bold; font-family: 'Segoe UI';";

constexpr const char *kButton =
    "QPushButton { background: #202026; color: #f0f0f3; border-radius: 6px; padding: 6px 12px; border: 1px solid #2d2d34; }"
    "QPushButton:hover { background: #2d2d34; border-color: #5865f2; color: #ffffff; }"
    "QPushButton:pressed { background: #1a1a1e; border-color: #5865f2; }";

constexpr const char *kButtonIcon =
    "QPushButton { background: #202026; color: #f0f0f3; border-radius: 6px; padding: 4px; border: 1px solid #2d2d34; }"
    "QPushButton:hover { background: #2d2d34; border-color: #ffffff; color: #ffffff; }"
    "QPushButton:pressed { background: #1a1a1e; border-color: #ffffff; }";

constexpr const char *kButtonToggle =
    "QPushButton { background: #202026; color: #f0f0f3; border-radius: 6px; padding: 4px 6px; border: 1px solid #2d2d34; }"
    "QPushButton:hover { background: #2d2d34; border-color: #8b8b94; }"
    "QPushButton:checked { background: #32323a; color: #ffffff; border-color: #f0f0f3; }"
    "QPushButton:checked:hover { border-color: #ffffff; }";

constexpr const char *kButtonDanger =
    "QPushButton { background: #202026; color: #f0f0f3; border-radius: 6px; padding: 4px 6px; border: 1px solid #2d2d34; }"
    "QPushButton:hover { background: #2a2020; color: #ed4245; border-color: #ed4245; }"
    "QPushButton:pressed { background: #1a1a1e; }";

constexpr const char *kButtonPrimary =
    "QPushButton { background: #202026; color: #ffffff; border-radius: 6px; padding: 4px 10px; border: 1px solid #2d2d34; }"
    "QPushButton:hover { background: #2d2d34; border-color: #ffffff; }";

constexpr const char *kComboBox =
    "QComboBox { background: #202026; color: #f0f0f3; border-radius: 6px; padding: 4px 10px; border: 1px solid #2d2d34; }"
    "QComboBox:hover { border-color: #ffffff; }"
    "QComboBox::drop-down { border: none; width: 20px; }"; // Keep simple to retain default arrow

constexpr const char *kProgressBar =
    "QProgressBar { background: #2d2d34; border-radius: 4px; color: #ffffff; }"
    "QProgressBar::chunk { background: #3ba55c; border-radius: 4px; }";

constexpr const char *kLineEdit =
    "QLineEdit { background: #2d2d34; color: #ffffff; border: 1px solid #25252b; border-radius: 4px; padding: 4px 8px; }"
    "QLineEdit:focus { border-color: #ffffff; }";

constexpr const char *kCheckBox =
    "QCheckBox { color: #8b8b94; font-size: 10px; spacing: 4px; outline: none; }"
    "QCheckBox:focus { outline: none; }"
    "QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px; border: 1px solid #2d2d34; background: #202026; }"
    "QCheckBox::indicator:unchecked:hover { border-color: #ffffff; }"
    "QCheckBox::indicator:checked { background: #ffffff; border-color: #ffffff; image: url(:/icons/check.svg); }" 
    "QCheckBox::indicator:checked:hover { background: #e0e0e0; border-color: #e0e0e0; }";
}
