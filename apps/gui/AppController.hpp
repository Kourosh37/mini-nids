#pragma once
#include "DataModel.hpp"
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>
#include <QVariantMap>
#include <mininids/capture/IPacketSource.hpp>
#include <mininids/capture/ZipArchive.hpp>
#include <mininids/engine/EngineConfig.hpp>

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString inputFile READ inputFile WRITE setInputFile NOTIFY changed)
    Q_PROPERTY(QString outputRoot READ outputRoot WRITE setOutputRoot NOTIFY changed)
    Q_PROPERTY(QString reportPath READ reportPath NOTIFY changed)
    Q_PROPERTY(QString groundTruth READ groundTruth WRITE setGroundTruth NOTIFY changed)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(QVariantMap evaluation READ evaluation NOTIFY changed)
    Q_PROPERTY(QVariantList timeline READ timeline NOTIFY changed)
    Q_PROPERTY(QVariantList protocols READ protocols NOTIFY changed)
    Q_PROPERTY(QString configText READ configText NOTIFY changed)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY changed)
    Q_PROPERTY(QVariantList archiveEntries READ archiveEntries NOTIFY changed)
    Q_PROPERTY(bool archiveInput READ archiveInput NOTIFY changed)
    Q_PROPERTY(bool canAnalyze READ canAnalyze NOTIFY changed)
    Q_PROPERTY(int preparedEntry READ preparedEntry NOTIFY changed)
  public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;
    bool busy() const {
        return busy_;
    }
    double progress() const {
        return progress_;
    }
    QString status() const {
        return status_;
    }
    QString inputFile() const {
        return input_;
    }
    QString outputRoot() const {
        return output_root_;
    }
    QString reportPath() const {
        return report_path_;
    }
    QString groundTruth() const {
        return ground_truth_;
    }
    QVariantMap summary() const {
        return summary_;
    }
    QVariantMap evaluation() const {
        return evaluation_;
    }
    QVariantList timeline() const {
        return timeline_;
    }
    QVariantList protocols() const {
        return protocols_;
    }
    QString configText() const;
    bool dark() const {
        return dark_;
    }
    QStringList recentFiles() const {
        return recent_;
    }
    QVariantList archiveEntries() const {
        return archive_entries_;
    }
    bool archiveInput() const {
        return input_.endsWith(".zip", Qt::CaseInsensitive);
    }
    bool canAnalyze() const {
        return !busy_ && !input_.isEmpty() && (!archiveInput() || !prepared_path_.isEmpty());
    }
    int preparedEntry() const {
        return prepared_entry_;
    }
    void setInputFile(const QString &);
    void setOutputRoot(const QString &);
    void setGroundTruth(const QString &);
    void setDark(bool);
    Q_INVOKABLE void start();
    Q_INVOKABLE void prepareArchiveEntry(int row);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool applyConfig(const QString &);
    Q_INVOKABLE QString inspect(int page, int row) const;
    Q_INVOKABLE void openReports();
    DataModel packets, flows, alerts, dns;
    DataProxy packet_proxy, flow_proxy, alert_proxy, dns_proxy;
  signals:
    void changed();
    void analysisFinished();
    void importFinished();

  private:
    void launch(std::function<void()> task);
    void scanArchive();
    std::shared_ptr<mininids::ZipArchive> archive_;
    std::shared_ptr<QTemporaryDir> prepared_directory_, analyzed_directory_;
    QVariantList archive_entries_;
    QString prepared_path_;
    int prepared_entry_{-1};
    mininids::EngineConfig config_;
    mininids::CancellationToken cancel_{false};
    QThread *worker_{};
    bool busy_{}, dark_{true};
    double progress_{};
    QString status_{"Ready for offline analysis"}, input_, output_root_, report_path_, ground_truth_,
        analyzed_input_;
    QStringList recent_;
    QVariantMap summary_, evaluation_;
    QVariantList timeline_, protocols_;
    qint64 input_size_{}, input_modified_{};
};
