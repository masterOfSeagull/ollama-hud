#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

struct DetectorSettings
{
    bool enabled = false;
    QString model = "google/owlv2-base-patch16-ensemble";
    QString device = "auto";
    QString dtype = "auto";
    double textThreshold = 0.10;
    double guideThreshold = 0.10;
    QString targetsJson = R"([{"name":"Entrance","prompts":["entrance","portal","exit","door"]}])";
    QString positiveGuideFolder = "C:/projects/owlv2-visual-detector/assets/guides/positive";
    QString negativeGuideFolder = "C:/projects/owlv2-visual-detector/assets/guides/negative";
    double liveRate = 4.0;
};

class DetectorSettingsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY changed)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY changed)
    Q_PROPERTY(QString dtype READ dtype WRITE setDtype NOTIFY changed)
    Q_PROPERTY(double textThreshold READ textThreshold WRITE setTextThreshold NOTIFY changed)
    Q_PROPERTY(double guideThreshold READ guideThreshold WRITE setGuideThreshold NOTIFY changed)
    Q_PROPERTY(QString targetsJson READ targetsJson WRITE setTargetsJson NOTIFY changed)
    Q_PROPERTY(QVariantList targets READ targets NOTIFY changed)
    Q_PROPERTY(QStringList defaultGuideTargetNames READ defaultGuideTargetNames NOTIFY changed)
    Q_PROPERTY(QString positiveGuideFolder READ positiveGuideFolder WRITE setPositiveGuideFolder NOTIFY changed)
    Q_PROPERTY(QString negativeGuideFolder READ negativeGuideFolder WRITE setNegativeGuideFolder NOTIFY changed)
    Q_PROPERTY(double liveRate READ liveRate WRITE setLiveRate NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QUrl settingsFolder READ settingsFolder CONSTANT)
public:
    explicit DetectorSettingsStore(QObject *parent = nullptr);
    static QString configPath();
    QUrl settingsFolder() const;
    DetectorSettings settings() const;
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveToFile(const QString &path);
    Q_INVOKABLE bool loadFromFile(const QString &path);
    Q_INVOKABLE bool resetToDefaults();
    bool enabled() const; void setEnabled(bool value);
    QString model() const; void setModel(const QString &value);
    QString device() const; void setDevice(const QString &value);
    QString dtype() const; void setDtype(const QString &value);
    double textThreshold() const; void setTextThreshold(double value);
    double guideThreshold() const; void setGuideThreshold(double value);
    QString targetsJson() const; void setTargetsJson(const QString &value);
    QVariantList targets() const;
    QStringList defaultGuideTargetNames() const;
    Q_INVOKABLE int defaultGuideTargetIndex(const QString &name) const;
    QString positiveGuideFolder() const; void setPositiveGuideFolder(const QString &value);
    QString negativeGuideFolder() const; void setNegativeGuideFolder(const QString &value);
    double liveRate() const; void setLiveRate(double value);
    QString lastError() const;
    Q_INVOKABLE void addTextTarget();
    Q_INVOKABLE void addImageTarget();
    Q_INVOKABLE void removeTarget(int index);
    Q_INVOKABLE void updateTextTarget(int index, const QString &name, const QString &prompts);
    Q_INVOKABLE void updateImageTarget(int index, const QString &name, bool useDefaultGuideDirectories, const QString &positiveGuideDir, const QString &negativeGuideDir);
    Q_INVOKABLE void setImageTargetGuideFolder(int index, bool positive, const QString &folder);
signals: void changed(); void lastErrorChanged();
private:
    bool load(); void setLastError(const QString &value); static void validate(const DetectorSettings &settings);
    DetectorSettings m_settings; QString m_lastError;
};
