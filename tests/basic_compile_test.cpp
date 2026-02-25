#include "seraph/queue.hpp"
#include "seraph/stack.hpp"

int main() {
    seraph::Stack<int> stack;
    stack.push(1);
    stack.push(2);

    if (stack.top() != 2) {
        return 1;
    }

    if (stack.pop() != 2) {
        return 1;
    }

    seraph::Stack<int> adaptive_stack;
    adaptive_stack.push(10);
    adaptive_stack.emplace(20);

    if (adaptive_stack.top() != 20) {
        return 1;
    }

    if (adaptive_stack.pop() != 20) {
        return 1;
    }

    seraph::Queue<int> queue;
    if (!queue.empty()) {
        return 1;
    }

    queue.push(1);
    queue.emplace(2);

    if (queue.front() != 1) {
        return 1;
    }

    if (queue.back() != 2) {
        return 1;
    }

    if (queue.size() != 2) {
        return 1;
    }

    if (queue.pop() != 1) {
        return 1;
    }

    if (queue.pop() != 2) {
        return 1;
    }

    if (!queue.empty()) {
        return 1;
    }

    if (queue.pop().has_value()) {
        return 1;
    }

    // TODO: Replace this smoke test with structured unit tests GoogleTest
    return 0;
}
