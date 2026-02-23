#include "seraph/stack.hpp"

int main() {
    seraph::SpinlockStack<int> stack;
    stack.push(1);
    stack.push(2);

    if (stack.top() != 2) {
        return 1;
    }

    if (stack.pop() != 2) {
        return 1;
    }

    // TODO: Replace this smoke test with structured unit tests GoogleTest
    return 0;
}
