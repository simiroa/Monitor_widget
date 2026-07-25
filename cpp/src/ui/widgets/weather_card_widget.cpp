#include "ui/widgets/weather_card_widget.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTime>
#include <QTimer>
#include <QShowEvent>
#include <QtMath>
#include <QDateTime>

namespace {
bool extractNumber(const QString &text, double *out) {
    QString numeric;
    bool has_digit = false;
    for (const QChar &ch : text) {
        if (ch.isDigit() || ch == '.' || (ch == '-' && numeric.isEmpty())) {
            numeric.append(ch);
            if (ch.isDigit()) {
                has_digit = true;
            }
        }
    }
    if (!has_digit) {
        return false;
    }
    bool ok = false;
    double value = numeric.toDouble(&ok);
    if (!ok) {
        return false;
    }
    *out = value;
    return true;
}

bool isZeroPrecip(const QString &text) {
    if (text.isEmpty()) {
        return true;
    }
    if (text.contains("강수없음") || text.contains("없음") || text == "-" || text == "0" || text == "0mm") {
        return true;
    }
    return false;
}
} // namespace

// Helper for Extra Grid Items (Simplified Text-Only Layout)
static QLabel* createExtraItem(QWidget *parent, const QString &label, int row, int col, QGridLayout *grid) {
    auto *container = new QWidget(parent);
    container->setStyleSheet("background: transparent; border: none;");
    
    auto *vbox = new QVBoxLayout(container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    vbox->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    
    auto *name_lbl = new QLabel(label, container);
    // Name at the top in bright white
    name_lbl->setStyleSheet("font-family: 'Inter'; font-size: 8px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.5px; color: #FFFFFF;");
    name_lbl->setAlignment(Qt::AlignCenter);

    auto *val_lbl = new QLabel("-", container);
    val_lbl->setStyleSheet(QString("font-family: 'Inter'; font-size: 10px; font-weight: 700; color: %1;").arg(UiStyle::kColorTextMain));
    val_lbl->setAlignment(Qt::AlignCenter);
    
    vbox->addWidget(name_lbl);
    vbox->addWidget(val_lbl);
    
    grid->addWidget(container, row, col);
    return val_lbl;
}

WeatherCardWidget::WeatherCardWidget(QWidget *parent) : QWidget(parent) {
    setObjectName("weatherCard");
    setupUi();
}

void WeatherCardWidget::setupUi() {
    // Base card style
    setStyleSheet(QString(
        "QWidget#weatherCard { "
        "   background-color: %1; "
        "   border-radius: 6px; "
        "   border: 1px solid %2; "
        "} "
        "QLabel { background: transparent; border: none; }"
    ).arg(UiStyle::kColorSectionBg).arg(UiStyle::kBorder));
    
    auto *card_layout = new QVBoxLayout(this);
    card_layout->setContentsMargins(12, 10, 12, 10);
    card_layout->setSpacing(4);

    // Row 1: Location
    auto *loc_layout = new QHBoxLayout();
    
    loc_btn_ = new QPushButton("Seoul", this);
    loc_btn_->setObjectName("locBtn");
    loc_btn_->setStyleSheet(
        "QPushButton#locBtn { "
        "   background: transparent; color: #ffffff; border: none; font-size: 11px; font-weight: 700; "
        "   text-align: left; padding: 0;"
        "} "
        "QPushButton#locBtn:hover { color: #cccccc; }"
    );
    connect(loc_btn_, &QPushButton::clicked, this, &WeatherCardWidget::locationClicked);

    loc_layout->addWidget(loc_btn_);
    loc_layout->addStretch();
    card_layout->addLayout(loc_layout);

    // Row 2: Main Info (Left) + Status/Warning (Right)
    auto *middle_layout = new QHBoxLayout();
    middle_layout->setSpacing(0);
    
    // --- Left: Weather Icon + Temp ---
    auto *weather_info_box = new QWidget(this);
    auto *weather_info_layout = new QHBoxLayout(weather_info_box);
    weather_info_layout->setContentsMargins(0, 0, 0, 0);
    weather_info_layout->setSpacing(10);
    weather_info_layout->setAlignment(Qt::AlignLeft);
    
    icon_label_ = new QLabel(this);
    icon_label_->setStyleSheet(QString("font-family: '%1'; font-size: 42px; color: #FFD700;").arg(Icons::fontName()));
    icon_label_->setAlignment(Qt::AlignCenter);

    auto *temp_cond_layout = new QVBoxLayout();
    temp_cond_layout->setSpacing(0);
    temp_cond_layout->setAlignment(Qt::AlignVCenter);
    
    temp_label_ = new QLabel("--°", this);
    temp_label_->setStyleSheet(QString("font-family: 'Inter'; font-size: 32px; font-weight: 800; color: %1;").arg(UiStyle::kColorTextMain));
    
    cond_label_ = new QLabel("Loading...", this);
    cond_label_->setStyleSheet(QString("font-family: 'Outfit'; font-size: 12px; font-weight: 600; color: %1;").arg(UiStyle::kColorTextDim));
    
    temp_cond_layout->addWidget(temp_label_);
    temp_cond_layout->addWidget(cond_label_);
    
    weather_info_layout->addWidget(icon_label_);
    weather_info_layout->addLayout(temp_cond_layout);
    
    middle_layout->addWidget(weather_info_box);
    middle_layout->addStretch();
    
    // --- Right: Status Box (Warning / Good) ---
    right_status_box_ = new QWidget(this);
    auto *status_layout = new QVBoxLayout(right_status_box_);
    status_layout->setContentsMargins(0, 0, 0, 0);
    status_layout->setSpacing(2);
    status_layout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    right_status_icon_ = new QLabel(this);
    right_status_icon_->setAlignment(Qt::AlignCenter);
    right_status_icon_->setStyleSheet(QString("font-family: '%1'; font-size: 24px; color: #FFD700;").arg(Icons::fontName()));
    
    right_status_text_ = new QLabel(this);
    right_status_text_->setAlignment(Qt::AlignCenter);
    right_status_text_->setStyleSheet(QString("font-family: 'Outfit'; font-size: 9px; font-weight: 600; color: %1;").arg(UiStyle::kColorTextDim));
    
    status_layout->addWidget(right_status_icon_);
    status_layout->addWidget(right_status_text_);
    
    middle_layout->addWidget(right_status_box_);

    card_layout->addLayout(middle_layout);
    
    // Divider
    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QString("color: %1;").arg(UiStyle::kBorder));
    divider->setFixedHeight(1);
    card_layout->addWidget(divider);

    // Row 3: Extra Grid
    auto *extra_grid = new QGridLayout();
    extra_grid->setContentsMargins(0, 4, 0, 0);
    extra_grid->setSpacing(10);
    
    wind_value_     = createExtraItem(this, "Wind", 0, 0, extra_grid);
    humidity_value_ = createExtraItem(this, "Humid", 0, 1, extra_grid); 
    precip_value_   = createExtraItem(this, "Rain", 0, 2, extra_grid); 
    dust_value_     = createExtraItem(this, "Dust", 0, 3, extra_grid);

    card_layout->addLayout(extra_grid);
    
    // Timer for background update
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60000); // 1 min
    connect(refresh_timer_, &QTimer::timeout, this, &WeatherCardWidget::updateBackground);
}

void WeatherCardWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    updateBackground();
    if (refresh_timer_ && !refresh_timer_->isActive()) refresh_timer_->start();
}

void WeatherCardWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (refresh_timer_) refresh_timer_->stop();
}

QString WeatherCardWidget::getLocationName() const {
    return loc_btn_->text();
}

void WeatherCardWidget::updateData(const WeatherData &data) {
    // 1. Basic Info
    temp_label_->setText(data.temp_current.isEmpty() ? "--°" : data.temp_current);
    cond_label_->setText(data.sky_code.isEmpty() ? "No Data" : data.sky_code);
    loc_btn_->setText(data.location.isEmpty() ? "Seoul" : data.location);

    // 2. Theme Color
    QString themeColor = "#FFD700"; // Default Gold (Sunny)
    if (data.sky_code.contains("Rain")) {
        themeColor = "#0047AB"; // Cobalt Blue (Rain)
    } else if (data.sky_code.contains("Snow")) {
        themeColor = "#6699CC"; // Blue-Gray (Snow)
    } else if (data.sky_code.contains("Cloudy") || data.sky_code.contains("Overcast")) {
        themeColor = "#9CA3AF"; // Gray (Cloudy)
    }

    if (!data.icon_code.isEmpty()) {
        icon_label_->setText(data.icon_code);
        icon_label_->setStyleSheet(QString("font-family: '%1'; font-size: 42px; color: %2;").arg(Icons::fontName()).arg(themeColor));
    }
    cond_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 600; font-family: 'Outfit';").arg(themeColor));

    // Refactored: Update Logic (Wind, Humid, Precip, Dust)
    
    // Wind
    double windVal = 0.0;
    QString windText = data.wind_speed.trimmed();
    QString windColor = UiStyle::kColorTextMain;
    if (!windText.isEmpty() && extractNumber(windText, &windVal)) {
        wind_value_->setText(windText);
        windColor = getWindColor(static_cast<float>(windVal));
    } else {
        wind_value_->setText("-");
    }
    wind_value_->setStyleSheet(QString("font-family: 'Inter'; font-size: 10px; font-weight: 700; color: %1;").arg(windColor));
    
    // Humidity
    double humidVal = 0.0;
    QString humidText = data.humidity.trimmed();
    QString humidColor = UiStyle::kColorTextMain;
    if (!humidText.isEmpty() && extractNumber(humidText, &humidVal)) {
        humidity_value_->setText(humidText);
        humidColor = getHumidColor(static_cast<float>(humidVal));
    } else {
        humidity_value_->setText("-");
    }
    humidity_value_->setStyleSheet(QString("font-family: 'Inter'; font-size: 10px; font-weight: 700; color: %1;").arg(humidColor));

    // Precip
    QString precipC = "#9CA3AF";
    QString precipAmount = data.precipitation_amount.trimmed();
    QString precipProb = data.precipitation_prob.trimmed();
    double precipVal = 0.0;
    bool hasAmount = !isZeroPrecip(precipAmount) && extractNumber(precipAmount, &precipVal);
    
    QString precipTextVal = "-";
    if (hasAmount) {
        precipTextVal = precipAmount;
        precipC = getPrecipColor(static_cast<float>(precipVal), false);
    } else if (!precipProb.isEmpty() && extractNumber(precipProb, &precipVal)) {
        precipTextVal = precipProb;
        precipC = getPrecipColor(static_cast<float>(precipVal), true);
    } else {
        precipTextVal = "-";
        precipC = UiStyle::kColorTextDim;
    }
    precip_value_->setText(precipTextVal);
    precip_value_->setStyleSheet(QString("font-family: 'Inter'; font-size: 10px; font-weight: 700; color: %1;").arg(precipC));
    
    // Dust
    QString dustText = data.fine_dust;
    QString ultraText = data.ultra_fine_dust;
    
    // Simplify text for Card
    auto simplify = [](const QString &full) {
        if (full.isEmpty() || full == "-") return "-";
        if (full.contains("좋음")) return "Good";
        if (full.contains("보통")) return "Normal";
        if (full.contains("나쁨") && !full.contains("매우")) return "Bad";
        if (full.contains("매우")) return "V.Bad";
        if (full.contains("Good")) return "Good";
        if (full.contains("Normal")) return "Normal";
        if (full.contains("Bad") && full.contains("Very")) return "V.Bad";
        if (full.contains("Bad")) return "Bad";
        return "-";
    };
    
    QString pm10C = getDustColor(data.fine_dust);
    QString pm25C = getDustColor(data.ultra_fine_dust);
    
    QString pm10Html = QString("<font color='%1'>PM10: %2</font>").arg(pm10C).arg(simplify(dustText));
    QString pm25Html = QString("<font color='%1'>PM2.5: %2</font>").arg(pm25C).arg(simplify(ultraText));
    
    dust_value_->setText(pm10Html + "<br>" + pm25Html);
    dust_value_->setStyleSheet("font-family: 'Inter'; font-size: 9px; font-weight: 600;");

    // 5. Right Status Logic
    bool hasWarning = false;
    QString warnIcon;
    QString warnText;
    QString warnColor = "#9CA3AF"; // Default gray

    for (const QString &warn : data.active_warnings) {
        if (warn.contains("폭염")) { 
            warnIcon = QString::fromUtf16(Icons::kWeatherSunny); 
            warnText = "Heat Warning"; 
            warnColor = "#F44336"; 
            hasWarning = true; break; 
        }
        if (warn.contains("한파")) { 
            warnIcon = QString::fromUtf16(Icons::kWeatherSnowflake); 
            warnText = "Freeze Warning"; 
            warnColor = "#00E5FF"; 
            hasWarning = true; break; 
        }
        if (warn.contains("황사") || warn.contains("미세먼지")) { 
            warnIcon = QString::fromUtf16(Icons::kWeatherDust); 
            warnText = "Poor Air"; 
            warnColor = "#D7CCC8"; 
            hasWarning = true; break; 
        }
    }
    
    if (!hasWarning) {
        QString tempStr = data.temp_current;
        double tempVal = 0.0;
        bool hasTemp = extractNumber(tempStr, &tempVal);
        if (hasTemp && tempVal >= 33.0) { 
            warnIcon = QString::fromUtf16(Icons::kWeatherSunny); 
            warnText = "Heat Advisory"; 
            warnColor = "#F44336"; 
            hasWarning = true; 
        }
        else if (hasTemp && tempVal <= -12.0) { 
            warnIcon = QString::fromUtf16(Icons::kWeatherSnowflake); 
            warnText = "Freeze Warning"; 
            warnColor = "#00E5FF"; 
            hasWarning = true; 
        }
        else if (data.fine_dust.contains("Bad") || data.fine_dust.contains("Very")) {
            warnIcon = QString::fromUtf16(Icons::kWeatherDust); 
            warnText = "Poor Air"; 
            warnColor = "#D7CCC8"; 
            hasWarning = true; 
        }
    }

    if (hasWarning) {
        right_status_icon_->setText(warnIcon);
        right_status_icon_->setStyleSheet(QString("font-family: '%1'; font-size: 26px; color: %2;").arg(Icons::fontName()).arg(warnColor));
        right_status_text_->setText(warnText);
        right_status_text_->setStyleSheet(QString("font-family: 'Outfit'; font-size: 9px; font-weight: 600; color: %1;").arg(warnColor));
        right_status_box_->show();
    } else {
        right_status_box_->hide();
    }

    current_theme_hex_ = themeColor; 
    
    if (!data.sunrise_time.isEmpty()) sunrise_time_ = QTime::fromString(data.sunrise_time, "HH:mm");
    if (!data.sunset_time.isEmpty()) sunset_time_ = QTime::fromString(data.sunset_time, "HH:mm");
    
    updateBackground();
}

