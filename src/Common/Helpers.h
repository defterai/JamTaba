#ifndef HELPERS_H
#define HELPERS_H

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QFuture>
#include <QFutureInterface>
#include <type_traits>

template<class T, class... Args>
std::enable_if_t<!std::is_base_of_v<QObject, T>, QSharedPointer<T>> createQSharedPointer(Args&&... arguments) {
    return QSharedPointer<T>::create(std::forward<Args>(arguments)...);
}

template<class T, class... Args>
std::enable_if_t<std::is_base_of_v<QObject, T>, QSharedPointer<T>> createQSharedPointer(Args&&... arguments) {
    return QSharedPointer<T>(new T(std::forward<Args>(arguments)...), &T::deleteLater);
}

using TaskCallback = std::function<void()>;

class TaskObject : public QObject
{
    Q_OBJECT

public:
    TaskObject();
    QFuture<void> postTask(TaskCallback&& callback);

signals:
    void postedTask(QFutureInterface<void> futureInterface, TaskCallback callback);

private slots:
    void processTask(QFutureInterface<void> futureInterface, TaskCallback callback);

};

inline TaskObject::TaskObject() {
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<QFutureInterface<void>>();
        qRegisterMetaType<TaskCallback>();
        registered = true;
    }

    connect(this, &TaskObject::postedTask, this, &TaskObject::processTask);
}

inline QFuture<void> TaskObject::postTask(TaskCallback&& callback) {
    QFutureInterface<void> futureInterface;
    futureInterface.reportStarted();
    emit postedTask(futureInterface, callback);
    return futureInterface.future();
}

inline void TaskObject::processTask(QFutureInterface<void> futureInterface, TaskCallback callback) {
    try {
        callback();
    } catch (...) {
        // todo
    }
    futureInterface.reportFinished();
}

Q_DECLARE_METATYPE(QFutureInterface<void>)
Q_DECLARE_METATYPE(TaskCallback)

#endif // HELPERS_H
