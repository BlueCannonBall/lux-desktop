#include <functional>
#include <glib-object.h>
#include <utility>

namespace glib {
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

        template <typename U>
        bool operator==(const U& other_object) const {
            return object == other_object.object;
        }

        template <typename U>
        bool operator!=(const U& other_object) const {
            return object != other_object.object;
        }
    };
} // namespace glib
