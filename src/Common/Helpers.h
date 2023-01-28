#ifndef HELPERS_H
#define HELPERS_H

#include <QScopedPointer>
#include <QSharedPointer>
#include <type_traits>

class QObject;

template<class T, class... Args>
std::enable_if_t<!std::is_base_of_v<QObject, T>, QSharedPointer<T>> createQSharedPointer(Args&&... arguments) {
    return QSharedPointer<T>::create(std::forward<Args>(arguments)...);
}

template<class T, class... Args>
std::enable_if_t<std::is_base_of_v<QObject, T>, QSharedPointer<T>> createQSharedPointer(Args&&... arguments) {
    return QSharedPointer<T>(new T(std::forward<Args>(arguments)...), &T::deleteLater);
}

#endif // HELPERS_H
