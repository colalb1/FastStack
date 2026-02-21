#include "seraph/queue.hpp"
#include "seraph/stack.hpp"

int main()
{
    seraph::Stack stack;
    stack.push(1);
    stack.push(2);

    if (stack.top() != 2)
    {
        return 1;
    }

    if (stack.pop() != 2)
    {
        return 1;
    }

    seraph::Queue queue;
    queue.enqueue(10);
    queue.enqueue(20);

    if (queue.front() != 10)
    {
        return 1;
    }

    if (queue.dequeue() != 10)
    {
        return 1;
    }

    // TODO(colin): Replace this smoke test with structured unit tests (e.g., GoogleTest or Catch2).
    return 0;
}
