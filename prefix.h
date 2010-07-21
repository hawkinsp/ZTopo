#ifndef PREFIX_H
#define PREFIX_H 1

struct pt_node;

typedef unsigned int qkey;

class PrefixTree {
public:
  PrefixTree();

  bool containsPrefix(qkey q) const;
  void add(qkey q);

private:
  struct pt_node *root;
};


#endif
