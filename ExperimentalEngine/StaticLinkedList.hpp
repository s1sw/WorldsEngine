#pragma once

namespace worlds {
    // Static linked list designed for adding to in static constructors.
    template <typename T>
    class StaticLinkedList {
    private:
        struct Node {
            Node() : ptr {nullptr}, next {nullptr} {}
            T* ptr;
            Node* next;
        };
    public:
        void add(T* val) {
            auto* newNode = new Node;
            newNode->ptr = val;
            newNode->next = first;
            first = newNode;
        }

        Node* first = nullptr;
    };
}
