/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "rtimage.h"
#include "svgpaintable.h"

#include <giomm/listmodel.h>
#include <glibmm/object.h>
#include <glibmm/property.h>
#include <glibmm/refptr.h>
#include <gtkmm/box.h>

#include <algorithm>
#include <vector>

template <class T>
class RtTreeListNode;

/**
 * Warning!
 * Glib::RefPtr<RtTreeListNode<T>> is not necessarily a unique shared_ptr to
 * the underlying GObject. That means you should be careful using weak_ptr and
 * must compare pointers using the raw pointer.
 *
 * You should only create a weak_ptr from the Glib::RefPtr returned by
 * add_node() and not the ones queried by ListItem/ListItemFactory.
 *
 * See get_item_vfunc() implementation.
 */
template <class T>
class RtTreeListModel final
    // Ordering is important for proper GObject initialization
    : public Gio::ListModel, public Glib::Object {
public:
    static_assert(std::is_base_of_v<Glib::Object, T>);

    using Node = RtTreeListNode<T>;
    using CompareFunc = std::function<bool(const Glib::RefPtr<Node>&, const Glib::RefPtr<Node>&)>;

    static Glib::RefPtr<RtTreeListModel<T>> create() {
        return Glib::make_refptr_for_instance<RtTreeListModel<T>>(new RtTreeListModel<T>());
    }

    RtTreeListModel(const RtTreeListModel&) = delete;
    RtTreeListModel& operator=(const RtTreeListModel&) = delete;
    RtTreeListModel(RtTreeListModel&&) = delete;
    RtTreeListModel& operator=(RtTreeListModel&&) = delete;

    ~RtTreeListModel() = default;

    /**
     * @param data The data contained in the new child tree node.
     * @param parent The parent tree node or nullptr for the root node.
     */
    Glib::RefPtr<Node> add_node(const Glib::RefPtr<T>& data, Node* parent);
    void remove_node(const Glib::RefPtr<Node>& node);
    bool owns_node(const Glib::RefPtr<Node>& node) const;

    Node* find_node(guint position) const;
    std::optional<guint> find_pos(Node* node) const;

    void set_sorter(const CompareFunc& compare);

protected:
    GType get_item_type_vfunc() override { return Node::get_base_type(); }
    guint get_n_items_vfunc() override { return m_list_size; }
    gpointer get_item_vfunc(guint position) override;

private:
    RtTreeListModel();

    void add_child_for_parent(Node* parent, const Glib::RefPtr<Node>& child);
    void on_expand(Node* node);
    void rebuild_cache() const;
    void notify_items_changed(guint pos, guint removed, guint added);

    template <bool INCREASE>
    void update_parent_visibility(Node* node, guint diff);

    CompareFunc m_sorter;
    Glib::RefPtr<Node> m_root;
    size_t m_tree_size;
    guint m_list_size;

    // Cached data that may be modified internally
    mutable std::vector<Node*> m_list_cache;
};

/**
 * Warning!
 * Glib::RefPtr<RtTreeListNode<T>> is not necessarily a unique shared_ptr to
 * the underlying GObject. That means you should be careful using weak_ptr and
 * must compare pointers using the raw pointer. See RtTreeListModel<T>.
 */
template <class T>
class RtTreeListNode final : public Glib::Object {
public:
    static_assert(std::is_base_of_v<Glib::Object, T>);

    Glib::RefPtr<T> data() const { return m_data; }
    RtTreeListNode* parent() const { return m_parent; }
    const std::vector<Glib::RefPtr<RtTreeListNode>>& children() const { return m_children; }
    size_t num_children() const { return m_children.size(); }
    size_t depth() const { return m_depth; }
    bool is_expanded() const { return m_prop_expanded.get_value(); }

    std::vector<Glib::RefPtr<RtTreeListNode<T>>> collect_descendants() const;
    size_t count_descendants() const;

    void toggle_expanded();

    Glib::PropertyProxy<bool> property_expanded() { return m_prop_expanded.get_proxy(); };
    Glib::PropertyProxy_ReadOnly<bool> property_expanded() const {
        return m_prop_expanded.get_proxy();
    };

private:
    friend class RtTreeListModel<T>;

    static Glib::RefPtr<RtTreeListNode<T>>
    create(const Glib::RefPtr<T>& data, RtTreeListNode* parent, size_t depth) {
        return Glib::make_refptr_for_instance<RtTreeListNode<T>>(
            new RtTreeListNode<T>(data, parent, depth));
    }