void WeatherCardWidget::updateBackground() {
    QTime now = QTime::currentTime();
    
    double progress = 0.0;
    bool isNight = false;
    
    int srMins = sunrise_time_.hour() * 60 + sunrise_time_.minute();
    int ssMins = sunset_time_.hour() * 60 + sunset_time_.minute();
    int nowMins = now.hour() * 60 + now.minute();
    
    if (ssMins <= srMins || nowMins < srMins || nowMins > ssMins) {
        isNight = true;
    } else {
        progress = (double)(nowMins - srMins) / (double)(ssMins - srMins);
        progress = qBound(0.0, progress, 1.0);
    }
    
    double cx = 0.5;
    double cy = 0.5;
    if (!isNight) {
        const double hour12 = fmod(now.hour() + (now.minute() / 60.0), 12.0);
        const double angleDeg = (hour12 / 12.0) * 360.0 - 90.0;
        const double angleRad = qDegreesToRadians(angleDeg);
        const double radius = 0.33;
        cx = 0.5 + radius * qCos(angleRad);
        cy = 0.5 + radius * qSin(angleRad);
    }

    QString style;
    
    if (isNight) {
        style = QString(
            "QWidget#weatherCard { "
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
            "       stop:0 #1a237e, stop:1 #000000); " 
            "   border-radius: 6px; "
            "   border: 1px solid %1; "
            "} "
        ).arg(UiStyle::kBorder);
    } else {
        QString skyColor = current_theme_hex_; 
        QString baseColor = UiStyle::kColorSectionBg; 
        
        const double glowStrength = qSin(M_PI * progress);
        const int coreAlpha = 60 + static_cast<int>(120 * glowStrength);
        const int midAlpha = 30 + static_cast<int>(80 * glowStrength);

        style = QString(
            "QWidget#weatherCard { "
            "   background: qradialgradient(spread:pad, cx:%1, cy:%2, radius:1.2, fx:%1, fy:%2, "
            "       stop:0 rgba(255, 255, 220, %3), "  
            "       stop:0.3 rgba(255, 255, 255, %4), " 
            "       stop:0.45 %5, "                     
            "       stop:1 %6); "                       
            "   border-radius: 6px; "
            "   border: 1px solid %7; "
            "} "
        ).arg(cx).arg(cy).arg(coreAlpha).arg(midAlpha).arg(skyColor).arg(baseColor).arg(UiStyle::kBorder);
    }

    setStyleSheet(style + "QLabel { background: transparent; border: none; }");
}

