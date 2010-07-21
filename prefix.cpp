// Simple prefix tree to represent sets of missing tiles
#include <cassert>
#include <cstring>
#include "prefix.h"
struct pt_node {
  struct pt_node *children[4];
};

#define PT_LEAF ((struct pt_node *)0x1)


PrefixTree::PrefixTree()
{
  root = NULL;
}

bool PrefixTree::containsPrefix(qkey q) const
{
  struct pt_node *x = root;
  while (q > 1) {
    if (x == PT_LEAF) return true;
    if (!x) return false;
    x = x->children[q & 3];
    q >>= 2;
  }
  return x == PT_LEAF;
}

void PrefixTree::add(qkey q) {
  struct pt_node **x = &root;
  while (q > 1) {
    struct pt_node *y = *x;
    assert(y != PT_LEAF);
    if (y == NULL) {
      *x = y = new pt_node;
      std::memset(y, 0, sizeof(struct pt_node));
    }
    x = &y->children[q & 3];
    q >>= 2;
  }
  *x = PT_LEAF;
}