    RtTreeListNode(const Glib::RefPtr<T>& data, RtTreeListNode* parent, size_t depth);

    Glib::Property<bool> m_prop_expanded;

    std::vector<Glib::RefPtr<RtTreeListNode>> m_children;
    Glib::RefPtr<T> m_data;
    RtTreeListNode* m_parent;
    size_t m_depth;
    // The number of descendants that are visible when this node is expanded.
    size_t m_num_visible_descendants;
    bool m_is_visible;
};

template <class T>
class RtTreeListExpander final : public Gtk::Box {
public:
    static_assert(std::is_base_of_v<Glib::Object, T>);

    using Node = RtTreeListNode<T>;

    RtTreeListExpander();
    ~RtTreeListExpander();

    Gtk::Widget* get_child();
    void set_child(Gtk::Widget& child);

    Glib::RefPtr<Node> get_node() const { return m_node; }
    void set_node(const Glib::RefPtr<Node>& node);

private:
    void update_expander();

    Gtk::Box m_indent_box;
    Gtk::Box m_child_box;
    RtImage m_expander;
    Glib::RefPtr<Node> m_node;
    Glib::RefPtr<Gtk::GestureClick> m_click;
    sigc::connection m_node_expanded_conn;
    size_t m_depth;
};

// Private --------------------------------------------------------------------

template <class T>
RtTreeListModel<T>::RtTreeListModel()
    : Glib::ObjectBase(typeid(RtTreeListModel<T>)),
      Gio::ListModel(),
      m_root(Node::create(nullptr, nullptr, 0)),
      m_tree_size(0),
      m_list_size(0)
{
    m_root->m_is_visible = true;
    m_root->m_prop_expanded.set_value(true);
}

template <class T>
auto RtTreeListModel<T>::add_node(const Glib::RefPtr<T>& data, Node* parent) -> Glib::RefPtr<Node>
{
    Glib::RefPtr<Node> node;
    if (parent) {
        node = Node::create(data, parent, parent->depth() + 1);
        add_child_for_parent(parent, node);
    } else {
        node = Node::create(data, m_root.get(), 0);
        add_child_for_parent(m_root.get(), node);
    }

    return node;
}

template <class T>
void RtTreeListModel<T>::add_child_for_parent(Node* parent, const Glib::RefPtr<Node>& child)
{
    // This slot needs to be first for proper interaction with adding entries
    // when expanded.
    child->property_expanded().signal_changed().connect(
        sigc::bind(sigc::mem_fun(*this, &RtTreeListModel<T>::on_expand), child.get()));

    child->m_is_visible = true;

    m_tree_size += 1;
    if (m_sorter) {
        auto& children = parent->m_children;
        children.insert(
            std::upper_bound(children.begin(), children.end(), child, m_sorter),
            child);
    } else {
        parent->m_children.push_back(child);
    }

    if (!child->m_is_visible) return;

    guint old_size = get_n_items();

    size_t num_added = child->m_num_visible_descendants + 1;
    update_parent_visibility<true>(child.get(), num_added);

    m_list_size = m_root->m_num_visible_descendants;
    guint new_size = get_n_items();
    notify_items_changed(0, old_size, new_size);
}

template <class T>
void RtTreeListModel<T>::remove_node(const Glib::RefPtr<Node>& node)
{
    if (!node) return;

    if (!owns_node(node)) {
        throw std::invalid_argument("Attempting to remove node not owned by model");
    }

    const std::optional<guint> found_pos = find_pos(node.get());

    m_tree_size -= node->count_descendants() + 1;
    auto& children = node->m_parent->m_children;
    // See RtTreeListModel for explanation on why we must use remove_if here.
    children.erase(
        std::remove_if(children.begin(), children.end(),
                       [&](const auto& child) { return child.get() == node.get(); }),
        children.end());

    if (!node->m_is_visible) {
        node->m_parent = nullptr;
        return;
    }

    size_t removed = node->m_num_visible_descendants + 1;
    update_parent_visibility<false>(node.get(), removed);
    node->m_parent = nullptr;

    if (found_pos) {
        notify_items_changed(*found_pos, removed, 0);
    }
}

template <class T>
bool RtTreeListModel<T>::owns_node(const Glib::RefPtr<Node>& node) const
{
    Node* curr = node.get();
    while (curr->m_parent) {
        curr = curr->m_parent;
    }
    return curr == m_root.get();
}

