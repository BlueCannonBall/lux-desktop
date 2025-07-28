#ifndef _GLIB_HPP
#define _GLIB_HPP

#include <glib-object.h>
#include <ostream>
#include <string>
#include <utility>
#include <type_traits>

namespace glib {
    template <typename... Args, typename T, typename F>
    unsigned long connect_signal(T* object, const std::string& signal_name, F&& handler) {
        GClosure* closure = g_cclosure_new(
            G_CALLBACK(+[](T* object, Args... args, void* handler) {
                return (*(F*) handler)(object, args...);
            }),
            new std::decay_t<F>(std::forward<F>(handler)),
            [](void* data, GClosure*) {
                delete (F*) data;
            });
        return g_signal_connect_closure(object, signal_name.c_str(), closure, FALSE);
    }

    template <typename T>
    class Object {
    protected:
        T* object = nullptr;

    public:
        Object() = default;
        Object(T* object):
            object(object) {}
        Object(const Object& object) {
            *this = object;
        }
        Object(Object&& object) {
            *this = std::move(object);
        }

        Object& operator=(const Object& object) {
            if (this != &object && this->object != object.object) {
                if (this->object) g_object_unref(this->object);
                if (object.object) g_object_ref(this->object = object.object);
            }
            return *this;
        }

        Object& operator=(Object&& object) {
            if (this != &object) {
                if (this->object != object.object) {
                    if (this->object) g_object_unref(this->object);
                    this->object = object.object;
                }
                object.object = nullptr;
            }
            return *this;
        }

        ~Object() {
            if (object) g_object_unref(object);
        }

        template <typename... Args, typename F>
        unsigned long connect_signal(const std::string& signal_name, F&& handler) {
            return glib::connect_signal<Args...>(object, signal_name, std::forward<F>(handler));
        }

        T* get() const {
            return object;
        }

        const T& operator*() const {
            return *object;
        }

        T& operator*() {
            return *object;
        }

        const T* operator->() const {
            return object;
        }

        T* operator->() {
            return object;
        }

        operator bool() const {
            return object;
        }

        bool operator==(const T& other_object) const {
            return object == other_object.object;
        }

        bool operator!=(const T& other_object) const {
            return object != other_object.object;
        }

        friend std::ostream& operator<<(std::ostream& os, const Object& object) {
            return os << object.get();
        }

        void reset() {
            if (object) g_object_unref(object);
            object = nullptr;
        }

        // It is assumed that the new object is not currently managed by any other glib::Object
        void reset(T* object) {
            if (this->object != object) {
                if (this->object) g_object_unref(this->object);
                this->object = object;
            }
        }

        T* release() {
            return std::exchange(object, nullptr);
        }
    };
} // namespace glib

#endif
