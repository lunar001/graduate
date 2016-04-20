#include <thread>
#include <iostream>
using namespace std;
 void func()
{
    cout <<"hwllo world\n";
    return;
}
        int
main()
{
    thread *p = new thread(bind(func));
    
    getchar();
    p->join();
    return 0;
}