template <class T>
void RtTreeListModel<T>::on_expand(Node* node) {
    if (!node->m_parent || !node->m_is_visible) return;

    std::optional<guint> found = find_pos(node);
    if (!found) return;

    // Updating list membership of children and not the current node
    guint pos = *found + 1;

    if (node->is_expanded()) {
        guint before = node->m_num_visible_descendants;
        for (auto& child : node->m_children) {
            if (!child->m_is_visible) {
                child->m_is_visible = true;
                node->m_num_visible_descendants += child->m_num_visible_descendants + 1;
            }
        }
        guint diff = node->m_num_visible_descendants - before;
        update_parent_visibility<true>(node, diff);
        notify_items_changed(pos, 0, diff);
    } else {
        guint before = node->m_num_visible_descendants;
        for (auto& child : node->m_children) {
            if (child->m_is_visible) {
                child->m_is_visible = false;
                node->m_num_visible_descendants -= child->m_num_visible_descendants + 1;
            }
        }
        guint diff = before - node->m_num_visible_descendants;
        update_parent_visibility<false>(node, diff);
        notify_items_changed(pos, diff, 0);
    }
}

template <class T>
template <bool INCREASE>
void RtTreeListModel<T>::update_parent_visibility(Node* node, guint diff)
{
    if (diff == 0) return;

    Node* curr = node->m_parent;
    while (curr && curr->m_is_visible) {
        if constexpr (INCREASE) {
            curr->m_num_visible_descendants += diff;
        } else {
            curr->m_num_visible_descendants -= diff;
        }
        curr = curr->m_parent;
    }

    if (curr && !curr->m_is_visible) {
        if constexpr (INCREASE) {
            curr->m_num_visible_descendants += diff;
        } else {
            curr->m_num_visible_descendants -= diff;
        }
    }
}

template <class T>
gpointer RtTreeListModel<T>::get_item_vfunc(guint position)
{
    auto node = find_node(position);
    // Q: Why use gobj_copy() here instead of just gobj()?
    //
    // The C API reference for Gio.ListModel says about the returned pointer:
    // > The caller of the method takes ownership of the returned data, and is
    // > responsible for freeing it.
    // Hence, we need to reference() the pointer which is done by gobj_copy().
    //
    // glibmm wraps the function get_object(guint) -> Glib::RefPtr<>
    // The implementation calls Glib::make_refptr_for_instance() on the
    // returned pointer. However, we already have a Glib::RefPtr pointing to
    // the node. There are now two shared_ptrs pointing to the same memory with
    // different ref-counts. Glib::ObjectBase::unreference() will be called two
    // times causing a double-free/segfault on the second unreference().
    //
    // By using gobj_copy(), we reference() the pointer and offset the
    // unreference() in the custom destructor of Glib::RefPtr.
    return node ? node->gobj_copy() : nullptr;
}

template <class T>
auto RtTreeListModel<T>::find_node(guint position) const -> Node*
{
    if (position >= get_n_items()) return nullptr;
    if (m_list_cache.empty()) {
        rebuild_cache();
    }
    return m_list_cache.at(position);
}

template <class T>
std::optional<guint> RtTreeListModel<T>::find_pos(Node* node) const
{
    if (m_list_cache.empty()) {
        rebuild_cache();
    }

    for (guint i = 0; i < m_list_cache.size(); i++) {
        if (m_list_cache[i] == node) return i;
    }

    return std::nullopt;
}

template <class T>
void RtTreeListModel<T>::rebuild_cache() const
{
    printf("rebuild cache\n");
    m_list_cache.reserve(m_tree_size);

    auto dfs = [&]() {
        auto func = [&](const auto& self, const auto& children) -> void {
            for (const auto& child : children) {
                if (child->m_is_visible) {
                    m_list_cache.push_back(child.get());
                    self(self, child->m_children);
                }
            }
        };

        // Make sure m_root does NOT get put in list
        func(func, m_root->m_children);
    };

    dfs();
}

template <class T>
void RtTreeListModel<T>::set_sorter(const CompareFunc& compare)
{
    m_sorter = compare;

    auto dfs = [&]() {
        auto func = [&](const auto& self, auto& children) -> void {
            std::sort(children.begin(), children.end(), m_sorter);
            for (const auto& child : children) {
                self(self, child->m_children);
            }
        };

        // Make sure m_root does NOT get put in list
        func(func, m_root->m_children);
    };

    dfs();

    guint size = get_n_items();
    notify_items_changed(0, size, size);
}

template <class T>
void RtTreeListModel<T>::notify_items_changed(guint pos, guint removed, guint added)
{
    m_list_cache.clear();
    m_list_size = m_root->m_num_visible_descendants;
    printf("items changed %d -%d +%d\n", pos, removed, added);
    items_changed(pos, removed, added);
}

