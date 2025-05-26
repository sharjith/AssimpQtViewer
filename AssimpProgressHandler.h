#include <assimp/ProgressHandler.hpp>
#include <QProgressBar>
#include <QMetaObject>
#include <QObject>
#include <QThread>
#include <iostream>

class AssimpProgressHandler : public QObject, public Assimp::ProgressHandler {
	Q_OBJECT
public:

    // Required by Qt system. TODO: Make sure it is fine
    inline void* operator new(size_t, void* ptr) noexcept
    {
        return ptr;
    }

    using Assimp::ProgressHandler::operator new;

    bool Update(float percentage) override {
		//std::cout << "Progress: " << percentage * 100 << "%" << std::endl;
        emit progressChanged(percentage);
        return true;
    }

signals:
    void progressChanged(float percentage);
};