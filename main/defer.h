#ifndef _DEFER_H_
#define _DEFER_H_

#include <functional>

class Defer {
   public:
    Defer(std::function<void()> cb) : cb(cb) {}

    ~Defer() { cb(); }

   private:
    std::function<void()> cb;

   private:
    Defer(const Defer&) = delete;
    Defer(Defer&&) = delete;
    Defer& operator=(const Defer&) = delete;
    Defer& operator=(Defer&&) = delete;
};

#endif  // _DEFER_H_