// ----------------------------------------------------------------------------
// Private Helper Implementation
// ----------------------------------------------------------------------------

QString WeatherCardWidget::getWindColor(float val) const {
    if (val < 5.0f) return "#66BB6A"; // Green
    if (val < 10.0f) return "#FFCA28"; // Yellow
    if (val < 14.0f) return "#FFA726"; // Orange
    return "#EF5350"; // Red
}

QString WeatherCardWidget::getHumidColor(float val) const {
    if (val < 30.0f) return "#EF5350"; // Red (Too dry)
    if (val < 40.0f) return "#FFA726"; // Orange
    if (val <= 60.0f) return "#66BB6A"; // Green (Optimal)
    if (val < 70.0f) return "#FFA726"; // Orange
    return "#EF5350"; // Red (Too humid)
}

QString WeatherCardWidget::getPrecipColor(float val, bool isProb) const {
    if (val <= 0.0f) return "#9CA3AF"; // Gray (None)
    if (isProb) {
            if (val < 30.0f) return "#66BB6A";
            if (val < 60.0f) return "#FFCA28";
            return "#42A5F5"; // Blue (High prob)
    }
    // Amount (mm)
    if (val < 5.0f) return "#42A5F5"; // Light Blue
    if (val < 20.0f) return "#1976D2"; // Blue
    return "#EF5350"; // Red (Heavy)
}

QString WeatherCardWidget::getDustColor(const QString &val) const {
    if (val.contains("Good") || val.contains("좋음")) return "#66BB6A";
    if (val.contains("Normal") || val.contains("보통")) return "#FFCA28";
    if (val.contains("Bad") || val.contains("나쁨")) return "#FFA726";
    if (val.contains("Very") || val.contains("매우")) return "#EF5350";
    return "#9CA3AF"; // N/A
}
