#include <uv.h>
#include <stdlib.h>
#include <coroutine>
#include <iostream>
#include <memory>
#include <utility>
#include <chrono>
#include <assert.h>

typedef std::chrono::high_resolution_clock Clock;
decltype(Clock::now()) tResume;

template <typename... Args>
struct std::coroutine_traits<void, Args...> {
    struct promise_type {
        void get_return_object() {}
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

/* The return value of the coroutine is the Awaitable.
 * The compiler tries to obtain the awaiter from Awaibable::operator co_await
 * and global co_await(Awaitable&) operator.
 * If it fails, the awaiter is the same object as the returned awaitable (as is in this case)
 */
auto setTimeout(uv_loop_t* loop, uint32_t msTime)
{
    struct Awaiter
    {
        uv_loop_t* mLoop;
        uint32_t mMsTime;
        std::coroutine_handle<> mCoro;

        bool await_ready()
        {
            printf("await_ready called, returning false\n");
            return false;
        }
        auto await_resume()
        {
            printf("await_resume called\n");
        }
        void await_suspend(std::coroutine_handle<> coro)
        {
            printf("await_suspend called, waiting for %u ms...\n", mMsTime);
            mCoro = coro;
            auto timer = new uv_timer_t;
            int r = uv_timer_init(mLoop, timer);
            assert(r == 0);
            timer->data = this;
            assert(!uv_is_active((uv_handle_t *) timer));
            r = uv_timer_start(timer, [](uv_timer_t* t)
            {
                auto self = static_cast<Awaiter*>(t->data);
                assert(self);
                uv_timer_stop(t);
                uv_close((uv_handle_t*)t, [](uv_handle_t* handle)
                {
                    delete reinterpret_cast<uv_timer_t*>(handle);
                });
                printf("timer expired, calling coroutine.resume()\n");
                tResume = Clock::now();
                //printf("immediately elapsed: %ld ns\n", std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - tResume).count());
                self->mCoro.resume();
            }, mMsTime, 0);
        }
        ~Awaiter() { printf("Awaiter destroyed\n"); }
    };
    return Awaiter{loop, msTime};
}

uint64_t msNow()
{
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void startupFunc(uv_async_t* handle)
{
    for (int i = 0; ; i++)
    {
        auto tsStart = msNow();
        printf("Calling co_await...\n");
        co_await setTimeout(uv_default_loop(), i*10);
        printf("elapsed: %d, overhead: %ld us\n\n", (int)(msNow() - tsStart),
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - tResume).count());
    }
}

int main(int argc, char *argv[])
{
    uv_async_t startup;
    uv_async_init(uv_default_loop(), &startup, &startupFunc);
    uv_async_send(&startup);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}
