#include <assimp/ProgressHandler.hpp>
#include <QProgressBar>
#include <QMetaObject>
#include <QObject>
#include <QThread>

class QtAssimpProgressHandler : public QObject, public Assimp::ProgressHandler {
	Q_OBJECT
public:
    QtAssimpProgressHandler(QObject* receiver) {
        connect(this, &QtAssimpProgressHandler::progressChanged, receiver, [receiver](float percentage) {
            auto* progressBar = receiver->findChild<QProgressBar*>("progressBar");
            if (progressBar) {
                progressBar->setValue(static_cast<int>(percentage * 100));
            }
            });
    }

    bool Update(float percentage) override {
        emit progressChanged(percentage);
        return true;
    }

signals:
    void progressChanged(float percentage);
};