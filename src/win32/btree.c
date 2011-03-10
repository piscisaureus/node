#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

typedef struct type type;

# define noinline                   __attribute__ ((noinline))

struct type {
  type *left;
  type *right;
  int height;
  int val;
};

type *tree = 0;

/*inline int compare(type *a, type *b) {
  return a->val - b->val;
}

  inline int avl_max(int a, int b) {
    return a > b ? a : b;
  }

  inline int avl_height(type *node) {
    return node ? node->height : -1;
  }
*/
#define inline static

#define compare(a, b) ((a)->val - (b)->val)
#define avl_max(a, b) ((a) > (b) ? (a) : (b))
//#define avl_height(node) ((node) ? (node)->height : -1)

  inline int avl_height(type *node) {
    return node ? node->height : -1;
  }

  inline type *avl_update_height(type *node) {
    int h1 = node->left ? node->left->height + 1 : 0;
    int h2 = node->right ? node->right->height + 1 : 0;
    node->height = h1 > h2 ? h1 : h2;
  }

  inline type *avl_rot_left(type *parent) {
    type *child = parent->left;
    parent->left = child->right;
    child->right = parent;
    return child;
  }

  inline type *avl_rot_right(type *parent) {
    type *child = parent->right;
    parent->right = child->left;
    child->left = parent;
    return child;
  }

  inline int avl_balance(type *node) {
    return avl_height(node->left) - avl_height(node->right);
  }

  static noinline type *avl_rebalance(type *parent) {
    int balance = avl_balance(parent);
    if (balance > 1) {
      // Left branch is too high
      if (avl_balance(parent->left) < 0) {
        // Need to rotate the right subtree first
        type *child = parent->left;
        child->height -= 1;
        child->right->height += 1;
        parent->left = avl_rot_right(child);
      }
      // Left rotation
      parent->height -= 2;
      return avl_rot_left(parent);
    } else if (balance < -1) {
      // Right branch is too high
      if (avl_balance(parent->right) > 0) {
        // Need to rotate the right subtree first
        type *child = parent->right;
        child->height -= 1;
        child->left->height += 1;
        parent->right = avl_rot_left(child);
      }
      // Right rotation
      parent->height -= 2;
      return avl_rot_right(parent);
    }
    return parent;
  }

  static noinline type *avl_insert(type *parent, type *node) {
    if (parent) {
      if (compare(parent, node) < 0) {
        parent->left = avl_insert(parent->left, node);
      } else {
        parent->right = avl_insert(parent->right, node);
      }
      avl_update_height(parent);
      return avl_rebalance(parent);
    } else {
      node->height = 0;
      return node;
    }
  }

  inline void avl_print(type *tree, int indent) {
    if (!tree) return;
    avl_print(tree->left, indent + 1);
    int j;
    for (j = 0; j < indent; j++) fprintf(stderr, " ");
    fprintf(stderr, "val=%d h=%d\n", tree->val, tree->height);
    avl_print(tree->right, indent + 1);
  }

  inline void avl_insert_random(type **tree, type *node) {
    node->val = rand();
    *tree = avl_insert(*tree, node);
  }

  int main() {
    int cnt = 1000000;
    type *nodes = (type*)malloc(cnt * sizeof(type));
    memset(nodes, 0, sizeof(type) * cnt);
    int i;
    for (i = 0; i < cnt; i++) {
      avl_insert_random(&tree, nodes + i);
    }
    //avl_print(tree, 0);
    fprintf(stderr, " -- %d\n", i);
    return 0;
  }

