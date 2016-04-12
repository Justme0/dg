#ifndef _CONTROL_EXPRESSION_H_
#define _CONTROL_EXPRESSION_H_

#include <cassert>
#include <vector>

#include "ADT/DGContainer.h"

namespace dg {

enum ControlExpressionType {
    PLUS = 1,
    STAR,
    SYMBOLS,
    // sequence means mix of PLUS, START and SYMBOL
    SEQUENCE
};

template <typename LabelT>
class ControlExpression
{
    ControlExpressionType type;
    union ElementType {
        ElementType(LabelT l): label(l) {}
        ElementType(ControlExpression<LabelT> *sub) : subexpression(sub) {}

        LabelT label;
        ControlExpression<LabelT> *subexpression;
    };

    std::vector<ElementType> elements;

public:
    ControlExpression<LabelT>(ControlExpressionType t = SEQUENCE,
                              ControlExpression<LabelT> *subexp = nullptr)
    : type(t)
    {
        if (subexp)
            addElement(subexp);
            //elements.push_back(subexp);
    }

    void addElement(ControlExpression<LabelT> *subexp)
    {
        elements.push_back(ElementType(subexp));
    }

    void addElement(LabelT label)
    {
        elements.push_back(ElementType(label));
    }

    void addElement(ElementType elem)
    {
        elements.push_back(elem);
    }

    std::vector<ElementType> getElements() { return elements; }
    ControlExpressionType getType() const { return type; }
};

template <typename LabelT>
class ControlExpressionNode
{
    LabelT label;

public:
    ControlExpressionNode<LabelT>(LabelT& lab)
        : label(lab) {}

    ControlExpressionNode<LabelT>(LabelT lab)
        : label(lab) {}

    struct Edge {
        Edge(ControlExpressionNode<LabelT> *t,
             ControlExpressionNode<LabelT> *elem = nullptr)
            : target(t)
            {
                if (elem)
                    expression.addElement(elem->label);
            }

        bool operator<(const Edge& e) const
        {
            return target < e.target;
        }

        // the target of the edge
        ControlExpressionNode *target;
        // the expression above the edge
        ControlExpression<LabelT> expression;
    };

    LabelT& getLabel() const { return label; }

    typedef DGContainer<ControlExpressionNode *> PredContainerT;
    typedef DGContainer<Edge> SuccContainerT;

    SuccContainerT& getSuccessors() { return successors; }
    const SuccContainerT& getSuccessors() const { return successors; }

    PredContainerT& getPredecessors() { return predecessors; }
    const PredContainerT& getPredecessors() const { return predecessors; }

    size_t successorsNum() const { return successors.size(); }
    size_t predecessorsNum() const { return predecessors.size(); }

    void addSuccessor(ControlExpressionNode<LabelT> *target)
    {
        // create an edge to 'target' with label 'target'
        successors.insert(Edge(target, target));
        target->predecessors.insert(this);
    }

    // remove all edges from/to this BB and reconnect them to
    // other nodes
    void eliminateState()
    {
        delete this;
    }

#if 0
    // return true if all successors point
    // to the same basic block (not considering labels,
    // just the targets)
    bool successorsAreSame() const
    {
        if (successors.size() < 2)
            return true;

        typename SuccContainerT::const_iterator start, iter, end;
        iter = successors.begin();
        end = successors.end();

        BBlock<LabelT> *block = iter->target;
        // iterate over all successor and
        // check if they are all the same
        for (++iter; iter != end; ++iter)
            if (iter->target != block)
                return false;

        return true;
    }

    void remove(bool with_nodes = true)
    {
        // do not leave any dangling reference
        isolate();

        if (dg)
            dg->removeBlock(key);

        // XXX what to do when this is entry block?

        if (with_nodes) {
            for (LabelT *n : nodes) {
                // we must set basic block to nullptr
                // otherwise the node will try to remove the
                // basic block again if it is of size 1
                n->setBasicBlock(nullptr);

                // remove dependency edges, let be CFG edges
                // as we'll destroy all the nodes
                n->removeCDs();
                n->removeDDs();
                // remove the node from dg
                n->removeFromDG();

                delete n;
            }
        }

        delete this;
    }

    void removeSuccessors()
    {
        // remove references to this node from successors
        for (const Edge& succ : successors) {
            // This assertion does not hold anymore, since if we have
            // two edges with different labels to the same successor,
            // and we remove the successor, then we remove 'this'
            // from predecessors twice. If we'll add labels even to predecessors,
            // this assertion must hold again
            // bool ret = succ.target->predecessors.erase(this);
            // assert(ret && "Did not have this BB in successor's pred");
            succ.target->predecessors.erase(this);
        }

        successors.clear();
    }

    void removeSuccessor(const Edge& succ)
    {
        succ.target->predecessors.erase(this);
        successors.erase(succ);
    }

    void removePredecessors()
    {
        for (auto BB : predecessors) {
            BB->successors.erase(this);
        }

        predecessors.clear();
    }
#endif
private:
    SuccContainerT successors;
    PredContainerT predecessors;
};

} // namespace dg

#endif // _CONTROL_EXPRESSION_
