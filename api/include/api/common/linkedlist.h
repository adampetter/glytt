#pragma once
#include <cstdio>

template <typename T>
class LinkedListNode {
public:
    T data;
    LinkedListNode* next;

    LinkedListNode(const T& value) : data(value), next(nullptr) {}
};

template <typename T>
class LinkedListIterator {
private:
    LinkedListNode<T>* current;

public:
    LinkedListIterator(LinkedListNode<T>* node) : current(node) {}

    T& operator*() const {
        if(!current)
            throw "Empty iterator exception!";

        return current->data;
    }

    LinkedListIterator<T>& operator++() {
        if (current != nullptr) {
            current = current->next;
        }
        return *this;
    }

    bool operator!=(const LinkedListIterator<T>& other) const {
        printf("Current: %p != %p\n", current, other.current);
        return current != other.current;
    }

    bool operator==(const LinkedListIterator<T>& other) const {
        return current == other.current;
    }

    bool Valid(){
        return current != nullptr;
    }
};

template <typename T>
class LinkedList {
protected:
    LinkedListNode<T>* head;
    int count;

public:
    LinkedList() : head(nullptr), count(0) {}

    ~LinkedList() {
        Clear();
    }

    void Add(const T& item) {
        LinkedListNode<T>* newNode = new LinkedListNode<T>(item);

        if (head == nullptr) {
            head = newNode;
        } else {
            LinkedListNode<T>* current = head;
            while (current->next != nullptr) {
                current = current->next;
            }
            current->next = newNode;
        }

        count++;
    }

    void RemoveAt(int index) {
        if (index < 0 || index >= count) {
            return;
        }

        if (index == 0) {
            LinkedListNode<T>* nodeToDelete = head;
            head = head->next;
            delete nodeToDelete;
        } else {
            LinkedListNode<T>* previous = nullptr;
            LinkedListNode<T>* current = head;
            int currentIndex = 0;

            while (current != nullptr && currentIndex < index) {
                previous = current;
                current = current->next;
                currentIndex++;
            }

            if (current != nullptr) {
                previous->next = current->next;
                delete current;
            }
        }

        count--;
    }

    void Remove(const T& value) {
        LinkedListNode<T>* previous = nullptr;
        LinkedListNode<T>* current = head;

        while (current != nullptr) {
            if (current->data == value) {
                if (previous == nullptr) {
                    head = current->next;
                } else {
                    previous->next = current->next;
                }

                LinkedListNode<T>* nodeToDelete = current;
                current = current->next;
                delete nodeToDelete;

                count--;
            } else {
                previous = current;
                current = current->next;
            }
        }
    }

    int Count() const {
        return count;
    }

    void Clear() {
        LinkedListNode<T>* current = head;

        while (current != nullptr) {
            LinkedListNode<T>* next = current->next;
            delete current;
            current = next;
        }

        head = nullptr;
        count = 0;
    }

    T At(int index) const {
        if (index < 0 || index >= count) {
            throw "Index out of range";
        }

        LinkedListNode<T>* current = head;
        int currentIndex = 0;

        while (current != nullptr && currentIndex < index) {
            current = current->next;
            currentIndex++;
        }

        return current->data;
    }

    T* Ptr(int index = 0) const {
        if (index < 0 || index >= count) {
            return nullptr;
        }

        LinkedListNode<T>* current = head;
        int currentIndex = 0;

        while (current != nullptr && currentIndex < index) {
            current = current->next;
            currentIndex++;
        }

        return &(current->data);
    }

    T& operator[](int index) {
        if (index < 0 || index >= count) {
            throw "Index out of range";
        }

        LinkedListNode<T>* current = head;
        int currentIndex = 0;

        while (current != nullptr && currentIndex < index) {
            current = current->next;
            currentIndex++;
        }

        return current->data;
    }

    LinkedListIterator<T> Begin() const {
        return LinkedListIterator<T>(head);
    }

    LinkedListIterator<T> End() const {
        return LinkedListIterator<T>(nullptr);
    }
};
