#include <assimp/ProgressHandler.hpp>
#include <iostream>
#include <QMetaObject>
#include <QObject>
#include <QProgressBar>
#include <QThread>

class AssimpProgressHandler : public QObject, public Assimp::ProgressHandler {
	Q_OBJECT
public:

    // Required by Qt system.
    inline void* operator new(size_t, void* ptr) noexcept
    {
        return ptr;
    }

    using Assimp::ProgressHandler::operator new;

    bool Update(float percentage) override {		
        emit progressChanged(percentage);
        return true;
    }

signals:
    void progressChanged(float percentage);
};