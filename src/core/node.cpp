#include "node.h"

Node::~Node() {
    _propagate_exit_tree();
}

void Node::add_child(std::shared_ptr<Node> child) {
    if (child->_parent) {
        child->_parent->remove_child(child.get());
    }
    child->_parent = this;
    _children.push_back(child);
    if (_inside_tree) {
        child->_set_tree(_tree);
        child->_propagate_enter_tree();
        child->_propagate_ready();
    }
}

void Node::remove_child(Node* child) {
    auto it = std::find_if(_children.begin(), _children.end(),
        [child](const auto& p) { return p.get() == child; });
    if (it != _children.end()) {
        (*it)->_propagate_exit_tree();
        (*it)->_parent = nullptr;
        (*it)->_tree = nullptr;
        _children.erase(it);
    }
}

void Node::queue_free() {
    _queued_free = true;
}

void Node::_set_tree(SceneTree* tree) {
    _tree = tree;
    for (auto& child : _children) {
        child->_set_tree(tree);
    }
}

void Node::_propagate_enter_tree() {
    if (_inside_tree) return;
    _inside_tree = true;
    _enter_tree();
    for (auto& child : _children) {
        child->_propagate_enter_tree();
    }
}

void Node::_propagate_ready() {
    _ready();
    for (auto& child : _children) {
        child->_propagate_ready();
    }
}

void Node::_propagate_exit_tree() {
    if (!_inside_tree) return;
    _exit_tree();
    _inside_tree = false;
    for (auto& child : _children) {
        child->_propagate_exit_tree();
    }
}

void Node::_propagate_process(double delta) {
    if (!_processing) return;
    _process(delta);
    for (auto& child : _children) {
        child->_propagate_process(delta);
    }
}
