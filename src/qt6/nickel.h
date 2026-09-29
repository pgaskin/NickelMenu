#pragma once
#include <QVariant>
#include <cstddef>
#include <cstring>

namespace nm6 {
// Private ABI boundary for the two audited ARM32 firmware builds. Resolution
// never loads Nickel, and never accepts a definition from another library.
class Nickel {
public:
    Nickel();
    ~Nickel();
    Nickel(const Nickel &) = delete;
    Nickel &operator=(const Nickel &) = delete;
    explicit operator bool() const { return handle_ != nullptr; }
    template<class Function> Function resolve(const char *name) const {
        return reinterpret_cast<Function>(symbol(name, true));
    }
    void *vtable(const char *name) const { return symbol(name, false); }
private:
    void *symbol(const char *, bool function) const;
    void *handle_ = nullptr;
    void *base_ = nullptr;
};

// Stack storage with the audited layout and a non-deleting destructor. This
// is deliberately not a C++ declaration of a private Nickel class.
class NickelSettings {
public:
    explicit NickelSettings(const Nickel &, const char *vtable);
    ~NickelSettings();
    NickelSettings(const NickelSettings &) = delete;
    NickelSettings &operator=(const NickelSettings &) = delete;
    explicit operator bool() const { return ready_; }
    QVariant get(const QString &, const QVariant &fallback);
    void set(const QString &, const QVariant &);
    void *object();
private:
    alignas(void *) unsigned char storage_[44]{};
    void *base_vtable_ = nullptr;
    void *section_vtable_ = nullptr;
    void (*destroy_)(void *) = nullptr;
    QVariant (*get_)(void *, const QString &, const QVariant &) = nullptr;
    void (*set_)(void *, const QString &, const QVariant &, bool) = nullptr;
    bool ready_ = false;
};
}
