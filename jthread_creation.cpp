/*
Create BackgroundAggregator:

Member variables:
A std::jthread
A std::condition_variable_any and std::mutex (needed for interruptible waiting)
Any configuration state (int interval_ms, std::string worker_name).

Constructor:
Initializes the fields and starts the std::jthread.
The thread runs a private member function run(std::stop_token st).

The Worker Loop (run):
Uses cv.wait_for(...) with the stop_token.
If not stopped, prints [worker_name] Iteration: N.
If stopped, breaks out and exits immediately.

Destructor:

Rely on std::jthread's destructor (or trigger the condition variable if needed to guarantee immediate wake-up).

Give this implementation a shot in code. Focus on getting the member initialization and the loop condition right.
*/

#include <iostream>
#include <thread>
#include <condition_variable>

class BackgroundAggregator
{
    private:
        int interval_ms;
        std::string worker_name;
        int n = 0;

        std::condition_variable_any cva;
        std::mutex mtx;

        std::jthread jt;

        void run(std::stop_token st)
        {
            while(!st.stop_requested())
            {
                std::unique_lock<std::mutex> lock(mtx);
                bool stopped = cva.wait_for(lock,st,std::chrono::milliseconds(interval_ms),[&st](){return st.stop_requested();});
                if(stopped) break;
                std::cout<<worker_name<<++n<<std::endl;
            }
        }

    public:
        BackgroundAggregator(int interval_ms, std::string worker_name) : jt([this](std::stop_token st){this->run(st);})
        {
            this->interval_ms = interval_ms;
            this->worker_name = worker_name;
        }

        ~BackgroundAggregator()
        {
            cva.notify_all();
        }
};

int main()
{
    return 0;
}