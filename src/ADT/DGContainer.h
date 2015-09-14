#ifndef _DG_CONTAINER_H_
#define _DG_CONTAINER_H_

#include <set>
#include <cassert>
#include <algorithm>

namespace dg {

/// ------------------------------------------------------------------
// - DGContainer
//
//   This is basically just a wrapper for real container, so that
//   we have the container defined on one place for all edges.
//   It may have more implementations depending on available features
/// ------------------------------------------------------------------
template <typename NodeT, unsigned int EXPECTED_ELEMENTS_NUM = 8>
class DGContainer
{
public:
    // XXX use llvm ADTs when available, or BDDs?
    typedef typename std::set<NodeT *> ContainerT;
    typedef typename ContainerT::iterator iterator;
    typedef typename ContainerT::const_iterator const_iterator;
    typedef typename ContainerT::size_type size_type;

    iterator begin() { return container.begin(); }
    const_iterator begin() const { return container.begin(); }
    iterator end() { return container.end(); }
    const_iterator end() const { return container.end(); }

    size_type size() const
    {
        return container.size();
    }

    bool insert(NodeT *n)
    {
        return container.insert(n).second;
    }

    bool contains(NodeT *n) const
    {
        return container.count(n) != 0;
    }

    size_t erase(NodeT *n)
    {
        return container.erase(n);
    }

    void clear()
    {
        container.clear();
    }

    bool empty()
    {
        return container.empty();
    }

    void swap(DGContainer<NodeT, EXPECTED_ELEMENTS_NUM>& oth)
    {
        container.swap(oth.container);
    }

    void intersect(const DGContainer<NodeT, EXPECTED_ELEMENTS_NUM>& oth)
    {
        DGContainer<NodeT, EXPECTED_ELEMENTS_NUM> tmp;

        std::set_intersection(container.begin(), container.end(),
                              oth.container.begin(),
                              oth.container.end(),
                              std::inserter(tmp.container,
                                            tmp.container.begin()));

        // swap containers
        container.swap(tmp.container);
    }

    bool operator==(const DGContainer<NodeT, EXPECTED_ELEMENTS_NUM>& oth) const
    {
        if (container.size() != oth.size())
            return false;

        // the sets are ordered, so this will work
        iterator snd = oth.container.begin();
        for (iterator fst = container.begin(), efst = container.end();
             fst != efst; ++fst, ++snd)
            if (*fst != *snd)
                return false;

        return true;
    }

    bool operator!=(const DGContainer<NodeT, EXPECTED_ELEMENTS_NUM>& oth) const
    {
        return !operator==(oth);
    }

private:
    ContainerT container;
};

template <typename NodeT, unsigned int EXPECTED_EDGES_NUM = 4>
class EdgesContainer : public DGContainer<NodeT, EXPECTED_EDGES_NUM>
{
};

template <typename NodeT>
class NodesContainer : public DGContainer<NodeT, 500>
{
};

} // namespace dg

#endif // _DG_CONTAINER_H_