template <class T>
RtTreeListNode<T>::RtTreeListNode(const Glib::RefPtr<T>& data, RtTreeListNode* parent, size_t depth)
    : Glib::ObjectBase(typeid(RtTreeListNode<T>)),
      m_prop_expanded(*this, "expanded", false),
      m_data(data),
      m_parent(parent),
      m_depth(depth),
      m_num_visible_descendants(0),
      m_is_visible(true)
{}

template <class T>
std::vector<Glib::RefPtr<RtTreeListNode<T>>> RtTreeListNode<T>::collect_descendants() const
{
    std::vector<Glib::RefPtr<RtTreeListNode<T>>> result;

    auto dfs = [&]() {
        auto func = [&](const auto& self, const auto& children) -> void {
            for (const auto& child : children) {
                result.push_back(child);
                self(self, child->m_children);
            }
        };

        func(func, m_children);
    };

    dfs();
    return result;
}

template <class T>
size_t RtTreeListNode<T>::count_descendants() const
{
    size_t size = 0;

    auto dfs = [&]() {
        auto func = [&](const auto& self, const auto& children) -> void {
            for (const auto& child : children) {
                size++;
                self(self, child->m_children);
            }
        };

        func(func, m_children);
    };

    dfs();
    return size;
}

template <class T>
void RtTreeListNode<T>::toggle_expanded()
{
    bool curr_state = m_prop_expanded.get_value();
    m_prop_expanded.set_value(!curr_state);
}

template <class T>
RtTreeListExpander<T>::RtTreeListExpander() : m_depth(0)
{
    append(m_indent_box);
    m_indent_box.append(m_expander);

    m_child_box.set_halign(Gtk::Align::START);
    m_child_box.set_hexpand(true);
    append(m_child_box);

    m_click = Gtk::GestureClick::create();
    m_click->set_button(GDK_BUTTON_PRIMARY);
    m_click->signal_released().connect([&](int n_press, double x, double y) {
        if (m_node) m_node->toggle_expanded();
        m_click->set_state(Gtk::EventSequenceState::CLAIMED);
    });
    m_expander.add_controller(m_click);

    signal_destroy().connect([&]() {
        if (m_expander.get_parent()) {
            m_indent_box.remove(m_expander);
        }
        if (m_child_box.get_parent()) {
            remove(m_indent_box);
            remove(m_child_box);
        }
    });
}

template <class T>
RtTreeListExpander<T>::~RtTreeListExpander()
{
    if (m_child_box.get_parent()) {
        if (m_expander.get_parent()) {
            m_indent_box.remove(m_expander);
        }
        remove(m_indent_box);
        remove(m_child_box);
    }
}

template <class T>
Gtk::Widget* RtTreeListExpander<T>::get_child()
{
    return m_child_box.get_first_child();
}

template <class T>
void RtTreeListExpander<T>::set_child(Gtk::Widget& child)
{
    if (auto existing = m_child_box.get_first_child(); existing) {
        m_child_box.remove(*existing);
    }
    m_child_box.append(child);
}

template <class T>
void RtTreeListExpander<T>::set_node(const Glib::RefPtr<Node>& node)
{
    m_node_expanded_conn.disconnect();

    m_node = node;
    if (!m_node) {
        m_depth = 0;
        update_expander();
        return;
    }

    m_depth = m_node->depth();
    update_expander();
    m_node_expanded_conn = m_node->property_expanded().signal_changed().connect(
        sigc::mem_fun(*this, &RtTreeListExpander<T>::update_expander));
}

template <class T>
void RtTreeListExpander<T>::update_expander()
{
    {
        auto children = m_indent_box.get_children();
        for (Gtk::Widget* widget : children) {
            m_indent_box.remove(*widget);
        }
    }

    for (size_t i = 0; i < m_depth; i++) {
        m_indent_box.append(*Gtk::make_managed<Gtk::Image>());
    }

    m_indent_box.append(m_expander);

    Glib::RefPtr<Gdk::Paintable> empty = nullptr;
    static auto collapsed_svg = SvgPaintableWrapper::createFromIcon("expander-closed-small");
    static auto expanded_svg = SvgPaintableWrapper::createFromIcon("expander-open-small");

    if (!m_node) {
        m_expander.set(empty);
    } else if (m_node->is_expanded()) {
        expanded_svg->setOnImage(&m_expander);
    } else {
        collapsed_svg->setOnImage(&m_expander);
    }
}
