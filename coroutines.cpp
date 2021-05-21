#include <uv.h>
#include <stdlib.h>
#include <coroutine>
#include <iostream>
#include <memory>
#include <utility>
#include <chrono>
#include <assert.h>

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


auto setTimeout(uv_loop_t* loop, uint32_t msTime)
{
    struct Awaiter
    {
        uv_loop_t* mLoop;
        uint32_t mMsTime;
        std::coroutine_handle<> mCoro;

        bool await_ready()
        {
            printf("await_ready called\n");
            return false;
        }
        auto await_resume()
        {
            printf("await_resume called\n");
        }
        void await_suspend(std::coroutine_handle<> coro)
        {
            printf("await_suspend called\n");
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
                printf("calling coroutine.resume()\n");
                self->mCoro.resume();
            }, mMsTime, 0);
        }
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
        co_await setTimeout(uv_default_loop(), i*10);
        printf("elapsed[%d]: %d\n", i, (int)(msNow() - tsStart));
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
